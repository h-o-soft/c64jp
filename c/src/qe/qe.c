/* qe © 2019 David Given
 * This library is distributable under the terms of the 2-clause BSD license.
 * See COPYING.cpmish in the distribution root directory for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "lib/screen.h"
#ifdef QE_ENABLE_IME
#include "ime.h"
#endif
#ifdef ENABLE_FILE_IO
#include <cbm.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 64
#endif

#define EDITOR_BUFFER_SIZE (11 * 1024)   // 11KB using high RAM at $A000-$CBFF (avoid stack at $D000)

#define POKE(addr, val) (*(volatile uint8_t*)(addr) = (val))
#define PEEK(addr) (*(volatile uint8_t*)(addr))


uint8_t width, height;
uint8_t viewheight;
uint8_t status_line_length;
void (*print_status)(const char*);

static char current_filename[PATH_MAX];
static bool filename_set = false;
static char buffer[512];

// Use high RAM for editor buffer
static uint8_t* editor_buffer = (uint8_t*)0xA000;

uint8_t* buffer_start;
uint8_t* gap_start;
uint8_t* gap_end;
uint8_t* buffer_end;
uint8_t dirty;

uint8_t* first_line;   /* <= gap_start */
uint8_t* current_line; /* <= gap_start */
uint8_t current_line_y;
uint8_t
    display_height[64];   /* array of number of screen lines per logical line */
uint16_t line_length[64]; /* array of line length per logical line */

// Cursor display tracking
static uint8_t last_cursor_x = 0xFF;  // 0xFF = no previous cursor
static uint8_t last_cursor_y = 0xFF;
static bool cursor_displayed = false;

static void reset_cursor_display(void)
{
    last_cursor_x = 0xFF;
    last_cursor_y = 0xFF;
    cursor_displayed = false;
}

// 画面再描画時用：古いカーソル反転を元に戻してからリセット
static void clear_and_reset_cursor_display(void)
{
    // リセット前に古いカーソルの反転を元に戻す
    if (cursor_displayed && last_cursor_x != 0xFF) {
        screen_invert_cursor(last_cursor_x, last_cursor_y);
    }
    reset_cursor_display();
}

static void update_cursor_display(void)
{
    uint8_t current_x, current_y;
    screen_getcursor(&current_x, &current_y);

    if (current_x != last_cursor_x || current_y != last_cursor_y) {
        // Remove old cursor
        if (cursor_displayed && last_cursor_x != 0xFF) {
            screen_invert_cursor(last_cursor_x, last_cursor_y);
        }

        // Show new cursor
        screen_invert_cursor(current_x, current_y);
        cursor_displayed = true;
        last_cursor_x = current_x;
        last_cursor_y = current_y;
    }
}

uint16_t command_count;
typedef void command_t(uint16_t);

struct bindings
{
    const char* name;
    const char* keys;
    command_t* const* callbacks;
};

const struct bindings* bindings;

extern const struct bindings delete_bindings;
extern const struct bindings zed_bindings;
extern const struct bindings change_bindings;

extern void colon(uint16_t count);
extern void goto_line(uint16_t lineno);
void goto_status_line(void);

/* ======================================================================= */
/*                           SHIFT-JIS SUPPORT                             */
/* ======================================================================= */

// Shift-JIS第1バイト判定（インライン関数で最小化）
static inline bool is_sjis_lead(uint8_t c) {
    return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
}

// gap_startがShift-JISの2バイト目にいるかチェック
static bool is_at_sjis_second_byte(uint8_t* pos, uint8_t* line_start) {
    uint8_t* p = line_start;
    while (p < pos) {
        if (is_sjis_lead(*p)) {
            p++; // 第1バイト
            if (p == pos) return true;  // posは第2バイト
            if (p < pos) p++; // 第2バイトもスキップ
        } else {
            p++; // ASCII文字
        }
    }
    return false;
}

