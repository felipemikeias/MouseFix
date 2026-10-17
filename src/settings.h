/*
 * settings.h - settings stored in HKCU\Software\MouseFix and start with
 * Windows through HKCU\...\CurrentVersion\Run.
 */
#ifndef MOUSEFIX_SETTINGS_H
#define MOUSEFIX_SETTINGS_H

#include "debounce.h"

#define MOUSEFIX_DEFAULT_THRESHOLD_MS 40   /* same value as the original */

struct settings {
    unsigned threshold_ms;
    enum db_mode mode;
    unsigned buttons;       /* mask of enum db_button */
    int enabled;
    int first_run;          /* no settings saved yet */
};

void settings_load(struct settings *s);
void settings_save(const struct settings *s);

/* Argument passed by the start-with-Windows entry. */
#define AUTOSTART_ARG L"/autostart"

int autostart_get(void);
void autostart_set(int on);

#endif
