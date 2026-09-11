// native/memory.c
// Memory scanning and manipulation

#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define LOG(fmt, ...) do { \
    char buf[512]; \
    snprintf(buf, sizeof(buf), "[MEM] " fmt "\n", ##__VA_ARGS__); \
    OutputDebugStringA(buf); \
} while(0)

// ============================================================
// PATTERN SCANNER
// ============================================================

typedef struct {
    BYTE* pattern;
    BYTE* mask;
    SIZE_T length;
    const char* name;
} Signature;

static Signature g_signatures[] = {
    { (BYTE*)"\x48\x8B\x0D\x00\x00\x00\x00\x48\x85\xC9\x74", 
      (BYTE*)"xxx????xxxx", 11, "lua_state_getter" },
    { (BYTE*)"\x48\x8B\x05\x00\x00\x00\x00\x48\x8B\x88", 
      (BYTE*)"xxx????xxx", 11, "script_context" },
    { (BYTE*)"\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8", 
      (BYTE*)"xxxxxxxxxx", 10, "get_global_state" },
};
static const int g_sigCount = sizeof(g_signatures) / sizeof(Signature);

BOOL pattern_match(BYTE* data, BYTE* pattern, BYTE* mask, SIZE_T length) {
    for (SIZE_T i = 0; i < length; i++) {
        if (mask[i] == 'x' && data[i] != pattern[i]) return FALSE;
    }
    return TRUE;
}

LPVOID scan_module(HMODULE module, Signature* sig) {
    MODULEINFO info;
    if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info))) {
        return NULL;
    }
    
    BYTE* base = (BYTE*)info.lpBaseOfDll;
    SIZE_T size = info.SizeOfImage;
    
    for (SIZE_T i = 0; i < size - sig->length; i++) {
        if (pattern_match(base + i, sig->pattern, sig->mask, sig->length)) {
            LOG("Found %s at %p", sig->name, base + i);
            return base + i;
        }
    }
    return NULL;
}

LPVOID find_signature(const char* name) {
    HMODULE module = GetModuleHandleA(NULL);
    for (int i = 0; i < g_sigCount; i++) {
        if (strcmp(g_signatures[i].name, name) == 0) {
            return scan_module(module, &g_signatures[i]);
        }
    }
    return NULL;
}
