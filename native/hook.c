// native/hook.c
// Roblox function hooking

#include <windows.h>
#include <stdio.h>

#define LOG(fmt, ...) do { \
    char buf[512]; \
    snprintf(buf, sizeof(buf), "[HOOK] " fmt "\n", ##__VA_ARGS__); \
    OutputDebugStringA(buf); \
} while(0)

// ============================================================
// TRAMPOLINE HOOK (x64)
// ============================================================

typedef struct {
    BYTE original[14];
    BYTE patch[14];
    LPVOID target;
    LPVOID detour;
} HookEntry;

static HookEntry g_hooks[64];
static int g_hookCount = 0;

BOOL install_hook(LPVOID target, LPVOID detour, HookEntry* entry) {
    if (g_hookCount >= 64) return FALSE;
    
    memcpy(entry->original, target, 14);
    entry->target = target;
    entry->detour = detour;
    
    // mov rax, detour
    entry->patch[0] = 0x48;
    entry->patch[1] = 0xB8;
    *(LPVOID*)&entry->patch[2] = detour;
    // jmp rax
    entry->patch[10] = 0xFF;
    entry->patch[11] = 0xE0;
    entry->patch[12] = 0x90;
    entry->patch[13] = 0x90;
    
    DWORD oldProtect;
    if (!VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LOG("VirtualProtect failed");
        return FALSE;
    }
    
    memcpy(target, entry->patch, 14);
    VirtualProtect(target, 14, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 14);
    
    g_hooks[g_hookCount++] = *entry;
    return TRUE;
}

void remove_all_hooks() {
    DWORD oldProtect;
    for (int i = 0; i < g_hookCount; i++) {
        VirtualProtect(g_hooks[i].target, 14, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(g_hooks[i].target, g_hooks[i].original, 14);
        VirtualProtect(g_hooks[i].target, 14, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), g_hooks[i].target, 14);
    }
    g_hookCount = 0;
}
