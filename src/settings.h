/*
 * settings.h - configurações persistidas em HKCU\Software\MouseFix e
 * inicialização automática via HKCU\...\CurrentVersion\Run.
 */
#ifndef MOUSEFIX_SETTINGS_H
#define MOUSEFIX_SETTINGS_H

#include "debounce.h"

#define MOUSEFIX_DEFAULT_THRESHOLD_MS 40   /* mesmo valor do original */

struct settings {
    unsigned threshold_ms;
    enum db_mode mode;
    unsigned buttons;       /* máscara de enum db_button */
    int enabled;
    int first_run;          /* nenhuma configuração salva ainda */
};

void settings_load(struct settings *s);
void settings_save(const struct settings *s);

/* Argumento passado pela entrada de início automático. */
#define AUTOSTART_ARG L"/autostart"

int autostart_get(void);
void autostart_set(int on);

#endif
