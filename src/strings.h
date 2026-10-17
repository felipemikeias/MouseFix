/*
 * strings.h - user-facing text. English is the default; Portuguese is used
 * when the Windows display language is Portuguese.
 */
#ifndef MOUSEFIX_STRINGS_H
#define MOUSEFIX_STRINGS_H

#include <wchar.h>

enum str_id {
    STR_TIP_ACTIVE,          /* %u = threshold in ms */
    STR_TIP_PAUSED,
    STR_HINT_TITLE_ACTIVE,
    STR_HINT_TITLE_PAUSED,
    STR_HINT_TEXT,
    STR_FIRST_RUN,
    STR_HOOK_FAILED,
    STR_BLOCKED_COUNT,       /* %lu = number of blocked clicks */
    STR_ENABLED,
    STR_THRESHOLD,
    STR_THRESHOLD_DEFAULT,   /* %u = ms */
    STR_BUTTONS,
    STR_BUTTON_LEFT,
    STR_BUTTON_RIGHT,
    STR_BUTTON_MIDDLE,
    STR_BUTTON_X1,
    STR_BUTTON_X2,
    STR_MODE,
    STR_MODE_BLOCK_PRESS,
    STR_MODE_HOLD_RELEASE,
    STR_AUTOSTART,
    STR_EXIT,
    STR_COUNT
};

/* Picks the language from the Windows display language. */
void strings_init(void);

const wchar_t *tr(enum str_id id);

#endif
