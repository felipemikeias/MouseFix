/*
 * main.c - MouseFix: filters phantom double clicks from faulty mice.
 *
 * Installs a WH_MOUSE_LL hook (which runs inside this process; no DLL is
 * injected into other programs), passes every button event to the filter in
 * debounce.c and shows a tray icon to configure and exit.
 *
 * Runs on Windows XP and later: only XP-era APIs are imported, and newer
 * tray features are used only when the running Windows supports them.
 */
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

#include "debounce.h"
#include "settings.h"
#include "strings.h"
#include "../res/resource.h"

#define APP_NAME L"MouseFix"
#define WINDOW_CLASS L"MouseFixWindow"
#define SINGLE_INSTANCE_MUTEX L"Local\\MouseFix.SingleInstance"

#define WM_TRAY (WM_APP + 1)
#define WM_SHOW_HINT (WM_APP + 2)  /* sent by a second instance */
#define TRAY_ID 1
#define TIMER_BASE 100  /* per-button timer: TIMER_BASE + db_button */

enum {
    CMD_ENABLED = 1,
    CMD_AUTOSTART,
    CMD_MODE_BLOCK_PRESS,
    CMD_MODE_HOLD_RELEASE,
    CMD_EXIT,
    CMD_THRESHOLD_BASE = 1000,  /* + ms */
    CMD_BUTTON_BASE = 2000      /* + db_button */
};

static const unsigned threshold_presets[] = {20, 30, 40, 50, 60, 80, 100, 150};

static HINSTANCE g_instance;
static HWND g_window;
static HHOOK g_hook;
static UINT g_taskbar_created;
static int g_vista_or_later;
static struct settings g_settings;
static struct db_state g_filter;
static unsigned long g_blocked;

static void inject(enum db_button b, int is_down)
{
    static const DWORD flags[DB_BUTTON_COUNT][2] = {
        {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_LEFTDOWN},
        {MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_RIGHTDOWN},
        {MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_MIDDLEDOWN},
        {MOUSEEVENTF_XUP, MOUSEEVENTF_XDOWN},
        {MOUSEEVENTF_XUP, MOUSEEVENTF_XDOWN}};
    INPUT in;

    ZeroMemory(&in, sizeof in);
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flags[b][is_down ? 1 : 0];
    if (b == DB_X1)
        in.mi.mouseData = XBUTTON1;
    else if (b == DB_X2)
        in.mi.mouseData = XBUTTON2;
    SendInput(1, &in, sizeof in);
}

/* Maps a hook message to button/direction. Returns 0 if it is not a button. */
static int classify(WPARAM msg, const MSLLHOOKSTRUCT *info,
                    enum db_button *b, int *is_down)
{
    switch (msg) {
    case WM_LBUTTONDOWN: *b = DB_LEFT;   *is_down = 1; return 1;
    case WM_LBUTTONUP:   *b = DB_LEFT;   *is_down = 0; return 1;
    case WM_RBUTTONDOWN: *b = DB_RIGHT;  *is_down = 1; return 1;
    case WM_RBUTTONUP:   *b = DB_RIGHT;  *is_down = 0; return 1;
    case WM_MBUTTONDOWN: *b = DB_MIDDLE; *is_down = 1; return 1;
    case WM_MBUTTONUP:   *b = DB_MIDDLE; *is_down = 0; return 1;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        *b = HIWORD(info->mouseData) == XBUTTON2 ? DB_X2 : DB_X1;
        *is_down = msg == WM_XBUTTONDOWN;
        return 1;
    }
    return 0;
}

static LRESULT CALLBACK mouse_hook(int code, WPARAM msg, LPARAM lparam)
{
    const MSLLHOOKSTRUCT *info = (const MSLLHOOKSTRUCT *)lparam;
    struct db_result r;
    enum db_button b;
    int is_down;

    /*
     * Injected events (from MouseFix itself or from other programs, such as
     * remote desktop and automation tools) do not come from the physical
     * button and are never filtered.
     */
    if (code != HC_ACTION || !g_settings.enabled ||
        (info->flags & LLMHF_INJECTED) || !classify(msg, info, &b, &is_down))
        return CallNextHookEx(g_hook, code, msg, lparam);

    r = db_on_event(&g_filter, b, is_down, info->time);

    if (r.cancel_timer)
        KillTimer(g_window, TIMER_BASE + b);
    if (r.start_timer)
        SetTimer(g_window, TIMER_BASE + b, g_filter.threshold_ms, NULL);
    if (r.inject_up)
        inject(b, 0);
    if (r.inject_down)
        inject(b, 1);
    if (is_down && r.block && !r.inject_down)
        g_blocked++;

    if (r.block)
        return 1;
    return CallNextHookEx(g_hook, code, msg, lparam);
}

