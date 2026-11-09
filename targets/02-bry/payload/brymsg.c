#include <windows.h>
int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR cmd, int n){
    (void)a;(void)b;(void)cmd;(void)n;
    const char *body =
        "NATIVE CODE EXECUTION inside the BRy signing host.\r\n\r\n"
        "An attacker-controlled DLL was loaded by BryNativeModule.exe because a\r\n"
        "WEB PAGE sent a sign message with an attacker-chosen modulePath:\r\n"
        "    sign -> modulePath -> LoadLibraryExA  (blind, no signature check).\r\n\r\n"
        "No cert, no token, no smartcard. Full remote code execution.\r\n"
        "Benign PoC: marker files + this dialog + the launched video.";
    MessageBoxA(NULL, body, "BRy Assinatura - arbitrary native code executed",
                MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
    return 0;
}
