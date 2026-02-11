#ifndef IME_H
#define IME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef JTXT_MAGICDESK_CRT
  #define IME_DICTIONARY_START_BANK 11
  #define IME_DICTIONARY_END_BANK   28
#else
  #define IME_DICTIONARY_START_BANK 10
  #define IME_DICTIONARY_END_BANK   27
#endif

#ifdef JTXT_EASYFLASH
// EasyFlash: dictionary physical bank start (after font banks 1-5)
#define IME_DIC_EF_START_BANK 6
#endif

#define IME_EVENT_NONE            0
#define IME_EVENT_CONFIRMED       1
#define IME_EVENT_CANCELLED       2
#define IME_EVENT_MODE_CHANGED    3
#define IME_EVENT_DEACTIVATED     4
#define IME_EVENT_KEY_PASSTHROUGH 5

#define IME_MODE_HIRAGANA   0
#define IME_MODE_KATAKANA   1
#define IME_MODE_FULLWIDTH  2

void ime_init(void);
void ime_toggle_mode(void);
bool ime_is_active(void);
void ime_set_hiragana_mode(void);
void ime_set_katakana_mode(void);
void ime_set_alphanumeric_mode(void);
uint8_t ime_get_input_mode(void);
void ime_activate(void);
void ime_deactivate(void);
uint8_t ime_process(void);
const uint8_t* ime_get_result_text(void);
uint8_t ime_get_result_length(void);
void ime_clear_output(void);
uint8_t ime_get_passthrough_key(void);

#endif /* IME_H */
