#include "ime.h"
#include "jtxt.h"
#include "c64_oscar.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define ROM_BASE 0x8000U
#define BANK_REG 0xDE00U

#define CIA1_DATA_A 0xDC00U
#define CIA1_DATA_B 0xDC01U

// KERNAL GETIN wrapper for Oscar64
// Using global variable to work around inline assembly limitations
static volatile uint8_t getin_result;

static uint8_t cbm_k_getin(void)
{
    __asm {
        jsr $FFE4
        sta getin_result
    }
    return getin_result;
}

#define COLOR_DEFAULT_FG 1
#define COLOR_DEFAULT_BG 0
#define COLOR_STATUS_FG  0
#define COLOR_STATUS_BG  1

#define KEY_SPACE  32
#define KEY_RETURN 13
#define KEY_ESC    27

#define COMMODORE_ROW 0x7FU
#define COMMODORE_BIT 0x20U
#define SPACE_ROW     0x7FU
#define SPACE_BIT     0x10U

#define FKEY_ROW      0xFEU
#define F1_BIT        0x10U
#define F3_BIT        0x20U
#define F5_BIT        0x40U

#define IME_STATUS_WIDTH 4

#define ROMAJI_BUFFER_SIZE    8
#define HIRAGANA_BUFFER_SIZE 64
#define CONVERSION_KEY_SIZE   64
#define CANDIDATE_BUFFER_SIZE 256
#define MAX_CANDIDATES        16

#define HIRAGANA_BUFFER_LIMIT (HIRAGANA_BUFFER_SIZE - 2)

#define PEEK(addr) (*(volatile uint8_t*)(addr))
#define POKE(addr, val) (PEEK(addr) = (uint8_t)(val))

static uint16_t mkword(uint8_t hi, uint8_t lo) {
    return ((uint16_t)hi << 8) | lo;
}

static uint8_t msb16(uint16_t value) {
    return (uint8_t)(value >> 8);
}

static uint8_t lsb16(uint16_t value) {
    return (uint8_t)(value & 0xFF);
}

enum {
    ROMAJI_EMPTY = 0,
    ROMAJI_CONSONANT,
    ROMAJI_N,
    ROMAJI_SMALL_TSU,
    ROMAJI_WAITING_2ND,
    ROMAJI_X_PREFIX,
    ROMAJI_Y_WAITING,
    ROMAJI_SKIP_NEXT
};

enum {
    IME_STATE_INPUT = 0,
    IME_STATE_CONVERTING = 1
};

static const uint16_t basic_hiragana[] = {
    0x82A0, 0x82A2, 0x82A4, 0x82A6, 0x82A8,
    0x82A9, 0x82AB, 0x82AD, 0x82AF, 0x82B1,
    0x82B3, 0x82B5, 0x82B7, 0x82B9, 0x82BB,
    0x82BD, 0x82BF, 0x82C2, 0x82C4, 0x82C6,
    0x82C8, 0x82C9, 0x82CA, 0x82CB, 0x82CC,
    0x82CD, 0x82D0, 0x82D3, 0x82D6, 0x82D9,
    0x82DC, 0x82DD, 0x82DE, 0x82DF, 0x82E0,
    0x82E2, 0x82E4, 0x82E6,
    0x82E7, 0x82E8, 0x82E9, 0x82EA, 0x82EB,
    0x82ED, 0x82F0, 0x82F1
};

static const uint16_t dakuten_hiragana[] = {
    0x82AA, 0x82AC, 0x82AE, 0x82B0, 0x82B2,
    0x82B4, 0x82B6, 0x82B8, 0x82BA, 0x82BC,
    0x82BE, 0x82C0, 0x82C3, 0x82C5, 0x82C7,
    0x82CE, 0x82D1, 0x82D4, 0x82D7, 0x82DA
};

static const uint16_t handakuten_hiragana[] = {
    0x82CF, 0x82D2, 0x82D5, 0x82D8, 0x82DB
};

static const uint16_t small_hiragana[] = {
    0x829F, 0x82A1, 0x82A3, 0x82A5, 0x82A7,
    0x82E1, 0x82E3, 0x82E5,
    0x82C1
};

static const uint8_t status_label_hiragana[] = { 0x5B, 0x82, 0xA0, 0x5D, 0x00 };
static const uint8_t status_label_katakana[] = { 0x5B, 0x83, 0x41, 0x5D, 0x00 };
static const uint8_t status_label_fullwidth[] = { 0x5B, 0x82, 0x60, 0x5D, 0x00 };

static bool ime_active = false;
static bool prev_commodore_state = false;
static bool prev_space_state = false;
static bool ime_has_output = false;
static bool is_verb_first = false;

static uint8_t ime_input_mode = IME_MODE_HIRAGANA;
static uint8_t ime_conversion_state = IME_STATE_INPUT;

static uint8_t romaji_state = ROMAJI_EMPTY;
static uint8_t last_consonant = 0;
static uint8_t second_consonant = 0;
static uint8_t romaji_buffer[ROMAJI_BUFFER_SIZE];
static uint8_t romaji_pos = 0;
static uint8_t hiragana_buffer[HIRAGANA_BUFFER_SIZE];
static uint8_t hiragana_pos = 0;

static uint8_t conversion_key_buffer[CONVERSION_KEY_SIZE];
static uint8_t conversion_key_length = 0;

static uint8_t saved_cursor_x = 0;
static uint8_t saved_cursor_y = 0;
static uint8_t saved_color = 0;
static uint8_t passthrough_key = 0;

static uint8_t candidates_buffer[CANDIDATE_BUFFER_SIZE];
static uint8_t candidate_offsets[MAX_CANDIDATES];
static uint8_t candidate_buffer_pos = 0;
static uint8_t candidate_count = 0;
static uint8_t current_candidate = 0;

static uint8_t ime_output_buffer[128];
static uint8_t ime_output_length = 0;

static uint8_t prev_display_length = 0;
static uint8_t prev_display_chars = 0;
static uint8_t prev_romaji_pos = 0;

static uint8_t current_entry_length = 0;

static uint8_t current_bank = IME_DICTIONARY_START_BANK;
static uint16_t current_offset = 0;

static uint8_t verb_match_length = 0;
static uint8_t verb_match_bank = 0;
static uint16_t verb_match_offset = 0;
static uint16_t verb_match_okurigana = 0;
static uint8_t verb_candidate_count = 0;

static uint8_t match_length = 0;
static uint8_t match_bank = 0;
static uint16_t match_offset = 0;
static uint16_t match_okurigana = 0;
static uint8_t match_candidate_count = 0;