// 視覚列位置を簡単計算
static uint16_t count_visual_chars(uint8_t* start, uint8_t* end) {
    uint16_t count = 0;
    while (start < end) {
        if (is_sjis_lead(*start)) {
            start += 2;
        } else {
            start++;
        }
        count++;
    }
    return count;
}

/* ======================================================================= */
/*                                MISCELLANEOUS                            */
/* ======================================================================= */

void print_newline(void)
{
    goto_status_line();
    screen_clear_to_eol();
}

static void append_filename(const char* path)
{
    size_t len = strlen(buffer);
    if (len >= sizeof(buffer) - 1)
        return;

    size_t remaining = sizeof(buffer) - len - 1;
    const char* text = (path && *path) ? path : "(unnamed)";
    strncat(buffer, text, remaining);
}

static void my_strrev(char *str)
{
	size_t len = strlen(str);
	for (size_t i = 0, j = len - 1; i < j; i++, j--)
	{
		uint8_t a = str[i];
		str[i] = str[j];
		str[j] = a;
	}
}

void itoa(uint16_t val, char* buf)
{
    uint8_t i = 0;
    do
    {
        uint8_t digit = val % 10;
        buf[i++] = '0' + digit;
        val /= 10;
    } while (val);
    buf[i] = '\0';
    my_strrev(buf);
}

/* ======================================================================= */
/*                                SCREEN DRAWING                           */
/* ======================================================================= */

void screen_puti(uint16_t i)
{
    itoa(i, buffer);
    screen_putstring(buffer);
}

void goto_status_line(void)
{
    screen_setcursor(0, viewheight);
}

void set_status_line(const char* message)
{
    uint8_t screenx, screeny;
    screen_getcursor(&screenx, &screeny);

    uint8_t length = 0;
    goto_status_line();
	screen_setstyle(1);
    for (;;)
    {
        char c = *message++;
        if (!c)
            break;
        screen_putchar(c);
        length++;
    }
	screen_setstyle(0);
    while (length < status_line_length)
    {
        screen_putchar(' ');
        length++;
    }
    status_line_length = length;
    screen_setcursor(screenx, screeny);
}

/* ======================================================================= */
/*                              BUFFER MANAGEMENT                          */
/* ======================================================================= */

void new_file(void)
{
    gap_start = buffer_start;
    gap_end = buffer_end;

    first_line = current_line = buffer_start;
    dirty = true;
}

uint16_t compute_length(
    const uint8_t* inp, const uint8_t* endp, const uint8_t** nextp)
{
    uint8_t xo;
    uint8_t sjis_pending = 0;

    xo = 0;
    for (;;)
    {
        if (inp == endp)
            break;
        if (inp == gap_start)
            inp = gap_end;

        uint8_t c = *inp++;

        if (sjis_pending)
        {
            sjis_pending = 0;
            xo++;
            continue;
        }

        if (c == '\n')
            break;
        if (c == '\t')
            xo = (xo + 8) & ~7;
        else if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC))
            sjis_pending = 1;
        else if (c < 32)
            xo += 2;
        else
            xo++;
    }

    if (nextp)
        *nextp = inp;
    return xo;
}

uint8_t* draw_line(uint8_t* startp)
{
    uint8_t* inp = startp;

    uint8_t screenx, starty;
    screen_getcursor(&screenx, &starty);
    uint8_t x = screenx;
    uint8_t y = starty;
    for (;;)
    {
        if (y == viewheight)
            goto bottom_of_screen;

        if (inp == gap_start)
        {
            inp = gap_end;
            startp += (gap_end - gap_start);
        }
        if (inp == buffer_end)
        {
            if (x == 0)
                screen_putchar('~');
            break;
        }

        char c = *inp++;
        if (c == '\n')
            break;

        if (c == '\t')
        {
            uint8_t spaces = (uint8_t)((((uint16_t)x / 8) + 1) * 8 - x);
            if (spaces == 0)
                spaces = 8;
            for (uint8_t i = 0; i < spaces; ++i)
                screen_putchar(' ');
        }
        else
        {
            screen_putchar(c);
        }

        screen_getcursor(&x, &y);
    }

    if (x != width)
        screen_clear_to_eol();
    screen_setcursor(0, y+1);

bottom_of_screen:
    display_height[starty] = y - starty + 1;
    line_length[starty] = inp - startp;

    return inp;
}

