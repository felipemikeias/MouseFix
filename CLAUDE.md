# CLAUDE.md

Guidance for agents working in this repository.

## What it is

A Win32 utility in C that filters the phantom double click of faulty mice
through a `WH_MOUSE_LL` hook. It is an open-source reimplementation of the old
MouseFix (2004); the analysis of the original is in
`docs/original-analysis.md`.

## Layout and responsibilities

- `src/debounce.c/.h`: all decision logic (block, hold back, inject).
  **Must not include `windows.h`** or call the system: it receives events and
  returns actions in `struct db_result`. Any change to filter behavior starts
  here, with a test in `tests/test_debounce.c`.
- `src/main.c`: hook, tray icon, menu, timers and `SendInput`. It only
  translates events for the filter and carries out what it returns.
- `src/settings.c/.h`: reads/writes `HKCU\Software\MouseFix` and the
  `MouseFix` value in `HKCU\...\CurrentVersion\Run`.
- `src/strings.c/.h`: every user-facing string, English (default) and
  Portuguese (picked when the Windows display language is Portuguese). New
  text goes into both tables, in the same order as `enum str_id`.
- `res/`: icons, manifest (`asInvoker`, DPI) and `VERSIONINFO`. The version
  appears in `CMakeLists.txt`, `res/mousefix.rc` and `res/mousefix.manifest`;
  change all three together. `mousefix.ico` is the icon of the `.exe` and the
  tray; its entries below 256 px must be BMP (Windows XP cannot read PNG icon
  entries). `docs/icon.png` is the 128 px copy used in the README.
- `cmake/mingw-i686.cmake`: toolchain for the release build.

## Commands

Filter tests (Linux/macOS/Windows):

```bash
cc -std=c99 -Wall -Wextra -pedantic -Isrc src/debounce.c tests/test_debounce.c -o /tmp/test_debounce && /tmp/test_debounce
```

Release build (MinGW-w64, Windows XP and later):

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release -DMOUSEFIX_TESTS=OFF && cmake --build build
```

Visual Studio build (Windows 7 and later), used by CI to keep MSVC working:

```bash
cmake -B build && cmake --build build --config Release && ctest --test-dir build -C Release
```

End-to-end testing without Windows: Wine inside a nested X server (Xephyr), a
helper program that logs `WM_LBUTTONDOWN/UP`, and `xdotool` sending sequences
such as `mousedown 1 sleep 0.08 mouseup 1 sleep 0.01 mousedown 1`. Always use
a separate display so the user's real mouse is not moved. Wine can report
itself as Windows XP (`HKCU\Software\Wine`, value `Version` = `winxp`), but
it implements every API regardless, so XP compatibility is checked through
the imports instead (see below).

## Windows XP compatibility

- Only call functions that exist on Windows XP. Anything newer must be loaded
  with `GetProcAddress` and have a fallback.
- `NOTIFYICONDATAW` is sized at run time (`init_tray_data`): XP rejects the
  Vista-sized structure. Vista-only fields are set only when
  `g_vista_or_later` is true.
- The release build must import only `ADVAPI32`, `KERNEL32`, `msvcrt`,
  `SHELL32` and `USER32`, with OS/subsystem version 5.1 in the PE header. CI
  fails otherwise; after adding a Win32 call, check it exists on XP.

## Releasing

Update the version in the three files above and push a `vX.Y.Z` tag.
`.github/workflows/build.yml` builds with MinGW and MSVC, runs the tests and
the XP check, and on tags publishes a release with **a single `MouseFix.exe`
(32-bit MinGW build)**, the text of `.github/release-notes.md` and its
SHA-256. Never publish more than one `.exe`: users are non-technical and
should not have to choose. Release notes are in English followed by a
Portuguese translation.

## Project rules

- No dependencies beyond the Windows API; a single `.exe`, no DLL.
- Never ask for administrator rights or touch the network or files.
- Events flagged `LLMHF_INJECTED` are never filtered.
- The hook must be fast (Windows removes slow hooks): no I/O, allocation or
  blocking calls inside `mouse_hook`.
- When pausing, changing settings or exiting, call `release_pending()` so no
  button stays stuck in drag-safe mode.
- Non-technical users: there is no window, so every manual launch shows a
  notification saying where the icon is; the first run asks about starting
  with Windows; launching a second time notifies the running instance instead
  of showing an error. Start with Windows passes `/autostart` and stays
  silent.
- Code, comments, documentation and commit messages in English. Sources are
  UTF-8 (CMake passes `/utf-8` to MSVC); the `.rc` file must stay ASCII.
- C99, matching the existing style: 4 spaces, function braces on their own
  line, `db_` prefix in the filter and `g_` for globals.

## Outside the repository

The icon generator (Python + Pillow) and the Wine test scripts live in
`../MouseFix-extras/`, outside the repository.

The original binaries (`MouseFix.exe`, `MouseFixDll.dll`) and their
disassembly dumps are kept outside this folder and must **never** be added to
the repository (unknown license).