/* Releases any button held back by the drag-safe mode. */
static void release_pending(void)
{
    unsigned mask = db_flush(&g_filter);
    int b;

    for (b = 0; b < DB_BUTTON_COUNT; b++) {
        KillTimer(g_window, TIMER_BASE + b);
        if (mask & (1u << b))
            inject((enum db_button)b, 0);
    }
}

static void apply_settings(void)
{
    release_pending();
    db_init(&g_filter, g_settings.threshold_ms, g_settings.mode,
            g_settings.buttons);
    settings_save(&g_settings);
}

static HICON load_icon(int id, int size_metric_x, int size_metric_y)
{
    return (HICON)LoadImageW(g_instance, MAKEINTRESOURCEW(id), IMAGE_ICON,
                             GetSystemMetrics(size_metric_x),
                             GetSystemMetrics(size_metric_y), LR_SHARED);
}

/*
 * Windows XP rejects the Vista-sized NOTIFYICONDATAW, so the structure size
 * is chosen at run time.
 */
static void init_tray_data(NOTIFYICONDATAW *nid)
{
    ZeroMemory(nid, sizeof *nid);
    nid->cbSize = g_vista_or_later ? sizeof *nid : NOTIFYICONDATAW_V3_SIZE;
    nid->hWnd = g_window;
    nid->uID = TRAY_ID;
}

static void update_tray(DWORD action)
{
    NOTIFYICONDATAW nid;

    init_tray_data(&nid);
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = load_icon(g_settings.enabled ? IDI_APP : IDI_PAUSED,
                          SM_CXSMICON, SM_CYSMICON);
    wsprintfW(nid.szTip, tr(g_settings.enabled ? STR_TIP_ACTIVE : STR_TIP_PAUSED),
              g_settings.threshold_ms);
    Shell_NotifyIconW(action, &nid);
}

/* Notification telling where MouseFix is, since it has no window. */
static void show_hint(void)
{
    NOTIFYICONDATAW nid;

    init_tray_data(&nid);
    nid.uFlags = NIF_INFO;
    if (g_vista_or_later) {
        nid.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
        nid.hBalloonIcon = load_icon(IDI_APP, SM_CXICON, SM_CYICON);
    } else {
        nid.dwInfoFlags = NIIF_INFO;
    }
    lstrcpynW(nid.szInfoTitle,
              tr(g_settings.enabled ? STR_HINT_TITLE_ACTIVE
                                    : STR_HINT_TITLE_PAUSED),
              sizeof nid.szInfoTitle / sizeof *nid.szInfoTitle);
    lstrcpynW(nid.szInfo, tr(STR_HINT_TEXT),
              sizeof nid.szInfo / sizeof *nid.szInfo);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void first_run(void)
{
    if (MessageBoxW(NULL, tr(STR_FIRST_RUN), APP_NAME,
                    MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDYES)
        autostart_set(1);
    settings_save(&g_settings);
}

static void show_menu(void)
{
    static const enum str_id button_names[DB_BUTTON_COUNT] = {
        STR_BUTTON_LEFT, STR_BUTTON_RIGHT, STR_BUTTON_MIDDLE, STR_BUTTON_X1,
        STR_BUTTON_X2};
    HMENU menu = CreatePopupMenu();
    HMENU thresholds = CreatePopupMenu();
    HMENU buttons = CreatePopupMenu();
    HMENU modes = CreatePopupMenu();
    wchar_t text[128];
    POINT pt;
    UINT i;

    for (i = 0; i < sizeof threshold_presets / sizeof *threshold_presets; i++) {
        unsigned ms = threshold_presets[i];
        wsprintfW(text, ms == MOUSEFIX_DEFAULT_THRESHOLD_MS
                            ? tr(STR_THRESHOLD_DEFAULT)
                            : L"%u ms",
                  ms);
        AppendMenuW(thresholds,
                    MF_STRING | (ms == g_settings.threshold_ms ? MF_CHECKED : 0),
                    CMD_THRESHOLD_BASE + ms, text);
    }

    for (i = 0; i < DB_BUTTON_COUNT; i++)
        AppendMenuW(buttons,
                    MF_STRING |
                        (g_settings.buttons & (1u << i) ? MF_CHECKED : 0),
                    CMD_BUTTON_BASE + i, tr(button_names[i]));

    AppendMenuW(modes,
                MF_STRING | (g_settings.mode == DB_MODE_BLOCK_PRESS ? MF_CHECKED : 0),
                CMD_MODE_BLOCK_PRESS, tr(STR_MODE_BLOCK_PRESS));
    AppendMenuW(modes,
                MF_STRING | (g_settings.mode == DB_MODE_HOLD_RELEASE ? MF_CHECKED : 0),
                CMD_MODE_HOLD_RELEASE, tr(STR_MODE_HOLD_RELEASE));

    wsprintfW(text, tr(STR_BLOCKED_COUNT), g_blocked);
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, text);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (g_settings.enabled ? MF_CHECKED : 0),
                CMD_ENABLED, tr(STR_ENABLED));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)thresholds, tr(STR_THRESHOLD));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)buttons, tr(STR_BUTTONS));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)modes, tr(STR_MODE));
    AppendMenuW(menu, MF_STRING | (autostart_get() ? MF_CHECKED : 0),
                CMD_AUTOSTART, tr(STR_AUTOSTART));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, CMD_EXIT, tr(STR_EXIT));

    GetCursorPos(&pt);
    /* Required so the menu closes when clicking outside of it. */
    SetForegroundWindow(g_window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_window, NULL);
    PostMessageW(g_window, WM_NULL, 0, 0);
    DestroyMenu(menu);  /* also destroys the submenus */
}