static void clear_romaji_buffer(void);
static void clear_hiragana_buffer(void);
static bool input_romaji(uint8_t key);
static bool handle_empty_state(uint8_t key);
static bool handle_consonant_state(uint8_t key);
static bool handle_n_state(uint8_t key);
static bool handle_small_tsu_state(uint8_t key);
static bool handle_waiting_2nd_state(uint8_t key);
static bool handle_x_prefix_state(uint8_t key);
static bool handle_y_waiting_state(uint8_t key);
static bool handle_skip_next_state(uint8_t key);
static void force_confirm_romaji(void);
static bool backspace_romaji(void);
static void recalculate_state(void);
static void add_to_hiragana_buffer(uint16_t sjis_char);
static void add_small_tsu(void);
static void add_n(void);
static uint8_t vowel_index(uint8_t ch);
static uint8_t consonant_to_row(uint8_t ch);
static uint16_t convert_special_2char(uint8_t ch1, uint8_t ch2);
static uint16_t convert_special_1char(uint8_t consonant, uint8_t vowel);
static uint16_t convert_basic(uint8_t consonant, uint8_t vowel);
static uint16_t convert_dakuten(uint8_t consonant, uint8_t vowel);
static uint16_t convert_handakuten(uint8_t consonant, uint8_t vowel);
static uint16_t convert_small(uint8_t vowel_or_y);
static uint16_t convert_youon(uint8_t consonant, uint8_t y_vowel);
static uint16_t convert_special_youon(uint8_t ch1, uint8_t ch2, uint8_t y_vowel);
static void convert_to_katakana(uint8_t* target_ptr);
static void convert_to_hiragana(uint8_t* target_ptr);
static bool is_commodore_pressed(void);
static bool is_space_pressed(void);
static bool is_f1_pressed(void);
static bool is_f3_pressed(void);
static bool is_f5_pressed(void);
static bool check_commodore_space(void);
static void activate_ime_input(void);
static void deactivate_ime_input_internal(void);
static void clear_ime_input_line(void);
static void show_ime_status(void);
static bool process_ime_key(uint8_t key);
static void input_ime_char(uint8_t key);
static void backspace_ime_input(void);
static void confirm_ime_input(void);
static void cancel_ime_input_internal(void);
static void update_ime_display(void);
static void display_input_text(void);
static void display_conversion_candidates(void);
static const uint8_t* get_ime_output_internal(void);
static uint8_t get_ime_output_length_internal(void);
static void clear_ime_output_internal(void);
static void clear_ime_input_area(void);
static void clear_conversion_key_buffer(void);
static uint8_t bytes_to_chars(uint8_t byte_length);
static void jtxt_bput_number(uint8_t value);
static uint8_t read_rom_byte(uint8_t bank, uint16_t offset);
static uint8_t read_dic_byte(void);
static uint8_t read_dic_string(uint8_t* buffer);
static uint8_t hiragana_to_index(uint8_t first_byte, uint8_t second_byte);
static bool check_okurigana_match(const uint8_t* okurigana_buffer, uint8_t verb_suffix);
static bool search_noun_entries(const uint8_t* key_buffer, uint8_t key_length);
static bool search_verb_entries(const uint8_t* key_buffer, uint8_t key_length);
static bool search_entries_in_group(const uint8_t* key_buffer, uint8_t key_length, bool is_verb_search);
static void add_candidates(uint16_t okurigana);
static bool start_conversion(void);
static void next_candidate(void);
static void prev_candidate(void);
static uint8_t* get_current_candidate(void);
static void confirm_conversion(void);
static void cancel_conversion(void);
static void backup_cursor(void);
static void restore_cursor(void);
static uint8_t check_mode_keys(void);
static uint8_t petscii_to_ascii(uint8_t key);
static uint8_t vowel_index(uint8_t ch) {
    switch (ch) {
        case 'a': return 0;
        case 'i': return 1;
        case 'u': return 2;
        case 'e': return 3;
        case 'o': return 4;
        default:  return 0xFF;
    }
}

static uint8_t consonant_to_row(uint8_t ch) {
    switch (ch) {
        case 'k': return 1;
        case 's': return 2;
        case 't':
        case 'c': return 3;
        case 'n': return 4;
        case 'h':
        case 'f': return 5;
        case 'm': return 6;
        case 'y': return 7;
        case 'r': return 8;
        case 'w': return 9;
        default:  return 0xFF;
    }
}

static uint16_t convert_special_2char(uint8_t ch1, uint8_t ch2) {
    if (ch1 == 's' && ch2 == 'h') {
        return 0x82B5;
    }
    if (ch1 == 'c' && ch2 == 'h') {
        return 0x82BF;
    }
    if (ch1 == 't' && ch2 == 's') {
        return 0x82C2;
    }
    return 0;
}

