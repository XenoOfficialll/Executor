#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        // Initialize Lua VM
        // Hook Roblox functions
        // Start command listener
    }
    return TRUE;
}