static void on_command(UINT id)
{
    if (id >= CMD_BUTTON_BASE && id < CMD_BUTTON_BASE + DB_BUTTON_COUNT) {
        g_settings.buttons ^= 1u << (id - CMD_BUTTON_BASE);
    } else if (id > CMD_THRESHOLD_BASE && id < CMD_BUTTON_BASE) {
        g_settings.threshold_ms = id - CMD_THRESHOLD_BASE;
    } else {
        switch (id) {
        case CMD_ENABLED:
            g_settings.enabled = !g_settings.enabled;
            break;
        case CMD_MODE_BLOCK_PRESS:
            g_settings.mode = DB_MODE_BLOCK_PRESS;
            break;
        case CMD_MODE_HOLD_RELEASE:
            g_settings.mode = DB_MODE_HOLD_RELEASE;
            break;
        case CMD_AUTOSTART:
            autostart_set(!autostart_get());
            return;
        case CMD_EXIT:
            DestroyWindow(g_window);
            return;
        default:
            return;
        }
    }
    apply_settings();
    update_tray(NIM_MODIFY);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam)
{
    switch (msg) {
    case WM_TRAY:
        if (lparam == WM_RBUTTONUP || lparam == WM_LBUTTONUP)
            show_menu();
        return 0;
    case WM_COMMAND:
        on_command(LOWORD(wparam));
        return 0;
    case WM_SHOW_HINT:
        show_hint();
        return 0;
    case WM_TIMER:
        if (wparam >= TIMER_BASE && wparam < TIMER_BASE + DB_BUTTON_COUNT) {
            enum db_button b = (enum db_button)(wparam - TIMER_BASE);
            KillTimer(hwnd, wparam);
            if (db_on_timer(&g_filter, b))
                inject(b, 0);
        }
        return 0;
    case WM_DESTROY:
        release_pending();
        update_tray(NIM_DELETE);
        PostQuitMessage(0);
        return 0;
    }
    if (g_taskbar_created && msg == g_taskbar_created) {
        /* Explorer restarted: the tray icon has to be added again. */
        update_tray(NIM_ADD);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show)
{
    WNDCLASSW wc;
    HANDLE mutex;
    MSG msg;

    (void)prev;
    (void)cmdline;
    (void)show;
    g_instance = instance;
    strings_init();

    mutex = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Already running: the existing instance shows where its icon is. */
        HWND running = FindWindowW(WINDOW_CLASS, NULL);
        if (running)
            PostMessageW(running, WM_SHOW_HINT, 0, 0);
        return 0;
    }

    /* GetTickCount64 exists from Vista on; checked without importing it. */
    g_vista_or_later =
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTickCount64") != NULL;

    settings_load(&g_settings);
    db_init(&g_filter, g_settings.threshold_ms, g_settings.mode,
            g_settings.buttons);

    ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = WINDOW_CLASS;
    if (!RegisterClassW(&wc))
        return 1;

    /* Never shown; exists to receive tray messages and timers. */
    g_window = CreateWindowExW(WS_EX_TOOLWINDOW, WINDOW_CLASS, APP_NAME, 0, 0,
                               0, 0, 0, NULL, NULL, instance, NULL);
    if (!g_window)
        return 1;

    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    g_hook = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook, instance, 0);
    if (!g_hook) {
        MessageBoxW(NULL, tr(STR_HOOK_FAILED), APP_NAME, MB_ICONERROR);
        DestroyWindow(g_window);
        return 1;
    }

    update_tray(NIM_ADD);

    /* Refreshes the path in case the .exe was moved to another folder. */
    if (autostart_get())
        autostart_set(1);

    if (g_settings.first_run)
        first_run();
    if (!wcsstr(GetCommandLineW(), AUTOSTART_ARG))
        show_hint();

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(g_hook);
    if (mutex)
        CloseHandle(mutex);
    return (int)msg.wParam;
}
