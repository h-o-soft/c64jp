#include <c64.h>
#include <cbm.h>
#include <stdbool.h>
#include <stdint.h>

#include "jtxt.h"
#include "ime.h"

#define POKE(addr, val) (*(volatile uint8_t*)(addr) = (uint8_t)(val))
#define SCREEN_WIDTH 40

#define INDICATOR_BITMAP_ADDR (0x6000u + (39u * 8u))
#define INDICATOR_COLOR_ADDR  (0x5C00u + 39u)

static void show_title(void);
static void simple_input_test(void);
static void update_indicator(uint8_t frame_counter);
static void cleanup(void);

int main(void) {
    ime_init();
    jtxt_init(JTXT_BITMAP_MODE);

    show_title();
    jtxt_bwindow(1, 23);

    simple_input_test();
    cleanup();

    return 0;
}

static void show_title(void) {
    jtxt_bcolor(1, 0); // white on black
    jtxt_blocate(0, 0);
    jtxt_bputs("IME TEST v1.0");

    // Fill remaining cells on the first row with spaces
    uint8_t cursor = jtxt_state.cursor_x;
    for (; cursor < SCREEN_WIDTH; ++cursor) {
        jtxt_bputc(' ');
    }
}

static void simple_input_test(void) {
    bool exit_requested = false;
    uint8_t frame_counter = 0;

    jtxt_bwindow(1, 23);
    jtxt_bcolor(1, 6); // white on blue (BASIC style)
    jtxt_bcls();

    POKE(INDICATOR_COLOR_ADDR, 0x02); // red foreground indicator

    while (!exit_requested) {
        uint8_t event = ime_process();

        switch (event) {
            case IME_EVENT_CONFIRMED: {
                const uint8_t* text = ime_get_result_text();
                uint8_t length = ime_get_result_length();
                if (text != NULL && length > 0) {
                    for (uint8_t i = 0; i < length; ++i) {
                        jtxt_bputc(text[i]);
                    }
                }
                ime_clear_output();
                break;
            }
            case IME_EVENT_CANCELLED:
                // No action required
                break;
            case IME_EVENT_DEACTIVATED: {
                uint8_t saved_x = jtxt_state.cursor_x;
                uint8_t saved_y = jtxt_state.cursor_y;

                jtxt_bwindow_disable();
                jtxt_blocate(0, 24);
                for (uint8_t col = 0; col < SCREEN_WIDTH; ++col) {
                    jtxt_bputc(' ');
                }
                jtxt_bwindow(1, 23);
                jtxt_blocate(saved_x, saved_y);
                break;
            }
            case IME_EVENT_MODE_CHANGED:
                // Status row handled inside IME
                break;
            case IME_EVENT_KEY_PASSTHROUGH: {
                uint8_t passthrough_key = ime_get_passthrough_key();
                if (passthrough_key == 20) { // Backspace/Delete
                    if (jtxt_state.cursor_x > 0) {
                        uint8_t saved_x = (uint8_t)(jtxt_state.cursor_x - 1);
                        uint8_t saved_y = jtxt_state.cursor_y;
                        jtxt_blocate(saved_x, saved_y);
                        jtxt_bputc(' ');
                        jtxt_blocate(saved_x, saved_y);
                    }
                } else if (passthrough_key == 13) { // Return
                    jtxt_bnewline();
                }
                break;
            }
            case IME_EVENT_NONE: {
                // IMEが非アクティブ時のみメインループでキー処理
                if (!ime_is_active()) {
                    uint8_t key = (uint8_t)cbm_k_getin();
                    if (key != 0) {
                        if (key == 27) { // ESC
                            exit_requested = true;
                        } else if (key == 8 || key == 20) {
                            jtxt_bputc(8);
                        } else if (key == 13) {
                            jtxt_bputc(13);
                        } else if (key >= 32 && key <= 126) {
                            jtxt_bputc(key);
                        }
                    }
                }
                break;
            }
            default:
                break;
        }

        frame_counter++;
        update_indicator(frame_counter);
    }
}

static void update_indicator(uint8_t frame_counter) {
    static const uint8_t patterns[4] = { 0xFF, 0xAA, 0x00, 0x55 };
    uint8_t pattern = patterns[(frame_counter >> 3) & 0x03];

    for (uint8_t i = 0; i < 8; ++i) {
        POKE(INDICATOR_BITMAP_ADDR + i, pattern);
    }
}

static void cleanup(void) {
    if (ime_is_active()) {
        ime_toggle_mode();
    }
    jtxt_set_mode(JTXT_TEXT_MODE);
}