static uint16_t convert_special_1char(uint8_t consonant, uint8_t vowel) {
    switch (consonant) {
        case 's':
            if (vowel == 'i') {
                return 0x82B5;
            }
            break;
        case 't':
            if (vowel == 'i') {
                return 0x82BF;
            }
            if (vowel == 'u') {
                return 0x82C2;
            }
            break;
        case 'h':
            if (vowel == 'u') {
                return 0x82D3;
            }
            break;
        case 'f':
            switch (vowel) {
                case 'a':
                    add_to_hiragana_buffer(0x82D3);
                    add_to_hiragana_buffer(0x829F);
                    return 0xFFFF;
                case 'i':
                    add_to_hiragana_buffer(0x82D3);
                    add_to_hiragana_buffer(0x82A1);
                    return 0xFFFF;
                case 'u':
                    return 0x82D3;
                case 'e':
                    add_to_hiragana_buffer(0x82D3);
                    add_to_hiragana_buffer(0x82A5);
                    return 0xFFFF;
                case 'o':
                    add_to_hiragana_buffer(0x82D3);
                    add_to_hiragana_buffer(0x82A7);
                    return 0xFFFF;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return 0;
}

static uint16_t convert_basic(uint8_t consonant, uint8_t vowel) {
    uint8_t row = consonant_to_row(consonant);
    uint8_t vol = vowel_index(vowel);

    if (row == 0xFF || vol == 0xFF) {
        return 0;
    }

    if (row == 7) {
        switch (vol) {
            case 0: return basic_hiragana[35];
            case 2: return basic_hiragana[36];
            case 4: return basic_hiragana[37];
            default: return 0;
        }
    }

    if (row == 9) {
        switch (vol) {
            case 0:
                return basic_hiragana[43];
            case 1:
                add_to_hiragana_buffer(0x82A4);
                add_to_hiragana_buffer(0x82A1);
                return 0xFFFF;
            case 3:
                add_to_hiragana_buffer(0x82A4);
                add_to_hiragana_buffer(0x82A5);
                return 0xFFFF;
            case 4:
                return basic_hiragana[44];
            default:
                return 0;
        }
    }

    if (row >= 1 && row <= 8 && row != 7) {
        uint8_t index;
        if (row < 7) {
            index = (uint8_t)((row - 1) * 5 + 5 + vol);
        } else {
            index = (uint8_t)(38 + vol);
        }
        return basic_hiragana[index];
    }

    return 0;
}

static uint16_t convert_dakuten(uint8_t consonant, uint8_t vowel) {
    uint8_t vol = vowel_index(vowel);
    uint8_t base = 0xFF;

    if (vol > 4) {
        return 0;
    }
    switch (consonant) {
        case 'g': base = 0; break;
        case 'z': base = 5; break;
        case 'd': base = 10; break;
        case 'b': base = 15; break;
        default: break;
    }

    if (base == 0xFF) {
        return 0;
    }
    return dakuten_hiragana[base + vol];
}

static uint16_t convert_handakuten(uint8_t consonant, uint8_t vowel) {
    uint8_t vol;

    if (consonant == 'p') {
        vol = vowel_index(vowel);
        if (vol <= 4) {
            return handakuten_hiragana[vol];
        }
    }
    return 0;
}

static uint16_t convert_small(uint8_t vowel_or_y) {
    switch (vowel_or_y) {
        case 'a': return small_hiragana[0];
        case 'i': return small_hiragana[1];
        case 'u': return small_hiragana[2];
        case 'e': return small_hiragana[3];
        case 'o': return small_hiragana[4];
        case 'y': return small_hiragana[5];
        default:  return 0;
    }
}

static uint16_t convert_youon(uint8_t consonant, uint8_t y_vowel) {
    uint16_t base_i = 0;
    uint16_t small_ya = 0;
    uint8_t base_index = 0;

    switch (consonant) {
        case 'k': base_index = 6; break;
        case 's': base_index = 11; break;
        case 't': base_index = 16; break;
        case 'n': base_index = 21; break;
        case 'h': base_index = 26; break;
        case 'f': base_index = 27; break;
        case 'm': base_index = 31; break;
        case 'r': base_index = 39; break;
        case 'g': base_index = (uint8_t)(1 | 0x40); break;
        case 'z': base_index = (uint8_t)(6 | 0x40); break;
        case 'j': base_index = (uint8_t)(6 | 0x40); break;
        case 'd': base_index = (uint8_t)(11 | 0x40); break;
        case 'b': base_index = (uint8_t)(16 | 0x40); break;
        case 'p': base_index = (uint8_t)(1 | 0x80); break;
        default: return 0;
    }

    switch (base_index & 0xC0) {
        case 0x00:
            base_i = basic_hiragana[base_index & 0x3F];
            break;
        case 0x40:
            base_i = dakuten_hiragana[base_index & 0x3F];
            break;
        case 0x80:
            base_i = handakuten_hiragana[base_index & 0x3F];
            break;
        default:
            base_i = 0;
            break;
    }

    if (base_i == 0) {
        return 0;
    }
    switch (y_vowel) {
        case 'a': small_ya = small_hiragana[5]; break;
        case 'u': small_ya = small_hiragana[6]; break;
        case 'o': small_ya = small_hiragana[7]; break;
        default: return 0;
    }

    add_to_hiragana_buffer(base_i);
    add_to_hiragana_buffer(small_ya);
    return 0xFFFF;
}

static uint16_t convert_special_youon(uint8_t ch1, uint8_t ch2, uint8_t y_vowel) {
    uint16_t base = 0;
    uint16_t small_ya = 0;

    if (ch1 == 'c' && ch2 == 'h') {
        base = 0x82BF;
    } else if (ch1 == 's' && ch2 == 'h') {
        base = 0x82B5;
    } else {
        return 0;
    }
    switch (y_vowel) {
        case 'a': small_ya = small_hiragana[5]; break;
        case 'u': small_ya = small_hiragana[6]; break;
        case 'o': small_ya = small_hiragana[7]; break;
        default: return 0;
    }

    add_to_hiragana_buffer(base);
    add_to_hiragana_buffer(small_ya);
    return 0xFFFF;
}

static void add_to_hiragana_buffer(uint16_t sjis_char) {
    uint8_t high;
    uint8_t low;

    if (hiragana_pos >= HIRAGANA_BUFFER_LIMIT) {
        return;
    }

    high = msb16(sjis_char);
    low = lsb16(sjis_char);

    if (ime_input_mode == IME_MODE_KATAKANA) {
        if (high >= 0x82 && low >= 0x9F && low <= 0xF1) {
            high = 0x83;
            if (low <= 0xDD) {
                low = (uint8_t)(low - 0x5F);
            } else {
                low = (uint8_t)(low - 0x5E);
            }
        }
    }

    hiragana_buffer[hiragana_pos] = high;
    hiragana_buffer[hiragana_pos + 1] = low;
    hiragana_pos = (uint8_t)(hiragana_pos + 2);
}

static void add_small_tsu(void) {
    add_to_hiragana_buffer(small_hiragana[8]);
}

static void add_n(void) {
    add_to_hiragana_buffer(basic_hiragana[45]);
}
static void clear_romaji_buffer(void) {
    romaji_pos = 0;
    romaji_state = ROMAJI_EMPTY;
    last_consonant = 0;
    second_consonant = 0;
    memset(romaji_buffer, 0, sizeof(romaji_buffer));
}

static void clear_hiragana_buffer(void) {
    hiragana_pos = 0;
    memset(hiragana_buffer, 0, sizeof(hiragana_buffer));
}

static void clear_conversion_key_buffer(void) {
    memset(conversion_key_buffer, 0, sizeof(conversion_key_buffer));
}
static void debug_print_state(uint8_t key) {
    (void)key;
}
static bool input_romaji(uint8_t key) {
    if (key < 32 || key > 126) {
        return false;
    }

    if (romaji_pos >= ROMAJI_BUFFER_SIZE - 1) {
        force_confirm_romaji();
    }

    romaji_buffer[romaji_pos++] = key;

    debug_print_state(key);

    switch (romaji_state) {
        case ROMAJI_EMPTY:
            return handle_empty_state(key);
        case ROMAJI_CONSONANT:
            return handle_consonant_state(key);
        case ROMAJI_N:
            return handle_n_state(key);
        case ROMAJI_SMALL_TSU:
            return handle_small_tsu_state(key);
        case ROMAJI_WAITING_2ND:
            return handle_waiting_2nd_state(key);
        case ROMAJI_X_PREFIX:
            return handle_x_prefix_state(key);
        case ROMAJI_Y_WAITING:
            return handle_y_waiting_state(key);
        case ROMAJI_SKIP_NEXT:
            return handle_skip_next_state(key);
        default:
            clear_romaji_buffer();
            return false;
    }
}
static bool handle_empty_state(uint8_t key) {
    uint8_t vol;
    uint8_t row;

    switch (key) {
        case '-':
            add_to_hiragana_buffer(0x815B);
            clear_romaji_buffer();
            return true;
        case ',':
            add_to_hiragana_buffer(0x8141);
            clear_romaji_buffer();
            return true;
        case '.':
            add_to_hiragana_buffer(0x8142);
            clear_romaji_buffer();
            return true;
        default:
            break;
    }

    vol = vowel_index(key);
    if (vol != 0xFF) {
        add_to_hiragana_buffer(basic_hiragana[vol]);
        clear_romaji_buffer();
        return true;
    }

    if (key == 'n') {
        romaji_state = ROMAJI_N;
        last_consonant = 'n';
        return true;
    }

    if (key == 'x') {
        romaji_state = ROMAJI_X_PREFIX;
        return true;
    }

    row = consonant_to_row(key);
    if (row != 0xFF || key == 'g' || key == 'z' || key == 'd' ||
        key == 'b' || key == 'p' || key == 'j') {
        if (key == 's' || key == 'c' || key == 't' || key == 'f' || key == 'd') {
            romaji_state = ROMAJI_WAITING_2ND;
            last_consonant = key;
            return true;
        }

        romaji_state = ROMAJI_CONSONANT;
        last_consonant = key;
        return true;
    }

    clear_romaji_buffer();
    return false;
}
static bool handle_consonant_state(uint8_t key) {
    uint8_t vol;
    uint16_t result = 0;

    if (key == last_consonant && key != 'n') {
        add_small_tsu();
        romaji_state = ROMAJI_CONSONANT;
        return true;
    }

    if (last_consonant == 'j') {
        if (key == 'i') {
            add_to_hiragana_buffer(0x82B6);
            clear_romaji_buffer();
            return true;
        }
        if (key == 'a' || key == 'u' || key == 'o') {
            add_to_hiragana_buffer(0x82B6);
            switch (key) {
                case 'a': add_to_hiragana_buffer(0x82E1); break;
                case 'u': add_to_hiragana_buffer(0x82E3); break;
                case 'o': add_to_hiragana_buffer(0x82E5); break;
            }
            clear_romaji_buffer();
            return true;
        }
        if (key == 'e') {
            add_to_hiragana_buffer(0x82B6);
            add_to_hiragana_buffer(0x82A5);
            clear_romaji_buffer();
            return true;
        }
        if (key == 'y') {
            romaji_state = ROMAJI_Y_WAITING;
            return true;
        }
        clear_romaji_buffer();
        return false;
    }

    vol = vowel_index(key);
    if (vol != 0xFF) {
        result = convert_special_1char(last_consonant, key);
        if (result == 0) {
            result = convert_dakuten(last_consonant, key);
            if (result == 0) {
                result = convert_handakuten(last_consonant, key);
                if (result == 0) {
                    result = convert_basic(last_consonant, key);
                }
            }
        }

        if (result != 0) {
            if (result != 0xFFFF) {
                add_to_hiragana_buffer(result);
            }
            clear_romaji_buffer();
            return true;
        }
    }

    if (key == 'y') {
        romaji_state = ROMAJI_Y_WAITING;
        return true;
    }

    clear_romaji_buffer();
    return false;
}
static bool handle_n_state(uint8_t key) {
    uint8_t row;
    uint8_t vol;
    uint16_t result;

    if (key == 'n') {
        add_n();
        clear_romaji_buffer();
        return true;
    }

    row = consonant_to_row(key);
    if (row != 0xFF && key != 'y') {
        add_n();
        clear_romaji_buffer();
        return input_romaji(key);
    }

    if (key == 'j' || key == 'g' || key == 'z' || key == 'd' || key == 'b' || key == 'p') {
        add_n();
        clear_romaji_buffer();
        return input_romaji(key);
    }

    vol = vowel_index(key);
    if (vol != 0xFF) {
        result = convert_basic('n', key);
        if (result != 0) {
            if (result != 0xFFFF) {
                add_to_hiragana_buffer(result);
            }
            clear_romaji_buffer();
            return true;
        }
    }

    if (key == 'y') {
        romaji_state = ROMAJI_Y_WAITING;
        return true;
    }

    clear_romaji_buffer();
    return false;
}
static bool handle_small_tsu_state(uint8_t key) {
    clear_romaji_buffer();
    return input_romaji(key);
}
static bool handle_waiting_2nd_state(uint8_t key) {
    if (key == last_consonant) {
        add_small_tsu();
        return true;
    }

    if (key == 'h') {
        second_consonant = key;
        romaji_state = ROMAJI_Y_WAITING;
        return true;
    }

    if (last_consonant == 't' && key == 's') {
        add_to_hiragana_buffer(0x82C2);
        clear_romaji_buffer();
        romaji_state = ROMAJI_SKIP_NEXT;
        return true;
    }

    romaji_state = ROMAJI_CONSONANT;
    return handle_consonant_state(key);
}

static bool handle_x_prefix_state(uint8_t key) {
    uint16_t result = convert_small(key);
    if (result != 0) {
        add_to_hiragana_buffer(result);
        clear_romaji_buffer();
        return true;
    }

    clear_romaji_buffer();
    return false;
}
static bool handle_y_waiting_state(uint8_t key) {
    uint8_t vol;
    uint16_t result;

    result = 0;

    if (second_consonant == 'h') {
        vol = vowel_index(key);
        if (vol != 0xFF) {
            switch (key) {
                case 'i':
                    if (last_consonant == 'd') {
                        add_to_hiragana_buffer(0x82C5);
                        add_to_hiragana_buffer(0x82A1);
                        clear_romaji_buffer();
                        return true;
                    }
                    result = convert_special_2char(last_consonant, second_consonant);
                    if (result != 0) {
                        add_to_hiragana_buffer(result);
                        clear_romaji_buffer();
                        return true;
                    }
                    break;
                case 'a':
                case 'u':
                case 'o':
                    if (last_consonant == 'd') {
                        add_to_hiragana_buffer(0x82C5);
                        if (key == 'a') {
                            add_to_hiragana_buffer(0x82E1);
                        } else if (key == 'u') {
                            add_to_hiragana_buffer(0x82E3);
                        } else {
                            add_to_hiragana_buffer(0x82E5);
                        }
                        clear_romaji_buffer();
                        return true;
                    }
                    result = convert_special_youon(last_consonant, second_consonant, key);
                    if (result != 0) {
                        clear_romaji_buffer();
                        return true;
                    }
                    break;
                case 'e':
                    if (last_consonant == 'd') {
                        add_to_hiragana_buffer(0x82C5);
                        add_to_hiragana_buffer(0x82A5);
                        clear_romaji_buffer();
                        return true;
                    }
                    break;
                default:
                    break;
            }
        }
    } else {
        if (key == 'a' || key == 'u' || key == 'o') {
            result = convert_youon(last_consonant, key);
            if (result != 0) {
                clear_romaji_buffer();
                return true;
            }
        }
    }

    clear_romaji_buffer();
    return false;
}

static bool handle_skip_next_state(uint8_t key) {
    (void)key;
    clear_romaji_buffer();
    return true;
}
static void force_confirm_romaji(void) {
    if (romaji_state == ROMAJI_N) {
        add_n();
    }
    clear_romaji_buffer();
}

static bool backspace_romaji(void) {
    if (romaji_pos == 0) {
        if (hiragana_pos >= 2) {
            hiragana_pos = (uint8_t)(hiragana_pos - 2);
            hiragana_buffer[hiragana_pos] = 0;
            hiragana_buffer[hiragana_pos + 1] = 0;
            return true;
        }
        return false;
    }

    if (romaji_pos > 0) {
        romaji_buffer[--romaji_pos] = 0;
        recalculate_state();
        return true;
    }

    return false;
}

static void recalculate_state(void) {
    uint8_t last_char;
    uint8_t saved_char;

    if (romaji_pos == 0) {
        clear_romaji_buffer();
        return;
    }

    last_char = romaji_buffer[romaji_pos - 1];

    if (romaji_pos == 1) {
        if (last_char == 'n') {
            romaji_state = ROMAJI_N;
            last_consonant = 'n';
        } else if (last_char == 'x') {
            romaji_state = ROMAJI_X_PREFIX;
            last_consonant = 0;
            second_consonant = 0;
        } else if (last_char == 's' || last_char == 'c' || last_char == 't' ||
                   last_char == 'f' || last_char == 'd') {
            romaji_state = ROMAJI_WAITING_2ND;
            last_consonant = last_char;
            second_consonant = 0;
        } else {
            uint8_t row = consonant_to_row(last_char);
            if (row != 0xFF || last_char == 'g' || last_char == 'z' ||
                last_char == 'd' || last_char == 'b' || last_char == 'p' ||
                last_char == 'j') {
                romaji_state = ROMAJI_CONSONANT;
                last_consonant = last_char;
                second_consonant = 0;
            } else {
                clear_romaji_buffer();
            }
        }
        return;
    }

    if (romaji_pos == 2) {
        uint8_t first_char = romaji_buffer[0];

        if ((first_char == 's' && last_char == 'h') ||
            (first_char == 'c' && last_char == 'h') ||
            (first_char == 't' && last_char == 's')) {
            romaji_state = ROMAJI_Y_WAITING;
            last_consonant = first_char;
            second_consonant = last_char;
            return;
        }

        if (last_char == 'y') {
            romaji_state = ROMAJI_Y_WAITING;
            last_consonant = first_char;
            second_consonant = 0;
            return;
        }

        romaji_state = ROMAJI_CONSONANT;
        last_consonant = first_char;
        second_consonant = 0;
        return;
    }

    saved_char = last_char;
    clear_romaji_buffer();
    romaji_buffer[0] = saved_char;
    romaji_pos = 1;
    recalculate_state();
}
static void convert_to_katakana(uint8_t* target_ptr) {
    uint8_t i = 0;
    uint8_t ch;
    while (target_ptr[i] != 0) {
        if (target_ptr[i] < 0x80) {
            ++i;
            continue;
        }
        ch = target_ptr[i + 1];
        if (target_ptr[i] >= 0x82 && ch >= 0x9F && ch <= 0xF1) {
            target_ptr[i] = 0x83;
            if (ch <= 0xDD) {
                target_ptr[i + 1] = (uint8_t)(ch - 0x5F);
            } else {
                target_ptr[i + 1] = (uint8_t)(ch - 0x5E);
            }
        }
        i = (uint8_t)(i + 2);
    }
}

static void convert_to_hiragana(uint8_t* target_ptr) {
    uint8_t i = 0;
    uint8_t ch;
    while (target_ptr[i] != 0) {
        if (target_ptr[i] < 0x80) {
            ++i;
            continue;
        }
        ch = target_ptr[i + 1];
        if (target_ptr[i] == 0x83 && ch >= 0x40 && ch <= 0x93) {
            target_ptr[i] = 0x82;
            if (ch <= 0x7E) {
                target_ptr[i + 1] = (uint8_t)(ch + 0x5F);
            } else {
                target_ptr[i + 1] = (uint8_t)(ch + 0x5E);
            }
        }
        i = (uint8_t)(i + 2);
    }
}
static bool is_commodore_pressed(void) {
    uint8_t result;

    POKE(CIA1_DATA_A, COMMODORE_ROW);
    result = PEEK(CIA1_DATA_B);
    return (result & COMMODORE_BIT) == 0;
}

static bool is_space_pressed(void) {
    uint8_t result;

    POKE(CIA1_DATA_A, SPACE_ROW);
    result = PEEK(CIA1_DATA_B);
    return (result & SPACE_BIT) == 0;
}

static bool is_f1_pressed(void) {
    uint8_t result;

    POKE(CIA1_DATA_A, FKEY_ROW);
    result = PEEK(CIA1_DATA_B);
    return (result & F1_BIT) == 0;
}

static bool is_f3_pressed(void) {
    uint8_t result;

    POKE(CIA1_DATA_A, FKEY_ROW);
    result = PEEK(CIA1_DATA_B);
    return (result & F3_BIT) == 0;
}

static bool is_f5_pressed(void) {
    uint8_t result;

    POKE(CIA1_DATA_A, FKEY_ROW);
    result = PEEK(CIA1_DATA_B);
    return (result & F5_BIT) == 0;
}

static bool check_commodore_space(void) {
    bool current_commodore = is_commodore_pressed();
    bool current_space = is_space_pressed();

    bool trigger = false;
    if (current_commodore && current_space && !prev_space_state) {
        trigger = true;
    }

    prev_commodore_state = current_commodore;
    prev_space_state = current_space;

    return trigger;
}
static void clear_ime_input_line(void) {
    jtxt_bwindow_disable();
    jtxt_bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
    jtxt_blocate(0, 24);
    {
        uint8_t i;
        for (i = 0; i < 40; ++i) {
            jtxt_bputc(32);
        }
    }

    prev_display_length = 0;
    prev_display_chars = 0;
    prev_romaji_pos = 0;

    if (ime_active) {
        jtxt_bwindow_enable();
    }
}

static void show_ime_status(void) {
    const uint8_t* label = status_label_hiragana;

    if (!ime_active) {
        return;
    }

    switch (ime_input_mode) {
        case IME_MODE_KATAKANA:
            label = status_label_katakana;
            break;
        case IME_MODE_FULLWIDTH:
            label = status_label_fullwidth;
            break;
        default:
            label = status_label_hiragana;
            break;
    }

    jtxt_bwindow_disable();
    jtxt_blocate(37, 24);
    jtxt_bcolor(COLOR_STATUS_FG, COLOR_STATUS_BG);
    jtxt_bputs((const char*)label);
    jtxt_bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
    jtxt_bwindow_enable();
}

static void activate_ime_input(void) {
    jtxt_bwindow(0, 23);
    clear_ime_input_line();
    show_ime_status();
}

static void deactivate_ime_input_internal(void) {
    jtxt_bwindow_disable();
    clear_ime_input_line();
    clear_romaji_buffer();
    clear_hiragana_buffer();
    ime_conversion_state = IME_STATE_INPUT;
    candidate_count = 0;
    current_candidate = 0;
    conversion_key_length = 0;
    candidate_buffer_pos = 0;
    verb_candidate_count = 0;
    match_candidate_count = 0;
    passthrough_key = 0;
}

static void clear_ime_input_area(void) {
    jtxt_bwindow_disable();
    jtxt_bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
    jtxt_blocate(0, 24);
    {
        uint8_t i;
        for (i = 0; i < 37; ++i) {
            jtxt_bputc(32);
        }
    }
    if (ime_active) {
        jtxt_bwindow_enable();
    }
}

static uint8_t bytes_to_chars(uint8_t byte_length) {
    uint8_t char_count;
    uint8_t i;

    if (byte_length == 0) {
        return 0;
    }

    char_count = 0;
    i = 0;
    while (i < byte_length) {
        if (jtxt_is_firstsjis(hiragana_buffer[i])) {
            ++char_count;
            i = (uint8_t)(i + 2);
        } else {
            ++char_count;
            ++i;
        }
    }

    return char_count;
}
static const uint8_t* get_ime_output_internal(void) {
    return ime_has_output ? ime_output_buffer : NULL;
}

static uint8_t get_ime_output_length_internal(void) {
    return ime_has_output ? ime_output_length : 0;
}

static void clear_ime_output_internal(void) {
    ime_has_output = false;
    ime_output_length = 0;
}
static void update_ime_display(void) {
    jtxt_bwindow_disable();
    jtxt_bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);

    if (ime_conversion_state == IME_STATE_INPUT) {
        display_input_text();
    } else {
        display_conversion_candidates();
    }

    jtxt_bwindow_enable();
}
static void display_input_text(void) {
    uint8_t current_length = hiragana_pos;
    uint8_t current_chars = bytes_to_chars(current_length);
    uint8_t current_romaji = romaji_pos;
    uint8_t total_chars = (uint8_t)(current_chars + current_romaji);
    uint8_t prev_total_chars = (uint8_t)(prev_display_chars + prev_romaji_pos);

    bool needs_update = (current_length != prev_display_length) ||
                        (current_romaji != prev_romaji_pos);

    if (!needs_update) {
        return;
    }

    if (total_chars < 37) {
        uint8_t i;

        jtxt_blocate(0, 24);

        for (i = 0; i < current_length; ++i) {
            jtxt_bputc(hiragana_buffer[i]);
        }

        for (i = 0; i < current_romaji; ++i) {
            jtxt_bputc(romaji_buffer[i]);
        }

        if (total_chars < prev_total_chars) {
            for (i = total_chars; i < prev_total_chars; ++i) {
                jtxt_bputc(32);
            }
        }
    }

    prev_display_length = current_length;
    prev_display_chars = current_chars;
    prev_romaji_pos = current_romaji;
}
static void jtxt_bput_number(uint8_t value) {
    char buffer[4];
    uint8_t pos = 0;

    if (value >= 100) {
        buffer[pos++] = (char)('0' + (value / 100));
        value %= 100;
    }
    if (value >= 10 || pos > 0) {
        buffer[pos++] = (char)('0' + (value / 10));
        value %= 10;
    }
    buffer[pos++] = (char)('0' + value);

    {
        uint8_t i;
        for (i = 0; i < pos; ++i) {
            jtxt_bputc((uint8_t)buffer[i]);
        }
    }
}
static void display_conversion_candidates(void) {
    uint8_t* candidate_str;
    uint8_t i;
    uint8_t col;
    uint8_t ch;

    clear_ime_input_area();

    if (candidate_count == 0) {
        return;
    }

    jtxt_bwindow_disable();
    jtxt_blocate(0, 24);

    candidate_str = get_current_candidate();
    if (candidate_str != NULL) {
        i = 0;
        col = 0;
        while (candidate_str[i] != 0 && col < 20) {
            ch = candidate_str[i];
            jtxt_bputc(ch);
            if (jtxt_is_firstsjis(ch)) {
                ++i;
                if (candidate_str[i] != 0) {
                    jtxt_bputc(candidate_str[i]);
                }
                col = (uint8_t)(col + 2);
            } else {
                col = (uint8_t)(col + 1);
            }
            ++i;
        }
    }

    jtxt_bputc(' ');
    jtxt_bput_number((uint8_t)(current_candidate + 1));
    jtxt_bputc('/');
    jtxt_bput_number(candidate_count);

    prev_display_length = 0;
    prev_display_chars = 0;
    prev_romaji_pos = 0;
}
static void input_ime_char(uint8_t key) {
    uint8_t processed_key = key;
    if (key >= 'A' && key <= 'Z') {
        processed_key = (uint8_t)(key + 32);
    }

    bool converted = input_romaji(processed_key);

    // input_romajiが失敗した場合も表示を更新
    // （バッファがクリアされた状態を反映するため）
    (void)converted;  // 現在は戻り値を使わないが、将来のために保持

    update_ime_display();
}

static void backspace_ime_input(void) {
    if (backspace_romaji()) {
        update_ime_display();
    }
}

static void confirm_ime_input(void) {
    ime_output_length = hiragana_pos;
    if (ime_output_length > sizeof(ime_output_buffer)) {
        ime_output_length = sizeof(ime_output_buffer);
    }

    if (ime_output_length > 0) {
        uint8_t i;
        for (i = 0; i < ime_output_length; ++i) {
            ime_output_buffer[i] = hiragana_buffer[i];
        }
        ime_has_output = true;
    }

    clear_romaji_buffer();
    clear_hiragana_buffer();

    prev_display_length = 0;
    prev_display_chars = 0;
    prev_romaji_pos = 0;

    clear_ime_input_area();
    update_ime_display();
}

static void cancel_ime_input_internal(void) {
    clear_romaji_buffer();
    clear_hiragana_buffer();

    prev_display_length = 0;
    prev_display_chars = 0;
    prev_romaji_pos = 0;

    update_ime_display();
}
static bool process_ime_key(uint8_t key) {
    if (!ime_active) {
        return false;
    }

    if (is_f1_pressed()) {
        ime_set_hiragana_mode();
        return true;
    }
    if (is_f3_pressed()) {
        ime_set_katakana_mode();
        return true;
    }
    if (is_f5_pressed()) {
        ime_set_alphanumeric_mode();
        return true;
    }

    switch (key) {
        case KEY_RETURN:
            if (ime_conversion_state == IME_STATE_INPUT) {
                confirm_ime_input();
            } else {
                confirm_conversion();
                update_ime_display();
            }
            return true;
        case KEY_ESC:
            if (ime_conversion_state == IME_STATE_INPUT) {
                cancel_ime_input_internal();
            } else {
                cancel_conversion();
            }
            return true;
        case 8:
        case 20:
            if (ime_conversion_state == IME_STATE_INPUT) {
                backspace_ime_input();
            } else {
                cancel_conversion();
                backspace_ime_input();
            }
            return true;
        case KEY_SPACE:
            if (ime_input_mode == IME_MODE_KATAKANA) {
                input_ime_char(KEY_SPACE);
            } else {
                if (ime_conversion_state == IME_STATE_INPUT) {
                    if (start_conversion()) {
                        update_ime_display();
                    } else {
                        input_ime_char(KEY_SPACE);
                    }
                } else {
                    next_candidate();
                    update_ime_display();
                }
            }
            return true;
        case 160:
            if (ime_input_mode != IME_MODE_KATAKANA &&
                ime_conversion_state == IME_STATE_CONVERTING) {
                prev_candidate();
                update_ime_display();
                return true;
            }
            return false;
        default:
            if (key >= 32 && key <= 126) {
                if (ime_conversion_state == IME_STATE_INPUT) {
                    input_ime_char(key);
                } else {
                    confirm_conversion();
                    update_ime_display();
                    input_ime_char(key);
                }
                return true;
            }
            break;
    }

    return false;
}
void ime_init(void) {
    ime_active = false;
    ime_input_mode = IME_MODE_HIRAGANA;

    clear_romaji_buffer();
    clear_hiragana_buffer();
    clear_conversion_key_buffer();

    prev_commodore_state = false;
    prev_space_state = false;
    prev_display_length = 0;
    prev_display_chars = 0;
    prev_romaji_pos = 0;
    ime_has_output = false;
    ime_output_length = 0;

    ime_conversion_state = IME_STATE_INPUT;
    candidate_count = 0;
    current_candidate = 0;
    conversion_key_length = 0;
    candidate_buffer_pos = 0;
    verb_candidate_count = 0;
    match_candidate_count = 0;
    passthrough_key = 0;
}

void ime_toggle_mode(void) {
    ime_active = !ime_active;
    if (ime_active) {
        activate_ime_input();
    } else {
        deactivate_ime_input_internal();
    }
}

bool ime_is_active(void) {
    return ime_active;
}

void ime_set_hiragana_mode(void) {
    ime_input_mode = IME_MODE_HIRAGANA;
    if (hiragana_pos > 0) {
        convert_to_hiragana(hiragana_buffer);
        prev_display_chars = 0;
        prev_display_length = 0;
        prev_romaji_pos = 0;
        update_ime_display();
    }
    if (ime_active) {
        show_ime_status();
    }
}

void ime_set_katakana_mode(void) {
    ime_input_mode = IME_MODE_KATAKANA;
    if (hiragana_pos > 0) {
        convert_to_katakana(hiragana_buffer);
        prev_display_chars = 0;
        prev_display_length = 0;
        prev_romaji_pos = 0;
        update_ime_display();
    }
    if (ime_active) {
        show_ime_status();
    }
}

void ime_set_alphanumeric_mode(void) {
    ime_input_mode = IME_MODE_FULLWIDTH;
    if (ime_active) {
        show_ime_status();
    }
}

uint8_t ime_get_input_mode(void) {
    return ime_input_mode;
}

void ime_activate(void) {
    ime_active = true;
    activate_ime_input();

    jtxt_bwindow_enable();
}

void ime_deactivate(void) {
    ime_active = false;
    deactivate_ime_input_internal();
    clear_ime_output_internal();
}
static void backup_cursor(void) {
    saved_cursor_x = jtxt_state.cursor_x;
    saved_cursor_y = jtxt_state.cursor_y;
    saved_color = jtxt_state.bitmap_color;
}

static void restore_cursor(void) {
    jtxt_blocate(saved_cursor_x, saved_cursor_y);
    jtxt_bcolor(saved_color >> 4, saved_color & 0x0F);
}
uint8_t ime_process(void) {
    uint8_t key;
    uint8_t mode_event;
    bool handled;
    const uint8_t* output;
    uint8_t length;

    if (check_commodore_space()) {
        backup_cursor();
        if (ime_active) {
            ime_deactivate();
            restore_cursor();
            return IME_EVENT_DEACTIVATED;
        }
        ime_activate();
        restore_cursor();
        return IME_EVENT_NONE;
    }

    if (!ime_active) {
        return IME_EVENT_NONE;
    }

    key = (uint8_t)cbm_k_getin();
    if (key == 0) {
        return IME_EVENT_NONE;
    }

    backup_cursor();

    key = petscii_to_ascii(key);

    mode_event = check_mode_keys();
    if (mode_event != IME_EVENT_NONE) {
        restore_cursor();
        return mode_event;
    }

    clear_ime_output_internal();

    if (key == KEY_ESC) {
        cancel_ime_input_internal();
        restore_cursor();
        return IME_EVENT_CANCELLED;
    }

    if ((key == 20 || key == KEY_RETURN) && romaji_pos == 0 && hiragana_pos == 0) {
        passthrough_key = key;
        restore_cursor();
        return IME_EVENT_KEY_PASSTHROUGH;
    }

    handled = process_ime_key(key);
    if (handled) {
        output = get_ime_output_internal();
        if (output != NULL) {
            length = get_ime_output_length_internal();
            if (length > 0) {
                restore_cursor();
                return IME_EVENT_CONFIRMED;
            }
        }
    }

    restore_cursor();
    return IME_EVENT_NONE;
}
const uint8_t* ime_get_result_text(void) {
    return get_ime_output_internal();
}

uint8_t ime_get_result_length(void) {
    return get_ime_output_length_internal();
}

void ime_clear_output(void) {
    clear_ime_output_internal();
}

uint8_t ime_get_passthrough_key(void) {
    return passthrough_key;
}
static uint8_t check_mode_keys(void) {
    if (is_f1_pressed()) {
        ime_set_hiragana_mode();
        return IME_EVENT_MODE_CHANGED;
    }
    if (is_f3_pressed()) {
        ime_set_katakana_mode();
        return IME_EVENT_MODE_CHANGED;
    }
    if (is_f5_pressed()) {
        ime_set_alphanumeric_mode();
        return IME_EVENT_MODE_CHANGED;
    }
    return IME_EVENT_NONE;
}
static uint8_t petscii_to_ascii(uint8_t key) {
    uint8_t ascii = key;

    if (ascii >= 128) {
        ascii &= 0x7F;
    }

    switch (ascii) {
        case 0x1D: // cursor right
        case 0x11: // cursor down
        case 0x0D: // return
        case 0x14: // delete
        case 0x1B: // escape
            return ascii;
        default:
            break;
    }

    return ascii;
}

static uint8_t read_rom_byte(uint8_t bank, uint16_t offset) {
    uint8_t saved_01;
    uint8_t value;

    saved_01 = PEEK(0x01);
    POKE(0x01, saved_01 | 0x01);
    POKE(BANK_REG, bank);
    value = PEEK(ROM_BASE + offset);
    POKE(0x01, saved_01);
    return value;
}

static uint8_t read_dic_byte(void) {
    uint8_t data = read_rom_byte(current_bank, current_offset);
    ++current_offset;
    if (current_offset >= 8192) {
        current_offset = 0;
        ++current_bank;
    }
    return data;
}

static uint8_t read_dic_string(uint8_t* buffer) {
    uint8_t length = 0;
    while (length < 63) {
        uint8_t ch = read_dic_byte();
        buffer[length] = ch;
        if (ch == 0) {
            return length;
        }
        ++length;
    }
    buffer[length] = 0;
    return length;
}


static uint8_t hiragana_to_index(uint8_t first_byte, uint8_t second_byte) {
    uint16_t ch = mkword(first_byte, second_byte);
    if (ch < 0x82A0) {
        return 0xFF;
    }
    ch = (uint16_t)(ch - 0x82A0);
    if (ch <= 82) {
        return (uint8_t)ch;
    }
    return 0xFF;
}
static bool check_okurigana_match(const uint8_t* okurigana_buffer, uint8_t verb_suffix) {
    uint8_t second_byte;

    if (okurigana_buffer[0] != 0x82) {
        return false;
    }
    second_byte = okurigana_buffer[1];

    switch (verb_suffix) {
        case 'k':
            return second_byte >= 0xA9 && second_byte <= 0xB1 && (second_byte & 1U) == 1U;
        case 'g':
            return second_byte >= 0xAA && second_byte <= 0xB2 && (second_byte & 1U) == 0U;
        case 's':
            return second_byte >= 0xB3 && second_byte <= 0xBB && (second_byte & 1U) == 1U;
        case 'z':
        case 'j':
            return second_byte >= 0xB4 && second_byte <= 0xBC && (second_byte & 1U) == 0U;
        case 't':
            return second_byte == 0xBD || second_byte == 0xBF ||
                   second_byte == 0xC1 || second_byte == 0xC2 ||
                   second_byte == 0xC4 || second_byte == 0xC6;
        case 'd':
            return second_byte == 0xBE || second_byte == 0xC0 ||
                   second_byte == 0xC3 || second_byte == 0xC5 ||
                   second_byte == 0xC7;
        case 'n':
            return (second_byte >= 0xC8 && second_byte <= 0xCC) || second_byte == 0xF1;
        case 'h':
            return second_byte >= 0xCD && second_byte <= 0xD1;
        case 'b':
            return second_byte >= 0xD2 && second_byte <= 0xD6;
        case 'p':
            return second_byte >= 0xD7 && second_byte <= 0xDB;
        case 'm':
            return second_byte >= 0xDC && second_byte <= 0xE0;
        case 'r':
            return second_byte >= 0xE7 && second_byte <= 0xEB;
        case 'w':
            return second_byte == 0xED || second_byte == 0xF0 || second_byte == 0xA4;
        case 'i':
            return second_byte == 0xA2;
        case 'u':
            return second_byte == 0xA4;
        case 'e':
            return second_byte == 0xA6;
        case 'o':
            return second_byte == 0xA8;
        default:
            return false;
    }
}
static bool search_noun_entries(const uint8_t* key_buffer, uint8_t key_length) {
    uint8_t index;
    uint8_t offset_low;
    uint8_t offset_high;
    uint8_t offset_bank;

    if (key_length < 2) {
        return false;
    }

    index = hiragana_to_index(key_buffer[0], key_buffer[1]);
    if (index == 0xFF) {
        return false;
    }

    current_bank = IME_DICTIONARY_START_BANK;
    current_offset = (uint16_t)(4 + (uint16_t)index * 3U);

    offset_low = read_dic_byte();
    offset_high = read_dic_byte();
    offset_bank = read_dic_byte();

    if (offset_low == 0 && offset_high == 0 && offset_bank == 0) {
        return false;
    }

    current_bank = (uint8_t)(IME_DICTIONARY_START_BANK + offset_bank);
    current_offset = mkword(offset_high, offset_low);
    return search_entries_in_group(key_buffer, key_length, false);
}

static bool search_verb_entries(const uint8_t* key_buffer, uint8_t key_length) {
    uint8_t index;
    uint8_t offset_low;
    uint8_t offset_high;
    uint8_t offset_bank;

    if (key_length < 2) {
        return false;
    }

    index = hiragana_to_index(key_buffer[0], key_buffer[1]);
    if (index == 0xFF) {
        return false;
    }

    current_bank = IME_DICTIONARY_START_BANK;
    current_offset = (uint16_t)(250 + (uint16_t)index * 3U);

    offset_low = read_dic_byte();
    offset_high = read_dic_byte();
    offset_bank = read_dic_byte();

    if (offset_low == 0 && offset_high == 0 && offset_bank == 0) {
        return false;
    }

    current_bank = (uint8_t)(IME_DICTIONARY_START_BANK + offset_bank);
    current_offset = mkword(offset_high, offset_low);
    return search_entries_in_group(key_buffer, key_length, true);
}
static bool search_entries_in_group(const uint8_t* key_buffer, uint8_t key_length, bool is_verb_search) {
    uint8_t skip_low;
    uint8_t skip_high;
    uint16_t skip_size;
    uint8_t prev_skip_high;

    candidate_count = 0;

    skip_low = read_dic_byte();
    skip_high = read_dic_byte();
    skip_size = (uint16_t)(mkword(skip_high, skip_low) & 0x7FFFU);

    for (;;) {
        uint8_t entry_key_buffer[64];
        uint8_t entry_key_length;
        bool should_check;
        bool match;
        uint8_t compare_length;
        uint8_t last_char;

        entry_key_length = read_dic_string(entry_key_buffer);

        should_check = false;
        if (is_verb_search) {
            if (entry_key_length > 0 && entry_key_buffer[entry_key_length - 1] < 128) {
                if (key_length >= (uint8_t)(entry_key_length - 1)) {
                    should_check = true;
                }
            }
        } else if (entry_key_length > 0 && key_length >= entry_key_length) {
            should_check = true;
        }

        if (should_check) {
            uint8_t okurigana_high = 0;
            uint8_t okurigana_low = 0;


            match = true;
            compare_length = entry_key_length;

            if (is_verb_search) {
                compare_length = (uint8_t)(entry_key_length - 1);
            }

            {
                uint8_t i;
                for (i = 0; i < compare_length; ++i) {
                    if (key_buffer[i] != entry_key_buffer[i]) {
                        match = false;
                        break;
                    }
                }
            }

            if (is_verb_search && match) {
                last_char = entry_key_buffer[compare_length];
                if (last_char < 128) {
                    match = check_okurigana_match(&key_buffer[compare_length], last_char);
                    entry_key_length = (uint8_t)(compare_length + 1);
                } else {
                    match = false;
                }
            }

            if (match) {
                match_length = entry_key_length;
                match_bank = current_bank;
                match_offset = current_offset;
                match_okurigana = 0;

                current_entry_length = entry_key_length;
                if (is_verb_search) {
                    ++current_entry_length;
                    if (key_length > (uint8_t)(entry_key_length - 1) && key_length > entry_key_length) {
                        okurigana_high = key_buffer[entry_key_length - 1];
                        okurigana_low = key_buffer[entry_key_length];
                        match_okurigana = mkword(okurigana_high, okurigana_low);
                    }
                }

                return true;
            }
        }

        {
            uint16_t i;
            for (i = 0; i < skip_size; ++i) {
                (void)read_dic_byte();
            }
        }

        skip_low = read_dic_byte();
        skip_high = read_dic_byte();
        skip_size = (uint16_t)(mkword(skip_high, skip_low) & 0x7FFFU);

        if ((skip_high & 0x80U) != 0U) {
            break;
        }
    }

    return false;
}
static void add_candidates(uint16_t okurigana) {
    uint8_t num_candidates = read_dic_byte();

    {
        uint8_t i;
        for (i = 0; i < num_candidates; ++i) {
            uint8_t candidate_length;

            if (candidate_count >= MAX_CANDIDATES) {
                break;
            }

            if (candidate_buffer_pos >= CANDIDATE_BUFFER_SIZE - 1) {
                break;
            }

            candidate_offsets[candidate_count] = candidate_buffer_pos;
            candidate_length = read_dic_string(&candidates_buffer[candidate_buffer_pos]);
            candidate_buffer_pos = (uint8_t)(candidate_buffer_pos + candidate_length + 1);

            if (okurigana != 0) {
                if (candidate_buffer_pos >= CANDIDATE_BUFFER_SIZE - 2) {
                    candidates_buffer[CANDIDATE_BUFFER_SIZE - 1] = 0;
                    break;
                }
                --candidate_buffer_pos;
                candidates_buffer[candidate_buffer_pos++] = msb16(okurigana);
                candidates_buffer[candidate_buffer_pos++] = lsb16(okurigana);
                candidates_buffer[candidate_buffer_pos++] = 0;
            }

            ++candidate_count;
        }
    }
}
static bool start_conversion(void) {
    bool found = false;
    bool verb_found = false;
    bool noun_found = false;
    uint8_t magic0;
    uint8_t magic1;
    uint8_t magic2;

    if (hiragana_pos == 0) {
        return false;
    }

    conversion_key_length = hiragana_pos;
    if (conversion_key_length > sizeof(conversion_key_buffer)) {
        conversion_key_length = sizeof(conversion_key_buffer);
    }
    memcpy(conversion_key_buffer, hiragana_buffer, conversion_key_length);

    current_bank = IME_DICTIONARY_START_BANK;
    current_offset = 0;
    magic0 = read_rom_byte(IME_DICTIONARY_START_BANK, 0);
    magic1 = read_rom_byte(IME_DICTIONARY_START_BANK, 1);
    magic2 = read_rom_byte(IME_DICTIONARY_START_BANK, 2);

    if (magic0 != 'D' || magic1 != 'I' || magic2 != 'C') {
        return false;
    }


    candidate_buffer_pos = 0;
    candidate_count = 0;
    current_candidate = 0;
    candidates_buffer[0] = 0;
    verb_candidate_count = 0;
    match_candidate_count = 0;
    is_verb_first = false;

    found = search_verb_entries(conversion_key_buffer, conversion_key_length);
    verb_found = found;

    if (verb_found) {
        verb_match_length = match_length;
        verb_match_bank = match_bank;
        verb_match_offset = match_offset;
        verb_match_okurigana = match_okurigana;
    }

    noun_found = search_noun_entries(conversion_key_buffer, conversion_key_length);

    if (noun_found && verb_found) {
        is_verb_first = verb_match_length > match_length;
        if (is_verb_first) {
            current_bank = verb_match_bank;
            current_offset = verb_match_offset;
            add_candidates(verb_match_okurigana);
            verb_candidate_count = candidate_count;

            current_bank = match_bank;
            current_offset = match_offset;
            add_candidates(0);
            match_candidate_count = candidate_count - verb_candidate_count;
        } else {
            current_bank = match_bank;
            current_offset = match_offset;
            add_candidates(0);
            match_candidate_count = candidate_count;

            current_bank = verb_match_bank;
            current_offset = verb_match_offset;
            add_candidates(verb_match_okurigana);
            verb_candidate_count = (uint8_t)(candidate_count - match_candidate_count);
        }
    } else if (noun_found) {
        current_bank = match_bank;
        current_offset = match_offset;
        add_candidates(0);
        match_candidate_count = candidate_count;
    } else if (verb_found) {
        is_verb_first = true;
        current_bank = verb_match_bank;
        current_offset = verb_match_offset;
        add_candidates(verb_match_okurigana);
        verb_candidate_count = candidate_count;
    }

    if (noun_found || verb_found) {
        ime_conversion_state = IME_STATE_CONVERTING;
        current_candidate = 0;
        return candidate_count > 0;
    }

    return false;
}
static void next_candidate(void) {
    if (ime_conversion_state == IME_STATE_CONVERTING && candidate_count > 0) {
        ++current_candidate;
        if (current_candidate >= candidate_count) {
            current_candidate = 0;
        }
    }
}

static void prev_candidate(void) {
    if (ime_conversion_state == IME_STATE_CONVERTING && candidate_count > 0) {
        if (current_candidate == 0) {
            current_candidate = (uint8_t)(candidate_count - 1);
        } else {
            --current_candidate;
        }
    }
}

static uint8_t* get_current_candidate(void) {
    if (ime_conversion_state == IME_STATE_CONVERTING && candidate_count > 0 &&
        current_candidate < candidate_count) {
        return &candidates_buffer[candidate_offsets[current_candidate]];
    }
    return NULL;
}
static void confirm_conversion(void) {
    if (ime_conversion_state == IME_STATE_CONVERTING) {
        uint8_t* candidate_str = get_current_candidate();
        if (candidate_str != NULL) {
            uint8_t i;
            uint8_t entry_length;
            uint8_t remaining;

            ime_output_length = 0;
            i = 0;
            while (candidate_str[i] != 0 && ime_output_length < sizeof(ime_output_buffer)) {
                ime_output_buffer[ime_output_length++] = candidate_str[i++];
            }
            if (ime_output_length < sizeof(ime_output_buffer)) {
                ime_output_buffer[ime_output_length] = 0;
            }

            entry_length = 0;
            if (is_verb_first) {
                if (current_candidate < verb_candidate_count) {
                    entry_length = (uint8_t)(verb_match_length + 1);
                } else {
                    entry_length = match_length;
                }
            } else {
                if (current_candidate < match_candidate_count) {
                    entry_length = match_length;
                } else {
                    entry_length = (uint8_t)(verb_match_length + 1);
                }
            }

            if (entry_length > hiragana_pos) {
                entry_length = hiragana_pos;
            }

            if (entry_length > 0) {
                remaining = (uint8_t)(hiragana_pos - entry_length);
                memmove(hiragana_buffer, &hiragana_buffer[entry_length], remaining);
                hiragana_pos = remaining;
                if (hiragana_pos < HIRAGANA_BUFFER_SIZE) {
                    memset(&hiragana_buffer[hiragana_pos], 0, HIRAGANA_BUFFER_SIZE - hiragana_pos);
                }
            }

            clear_ime_input_area();
            ime_has_output = true;
        }
    }

    cancel_conversion();
}
static void cancel_conversion(void) {
    ime_conversion_state = IME_STATE_INPUT;
    candidate_count = 0;
    current_candidate = 0;
    candidate_buffer_pos = 0;
    conversion_key_length = 0;
    verb_candidate_count = 0;
    match_candidate_count = 0;
    clear_conversion_key_buffer();

    prev_display_length = 0;
    prev_display_chars = 0;
    prev_romaji_pos = 0;

    clear_ime_input_area();
    update_ime_display();

    jtxt_bwindow_disable();
    jtxt_bcolor(1, 0);
    jtxt_blocate(0, 23);
    jtxt_bwindow_enable();
}
#include "jtxt.h"