/* inp <= gap_start */
void render_screen(uint8_t* inp)
{
    uint8_t x, y;
    screen_getcursor(&x, &y);

    while (y != viewheight)
        display_height[y++] = 0;

    for (;;)
    {
        screen_getcursor(&x, &y);
        if (y >= viewheight)
            break;

        if (inp == current_line)
            current_line_y = y;

        inp = draw_line(inp);
    }

    // 画面再描画後にカーソル追跡をリセット（古いカーソル表示は自然に上書きされる）
    reset_cursor_display();
}

void adjust_scroll_position(void)
{
    uint8_t total_height = 0;

    first_line = current_line;
    while (first_line != buffer_start)
    {
        uint8_t* line_start = first_line;
        const uint8_t* line_end = line_start--;
        while ((line_start != buffer_start) && (line_start[-1] != '\n'))
            line_start--;

        total_height +=
            (compute_length(line_start, line_end, NULL) / width) + 1;
        if (total_height > (viewheight / 2))
            break;
        first_line = line_start;
    }

    screen_setcursor(0, 0);
    render_screen(first_line);
}

void recompute_screen_position(void)
{
    const uint8_t* inp;

    if (current_line < first_line)
        adjust_scroll_position();

    for (;;)
    {
        inp = first_line;
        current_line_y = 0;
        while (current_line_y < viewheight)
        {
            if (inp == current_line)
                break;

            uint8_t h = display_height[current_line_y];
            inp += line_length[current_line_y];

            current_line_y += h;
        }

        if ((current_line_y >= viewheight) ||
            ((current_line_y + display_height[current_line_y]) > viewheight))
        {
            adjust_scroll_position();
        }
        else
            break;
    }

    uint8_t length = compute_length(current_line, gap_start, NULL);
    screen_setcursor(length % width, current_line_y + (length / width));
}

void redraw_current_line(void)
{
    uint8_t* nextp;
    uint8_t oldheight;

    oldheight = display_height[current_line_y];
    screen_setcursor(0, current_line_y);
    nextp = draw_line(current_line);
    if (oldheight != display_height[current_line_y])
        render_screen(nextp);
    else
        // render_screen()が呼ばれない場合もカーソル追跡をリセット
        reset_cursor_display();

    recompute_screen_position();
}

/* ======================================================================= */
/*                                LIFECYCLE                                */
/* ======================================================================= */

