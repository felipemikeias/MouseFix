# Analysis of the original MouseFix

Reverse engineering of the `MouseFix.exe` + `MouseFixDll.dll` pair that
circulates online as a double-click fix. The goal was to understand exactly
what it does and to check whether it did anything else. No code or binary
from the original is part of this repository.

## Files analyzed

| File              | Size     | SHA-256                                                            |
| ----------------- | -------- | ------------------------------------------------------------------ |
| `MouseFix.exe`    | 40,960 B | `98bb12182d9e61918fd96813c1644e29a6125c86ee43ee28c43acac86de14efa` |
| `MouseFixDll.dll` | 45,056 B | `cd168c8b04f1c637834c975370bc780f93676fab0698e62be07e1e30446904bc` |

Both are PE32 (x86, 32-bit) files from 2004-01-29, built with Visual C++ 6
and a static CRT. They have no digital signature and no ASLR/DEP
(`DllCharacteristics = 0`). If your files have a different hash, this analysis
does not apply to them.

Tools: `objdump` (headers, imports, exports and disassembly). Almost all of
the code in both files is the Visual C++ runtime; the program's own code is
about 0x2F0 bytes in the `.exe` and 0x60 bytes in the DLL.

## MouseFixDll.dll

Exports two functions and has an 8-byte shared section `.SHARDAT` (two
variables visible to every process that loads the DLL).

The only system function the DLL imports, besides the CRT, is
`USER32!CallNextHookEx`.

Reconstruction from the disassembly (`0x10001010` and `0x10001020`):

```c
#pragma data_seg(".SHARDAT")
HHOOK g_hook     = 0;   /* 0x1000a000 */
DWORD g_lastUpMs = 0;   /* 0x1000a004 */
#pragma data_seg()

BOOL SetHook(HHOOK h)                       /* exported as "SetHook" */
{
    g_hook = h;
    return TRUE;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{                                           /* "_LowLevelMouseProc@12" */
    if (nCode == HC_ACTION) {
        DWORD t = ((MSLLHOOKSTRUCT *)lParam)->time;
        if (wParam == WM_LBUTTONDOWN) {
            if (t - g_lastUpMs < 40)        /* cmp eax,0x28 ; jae */
                return 1;                   /* drop the click */
        } else if (wParam == WM_LBUTTONUP) {
            g_lastUpMs = t;
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}
```

## MouseFix.exe

The program only loads the DLL, installs the hook and waits:

1. `RegisterClassA("MouseFixClass")` and `CreateWindowExA` of a window that
   is hidden right away (`ShowWindow(SW_HIDE)`).
2. On `WM_CREATE`: `LoadLibraryA("MouseFixDll.dll")`, `GetProcAddress` of
   `_LowLevelMouseProc@12` and `SetHook`, then
   `SetWindowsHookExA(WH_MOUSE_LL, proc, hDll, 0)` and `SetHook(hHook)`.
   If anything fails, it shows a `MessageBox` and closes.
3. Message loop. The Esc key with the window focused (it is never visible)
   sends `WM_CLOSE`.
4. On exit: `UnhookWindowsHookEx`, `SetHook(NULL)`, `FreeLibrary`.

Relevant imports: `LoadLibraryA`, `GetProcAddress`, `FreeLibrary`,
`SetWindowsHookExA`, `UnhookWindowsHookEx`, and window/message functions from
`USER32` and `GDI32`.

## Security assessment

**Nothing malicious was found.** There are no imports or code for network,
registry, files (other than the CRT's error output), processes, code
injection or persistence.

Weak points, though not malicious:

- **DLL loaded by relative name.** `LoadLibraryA("MouseFixDll.dll")` follows
  the Windows DLL search order, so another DLL with that name in a folder
  searched earlier would be loaded instead (*DLL hijacking*).
- **Unsigned binaries from 2004 without ASLR/DEP**, distributed by third-party
  sites. There is no way to know whether a downloaded copy is the original
  without comparing hashes.
- **Unnecessary DLL.** `WH_MOUSE_LL` hooks are not injected into other
  processes; Windows calls the function in the process that installed the
  hook. A DLL with a shared section is a pattern for global hooks
  (`WH_MOUSE`) and adds nothing here.

## Behavior and limitations of the original

- Filters only the left button, with a fixed 40 ms interval.
- When it drops a press, it lets the next release through. Programs receive a
  release without a matching press.
- If the fault happens during a drag (a quick release–press in the middle of
  the movement), the phantom release gets through and the drag is broken.
- No icon, no visible window and no way to close it other than Task Manager.
  No protection against running twice.

## What this version keeps and changes

| Aspect            | Original                     | This version                          |
| ----------------- | ---------------------------- | ------------------------------------- |
| Blocking rule     | press < 40 ms after release  | Same in the default mode              |
| Orphan release    | Passes through               | Dropped together with the press       |
| Broken drags      | Not handled                  | Drag-safe mode                        |
| Buttons           | Left                         | Configurable (5 buttons)              |
| Interval          | Fixed 40 ms                  | Configurable                          |
| Injected events   | Filtered too                 | Never filtered                        |
| Structure         | `.exe` + DLL                 | A single `.exe`                       |
| Interface         | None                         | Tray icon with menu                   |
