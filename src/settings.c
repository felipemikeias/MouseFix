#include "settings.h"

#include <windows.h>

#define SETTINGS_KEY L"Software\\MouseFix"
#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_VALUE L"MouseFix"

#define MIN_THRESHOLD_MS 5
#define MAX_THRESHOLD_MS 500

static DWORD read_dword(HKEY key, const wchar_t *name, DWORD fallback)
{
    DWORD value, size = sizeof value, type;

    if (RegQueryValueExW(key, name, NULL, &type, (BYTE *)&value, &size) !=
            ERROR_SUCCESS || type != REG_DWORD)
        return fallback;
    return value;
}

static void write_dword(HKEY key, const wchar_t *name, DWORD value)
{
    RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE *)&value,
                   sizeof value);
}

void settings_load(struct settings *s)
{
    HKEY key;

    s->threshold_ms = MOUSEFIX_DEFAULT_THRESHOLD_MS;
    s->mode = DB_MODE_BLOCK_PRESS;
    s->buttons = 1u << DB_LEFT;
    s->enabled = 1;
    s->first_run = 1;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &key) !=
            ERROR_SUCCESS)
        return;
    s->first_run = 0;

    s->threshold_ms = read_dword(key, L"ThresholdMs", s->threshold_ms);
    s->mode = read_dword(key, L"Mode", s->mode) == DB_MODE_HOLD_RELEASE
                  ? DB_MODE_HOLD_RELEASE
                  : DB_MODE_BLOCK_PRESS;
    s->buttons = read_dword(key, L"Buttons", s->buttons) &
                 ((1u << DB_BUTTON_COUNT) - 1);
    s->enabled = read_dword(key, L"Enabled", s->enabled) != 0;
    RegCloseKey(key);

    if (s->threshold_ms < MIN_THRESHOLD_MS)
        s->threshold_ms = MIN_THRESHOLD_MS;
    if (s->threshold_ms > MAX_THRESHOLD_MS)
        s->threshold_ms = MAX_THRESHOLD_MS;
}

void settings_save(const struct settings *s)
{
    HKEY key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, NULL, 0,
                        KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
        return;
    write_dword(key, L"ThresholdMs", s->threshold_ms);
    write_dword(key, L"Mode", s->mode);
    write_dword(key, L"Buttons", s->buttons);
    write_dword(key, L"Enabled", (DWORD)s->enabled);
    RegCloseKey(key);
}

int autostart_get(void)
{
    HKEY key;
    LONG rc;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &key) !=
            ERROR_SUCCESS)
        return 0;
    rc = RegQueryValueExW(key, RUN_VALUE, NULL, NULL, NULL, NULL);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

void autostart_set(int on)
{
    wchar_t path[MAX_PATH], command[MAX_PATH + 16];
    HKEY key;
    DWORD len;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_WRITE, &key) !=
            ERROR_SUCCESS)
        return;

    if (!on) {
        RegDeleteValueW(key, RUN_VALUE);
    } else {
        len = GetModuleFileNameW(NULL, path, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            /* Entre aspas: o caminho pode conter espaços. */
            wsprintfW(command, L"\"%s\" " AUTOSTART_ARG, path);
            RegSetValueExW(key, RUN_VALUE, 0, REG_SZ, (const BYTE *)command,
                           (DWORD)((lstrlenW(command) + 1) * sizeof(wchar_t)));
        }
    }
    RegCloseKey(key);
}