static void format_status_with_path(const char* prefix, const char* path)
{
    strncpy(buffer, prefix, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    append_filename(path);
    print_status(buffer);
}

static bool insert_file(const char* path)
{
#ifdef ENABLE_FILE_IO
    if (!path || !*path)
        return false;

    format_status_with_path("Reading ", path);

    // Set filename for KERNAL
    cbm_k_setnam(path);

    // Set logical file parameters (LFN=1, Device=8, Secondary=0 for read)
    cbm_k_setlfs(1, 8, 0);

    // Load file to current gap position
    // flag=0 means load, not verify
    void* end_addr = cbm_k_load(0, gap_start);

    // Check for error (high byte of return address < $20 indicates error)
    if ((uint16_t)end_addr < 0x2000)
    {
        print_status("Load failed");
        return false;
    }

    // Calculate how many bytes were loaded
    uint16_t bytes_loaded = (uint8_t*)end_addr - gap_start;

    // Update gap_start to point after loaded data
    gap_start += bytes_loaded;

    // Convert CR to LF
    uint8_t* p = gap_start - bytes_loaded;
    while (p < gap_start)
    {
        if (*p == '\r')
            *p = '\n';
        p++;
    }

    if (bytes_loaded > 0)
        dirty = true;

    return true;
#else
    (void)path;
    print_status("File I/O disabled");
    return false;
#endif
}

void load_file(void)
{
    new_file();
    if (filename_set)
        insert_file(current_filename);
    dirty = false;
    goto_line(1);
}

bool save_file(void)
{
#ifdef ENABLE_FILE_IO
    if (!filename_set)
    {
        print_status("No filename set");
        return false;
    }

    format_status_with_path("Writing ", current_filename);

    // Set filename for KERNAL
    cbm_k_setnam(current_filename);

    // Set logical file parameters (LFN=1, Device=8, Secondary=1 for write)
    cbm_k_setlfs(1, 8, 1);

    // Calculate buffer range to save
    // We need to save from buffer_start to gap_start, then gap_end to buffer_end
    // But KERNAL save needs continuous memory, so we need to compact first

    // Move gap to end of buffer to make data continuous
    while (gap_end != buffer_end)
        *gap_start++ = *gap_end++;

    // Now all data is continuous from buffer_start to gap_start
    uint8_t result = cbm_k_save(buffer_start, gap_start);

    // Restore gap position (move gap back to current position)
    // This is simplified - in real use we'd need to track original gap position

    bool ok = (result == 0);
    if (!ok)
    {
        print_status("Save failed");
    }

#else
    print_status("File I/O disabled");
    bool ok = false;
#endif

    if (ok)
    {
        dirty = false;
        print_status("File saved");
    }

    return ok;
}

void quit(void)
{
    screen_shutdown();

    // Restore normal memory configuration
    POKE(0x01, 0x37);

    // Restore KERNAL vectors and I/O
    __asm__("jsr $FF8A");  // RESTOR: Restore vectors
    __asm__("jsr $FF81");  // CINT: Initialize screen editor
    __asm__("jsr $FF84");  // IOINIT: Initialize I/O

    __asm__("ldx #$ff");
    __asm__("txs");

    __asm__("jmp $FCE2");
    //exit(0);
}

/* ======================================================================= */
/*                            EDITOR OPERATIONS                            */
/* ======================================================================= */

void cursor_home(uint16_t count)
{
    (void)count;
    while (gap_start != current_line)
        *--gap_end = *--gap_start;
}

void cursor_end(uint16_t count)
{
    (void)count;
    while ((gap_end != buffer_end) && (gap_end[0] != '\n'))
        *gap_start++ = *gap_end++;
}

void cursor_left(uint16_t count)
{
    while (count--)
    {
        if ((gap_start != buffer_start) && (gap_start[-1] != '\n'))
        {
            *--gap_end = *--gap_start;

            // Shift-JISの2バイト目に着地した場合、1バイト目まで戻る
            if (is_at_sjis_second_byte(gap_start, current_line))
                *--gap_end = *--gap_start;
        }
    }
}

void cursor_right(uint16_t count)
{
    while (count--)
    {
        if ((gap_end != buffer_end) && (gap_end[0] != '\n'))
        {
            uint8_t c = *gap_start++ = *gap_end++;

            // Shift-JIS第1バイトなら、第2バイトも移動
            if (is_sjis_lead(c) && gap_end != buffer_end && *gap_end != '\n')
                *gap_start++ = *gap_end++;
        }
    }
}

void cursor_down(uint16_t count)
{
    while (count--)
    {
        uint16_t visual_col = count_visual_chars(current_line, gap_start);
        cursor_end(1);
        if (gap_end == buffer_end)
            return;

        *gap_start++ = *gap_end++;
        current_line = gap_start;
        cursor_right(visual_col);
    }
}

void cursor_up(uint16_t count)
{
    while (count--)
    {
        uint16_t visual_col = count_visual_chars(current_line, gap_start);

        cursor_home(1);
        if (gap_start == buffer_start)
            return;

        do
            *--gap_end = *--gap_start;
        while ((gap_start != buffer_start) && (gap_start[-1] != '\n'));

        current_line = gap_start;
        cursor_right(visual_col);
    }
}




void insert_newline(void)
{
    if (gap_start != gap_end)
    {
        *gap_start++ = '\n';
        screen_setcursor(0, current_line_y);

        // 改行挿入後は複数行を再描画する必要がある
        render_screen(current_line);

        // 新しい行に移動
        current_line = gap_start;
        recompute_screen_position();
    }
}

static bool insert_key(uint8_t key, bool replacing)
{
    if (key == 127)
    {
        if (gap_start != current_line)
        {
            // Shift-JIS対応：バックスペース処理
            gap_start--;  // まず1バイト削除

            // 削除後の位置が全角文字の2バイト目になった場合、1バイト目も削除
            if (gap_start > current_line && is_at_sjis_second_byte(gap_start, current_line))
            {
                gap_start--;  // 1バイト目も削除
            }

            return true;
        }
        return false;
    }

    if (gap_start == gap_end)
        return false;

    if (replacing && (gap_end != buffer_end) && (*gap_end != '\n'))
    {
        uint8_t c = *gap_end;
        gap_end++;
        // Shift-JIS第1バイトなら第2バイトも削除
        if (is_sjis_lead(c) && gap_end != buffer_end && *gap_end != '\n')
            gap_end++;
    }

    if (key == 13)
    {
        insert_newline();
        return true;
    }

    *gap_start++ = key;
    return true;
}

#ifdef QE_ENABLE_IME
static bool insert_bytes(const uint8_t* data, uint8_t length, bool replacing)
{
    bool modified = false;

    if (!data || length == 0)
        return false;

    for (uint8_t i = 0; i < length; ++i)
    {
        if (insert_key(data[i], replacing))
        {
            modified = true;
        }
        else if (gap_start == gap_end)
        {
            break;
        }
    }

    return modified;
}

static bool process_ime_insert(bool replacing)
{
    for (;;)
    {
        uint8_t event = ime_process();

        switch (event)
        {
            case IME_EVENT_NONE:
                if (ime_is_active())
                    continue;
                return false;

            case IME_EVENT_CONFIRMED:
            {
                const uint8_t* text = ime_get_result_text();
                uint8_t length = ime_get_result_length();
                if (insert_bytes(text, length, replacing))
                {
                    dirty = true;
                    redraw_current_line();
                }
                ime_clear_output();
                return true;
            }

            case IME_EVENT_KEY_PASSTHROUGH:
            {
                uint8_t key = ime_get_passthrough_key();
                if (key == 20)
                    key = 127;
                if (key <= 127 && insert_key(key, replacing))
                {
                    dirty = true;
                    // 改行の場合はinsert_newline()内で再描画するため、ここでは再描画しない
                    if (key != 13)
                        redraw_current_line();
                }
                return true;
            }

            case IME_EVENT_MODE_CHANGED:
                // モード変更（ひらがな⇔カタカナ）はIME継続
                return true;

            case IME_EVENT_CANCELLED:
            case IME_EVENT_DEACTIVATED:
                // IME無効化時は通常入力に戻る
                return false;

            default:
                return true;
        }
    }
}
#endif

static void set_insert_status(bool replacing)
{
    goto_status_line();
    screen_setstyle(0);
    screen_clear_to_eol();
    set_status_line(replacing ? "Replace mode" : "Insert mode");
}

void insert_mode(bool replacing)
{
    set_insert_status(replacing);

#ifdef QE_ENABLE_IME
    bool ime_was_active = false;
#endif

    for (;;)
    {
#ifdef QE_ENABLE_IME
        if (process_ime_insert(replacing))
        {
            ime_was_active = true;
            update_cursor_display();
            continue;
        }
        // IME無効化後のみステータスラインを再設定
        if (ime_was_active)
        {
            set_insert_status(replacing);
            ime_was_active = false;
        }
#endif

        uint8_t c = screen_waitchar();
        if (c == 27)
            break;
        if (c > 127)
            continue;

        if (insert_key(c, replacing))
        {
            dirty = true;
            // 改行の場合はinsert_newline()内で再描画するため、ここでは再描画しない
            if (c != 13)
                redraw_current_line();
        }

        // 文字入力後にカーソル表示を更新
        update_cursor_display();
    }

    set_status_line("");
}

void insert_text(uint16_t count)
{
    (void)count;
    insert_mode(false);
}

void append_text(uint16_t count)
{
    cursor_end(1);
    recompute_screen_position();
    insert_text(count);
}

void goto_line(uint16_t lineno)
{
    while (gap_start != buffer_start)
        *--gap_end = *--gap_start;
    current_line = buffer_start;

    while ((gap_end != buffer_end) && --lineno)
    {
        while (gap_end != buffer_end)
        {
            uint16_t c = *gap_start++ = *gap_end++;
            if (c == '\n')
            {
                current_line = gap_start;
                break;
            }
        }
    }
}

void delete_right(uint16_t count)
{
    while (count--)
    {
        if (gap_end == buffer_end)
            break;

        // Shift-JIS対応：カーソル位置の文字を確認
        if (is_sjis_lead(*gap_end)) {
            // 全角文字の1文字目なら2バイト削除
            gap_end++;
            if (gap_end < buffer_end)
                gap_end++;
        } else {
            // 半角文字または全角文字の2文字目なら1バイト削除
            gap_end++;
        }
    }

    redraw_current_line();
    dirty = true;
}

void delete_rest_of_line(uint16_t count)
{
    while ((gap_end != buffer_end) && (gap_end[0] != '\n'))
        gap_end++;

    if (count != 0)
        redraw_current_line();
    dirty = true;
}

void delete_line(uint16_t count)
{
    while (count--)
    {
        cursor_home(1);
        delete_rest_of_line(0);
        if (gap_end != buffer_end)
        {
            gap_end++;
            display_height[current_line_y] = 0;
        }
    }

    redraw_current_line();
    dirty = true;
}



void change_rest_of_line(uint16_t count)
{
    delete_rest_of_line(1);
    insert_text(count);
}

void join(uint16_t count)
{
    while (count--)
    {
        uint8_t* ptr = gap_end;
        while ((ptr != buffer_end) && (*ptr != '\n'))
            ptr++;

        if (ptr != buffer_end)
            *ptr = ' ';
    }

    screen_setcursor(0, current_line_y);
    render_screen(current_line);
    dirty = true;
}

void open_above(uint16_t count)
{
    if (gap_start == gap_end)
        return;

    cursor_home(1);
    *--gap_end = '\n';

    recompute_screen_position();
    screen_setcursor(0, current_line_y);

    // render_screen前に古いカーソルをクリア
    clear_and_reset_cursor_display();
    render_screen(current_line);
    recompute_screen_position();

    // カーソル表示を明示的に更新
    update_cursor_display();

    insert_text(count);
}

void open_below(uint16_t count)
{
    cursor_down(1);
    open_above(count);
}

void replace_char(uint16_t count)
{
    (void)count;
    uint8_t c = screen_waitchar();

    if (gap_end == buffer_end)
        return;
    if (c == '\n')
    {
        gap_end++;
        /* The cursor ends up *after* the newline. */
        insert_newline();
    }
    else if (isprint(c))
    {
        *gap_end = c;
        /* The cursor ends on *on* the replace character. */
        redraw_current_line();
    }
}

void replace_line(uint16_t count)
{
    (void)count;
    insert_mode(true);
}

void zed_save_and_quit(uint16_t count)
{
    (void)count;
    if (!dirty)
        quit();
    if (!filename_set)
    {
        set_status_line("No filename set");
        return;
    }
    if (save_file())
        quit();
}

void zed_force_quit(uint16_t count)
{
    (void)count;
    quit();
}

void redraw_screen(uint16_t count)
{
    (void)count;
    screen_clear();
    render_screen(first_line);
}

void enter_delete_mode(uint16_t count)
{
    bindings = &delete_bindings;
    command_count = count;
}

void enter_zed_mode(uint16_t count)
{
    bindings = &zed_bindings;
    command_count = count;
}

void enter_change_mode(uint16_t count)
{
    bindings = &change_bindings;
    command_count = count;
}

// デバッグ: gapアドレス表示
void show_gap_debug(uint16_t count)
{
    (void)count;
    char buf[32];
    buf[0] = 'S';
    buf[1] = ':';

    // gap_start表示
    uint16_t addr = (uint16_t)gap_start;
    for (int i = 3; i >= 0; i--) {
        uint8_t n = (addr >> (i * 4)) & 0xF;
        buf[5-i] = (n < 10) ? ('0' + n) : ('A' + n - 10);
    }

    buf[6] = ' ';
    buf[7] = 'E';
    buf[8] = ':';

    // gap_end表示
    addr = (uint16_t)gap_end;
    for (int i = 3; i >= 0; i--) {
        uint8_t n = (addr >> (i * 4)) & 0xF;
        buf[12-i] = (n < 10) ? ('0' + n) : ('A' + n - 10);
    }

    buf[13] = '\0';
    set_status_line(buf);
}

const char normal_keys[] = "^$hjkliAGxJOorR:\022dZcD\210\211\212\213";

command_t* const normal_cbs[] = {
    cursor_home,
    cursor_end,
    cursor_left,
    cursor_down,
    cursor_up,
    cursor_right,
    insert_text,
    append_text,
    goto_line,
    delete_right,
    join,
    open_above,
    open_below,
    replace_char,
    replace_line,
    colon,
    redraw_screen,
    enter_delete_mode,
    enter_zed_mode,
    enter_change_mode,
    show_gap_debug,  // Dキーでデバッグ表示
    cursor_left,
    cursor_right,
    cursor_down,
    cursor_up,
};

const struct bindings normal_bindings = {NULL, normal_keys, normal_cbs};

const char delete_keys[] = "d$";
command_t* const delete_cbs[] = {
    delete_line,
    delete_rest_of_line,
};

const struct bindings delete_bindings = {"Delete", delete_keys, delete_cbs};

const char change_keys[] = "$";
command_t* const change_cbs[] = {
    change_rest_of_line,
};

const struct bindings change_bindings = {"Change", change_keys, change_cbs};

const char zed_keys[] = "ZQ";
command_t* const zed_cbs[] = {
    zed_save_and_quit,
    zed_force_quit,
};

const struct bindings zed_bindings = {"Zed", zed_keys, zed_cbs};

/* ======================================================================= */
/*                             COLON COMMANDS                              */
/* ======================================================================= */

void set_current_filename(const char* f)
{
    if (!f || !*f)
    {
        filename_set = false;
        current_filename[0] = '\0';
        return;
    }

    strncpy(current_filename, f, sizeof(current_filename) - 1);
    current_filename[sizeof(current_filename) - 1] = '\0';
    filename_set = true;
    dirty = true;
}

void print_no_filename(void)
{
    set_status_line("No filename set");
}

void print_document_not_saved(void)
{
    set_status_line("Not saved! Use :e! to force");
}

void print_colon_status(const char* s)
{
    set_status_line(s);
}

static bool read_colon_input(char* out, size_t maxlen)
{
    size_t len = 0;
    for (;;)
    {
        uint8_t c = screen_waitchar();
        if (c == 27)
        {
            out[0] = '\0';
            return false;
        }
        if (c == '\r')
        {
            out[len] = '\0';
            return len != 0;
        }
        if ((c == 127 || c == '\b') && len > 0)
        {
            len--;
            screen_setcursor((uint8_t)(1 + len), viewheight);
            screen_putchar(' ');
            screen_setcursor((uint8_t)(1 + len), viewheight);
            continue;
        }
        if (isprint(c) && len < maxlen - 1)
        {
            out[len++] = (char)c;
            screen_putchar((char)c);
        }
    }
}

void colon(uint16_t count)
{
    (void)count;
    print_status = print_colon_status;

    for (;;)
    {
        update_cursor_display();
        goto_status_line();
        screen_setstyle(1);
        screen_putstring(":");
        screen_clear_to_eol();
        screen_setstyle(0);
        screen_setcursor(1, viewheight);
        screen_showcursor(1);

        char input[128];
        bool have_input = read_colon_input(input, sizeof(input));
        print_newline();

        if (!have_input)
            break;

        char* w = strtok(input, " ");
        if (!w)
            continue;
        char* arg = strtok(NULL, " ");

        switch (*w)
        {
            case 'w':
            {
                bool quitting = w[1] == 'q';
                if (arg)
                    set_current_filename(arg);
                if (!filename_set)
                    print_no_filename();
                else if (save_file() && quitting)
                    quit();
                break;
            }

            case 'r':
            {
                if (arg)
                    insert_file(arg);
                else
                    print_no_filename();
                break;
            }

            case 'e':
            {
                if (!arg)
                {
                    print_no_filename();
                    break;  // コマンドのbreakではなく、switch文のbreak
                }
                else if (dirty && (w[1] != '!'))
                {
                    print_document_not_saved();
                    // エラー時は単にbreakして、メッセージを表示したまま通常モードに戻る
                    goto colon_exit;
                }
                else
                {
                    set_current_filename(arg);
                    if (filename_set)
                        load_file();
                }
                break;
            }

            case 'p':
            {
                strcpy(buffer, "File: ");
                append_filename(filename_set ? current_filename : NULL);
                print_colon_status(buffer);
                break;
            }

            case 'n':
            {
                if (dirty && (w[1] != '!'))
                    print_document_not_saved();
                else
                {
                    new_file();
                    filename_set = false;
                    current_filename[0] = '\0';
                    dirty = false;
                }
                break;
            }

            case 'q':
            {
                if (!dirty || (w[1] == '!'))
                    quit();
                else
                    print_document_not_saved();
                break;
            }

            default:
                set_status_line("Unknown command");
        }
    }

    screen_clear();
    screen_showcursor(1);
    print_status = set_status_line;
    render_screen(first_line);
    return;

colon_exit:
    screen_showcursor(1);
    print_status = set_status_line;
    // エラー時は画面をクリアしない（メッセージを残すため）
}

/* ======================================================================= */
/*                            EDITOR OPERATIONS                            */
/* ======================================================================= */

int main(void)
{
#ifdef QE_ENABLE_IME
    ime_init();
#endif

    screen_init();
    screen_clear();

    // Continue with original QE functionality
    buffer_start = editor_buffer;
    buffer_end = editor_buffer + EDITOR_BUFFER_SIZE;

    // LO ROM
    // HIGH RAM
    POKE(0x01, (PEEK(0x01) & 0xfc) | 0x02);


    screen_getsize(&width, &height);
    width += 1;
    height += 1;
    viewheight = height - 1;
    status_line_length = 0;
    print_status = set_status_line;

    new_file();

    screen_setcursor(0, 0);
    render_screen(first_line);
    bindings = &normal_bindings;

    command_count = 0;
    for (;;)
    {
        recompute_screen_position();
        update_cursor_display();

        char c;
        for (;;)
        {
            c = screen_waitchar();
            if (isdigit(c))
            {
                command_count = (command_count * 10) + (c - '0');
                itoa(command_count, buffer);
                strcat(buffer, " repeat");
                set_status_line(buffer);
            }
            else
            {
                set_status_line("");
                break;
            }
        }

        const char* cmdp = strchr(bindings->keys, c);
        if (cmdp)
        {
            command_t* cmd = bindings->callbacks[cmdp - bindings->keys];
            uint16_t count = command_count;
            if (count == 0)
            {
                if (cmd == goto_line)
                    count = UINT16_MAX;
                else
                    count = 1;
            }
            command_count = 0;

            bindings = &normal_bindings;
            set_status_line("");
            cmd(count);
            if (bindings->name)
                set_status_line(bindings->name);
        }
        else
        {
            set_status_line("Unknown key");
            bindings = &normal_bindings;
            command_count = 0;
        }
    }
    return 0;
}
