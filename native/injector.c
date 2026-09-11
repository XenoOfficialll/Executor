// native/injector.c
// Roblox DLL Injector - Native implementation
// Compile with: cl /LD injector.c /Fe:injector.dll

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

#define LOG(fmt, ...) do { \
    char buf[512]; \
    snprintf(buf, sizeof(buf), "[INJECTOR] " fmt "\n", ##__VA_ARGS__); \
    OutputDebugStringA(buf); \
} while(0)

static HMODULE g_hModule = NULL;

// ============================================================
// LUA VM INTEGRATION
// ============================================================

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State*);

typedef struct {
    lua_State* (*luaL_newstate)(void);
    void (*luaL_openlibs)(lua_State*);
    int (*luaL_loadstring)(lua_State*, const char*);
    int (*lua_pcall)(lua_State*, int, int, int);
    void (*lua_close)(lua_State*);
    const char* (*lua_tostring)(lua_State*, int);
    void (*lua_settop)(lua_State*, int);
} LuaAPI;

static LuaAPI g_lua = {0};
static HMODULE g_luaModule = NULL;
static lua_State* g_luaState = NULL;

BOOL load_lua() {
    g_luaModule = LoadLibraryA("lua51.dll");
    if (!g_luaModule) {
        LOG("Failed to load lua51.dll");
        return FALSE;
    }
    
    g_lua.luaL_newstate = (void*)GetProcAddress(g_luaModule, "luaL_newstate");
    g_lua.luaL_openlibs = (void*)GetProcAddress(g_luaModule, "luaL_openlibs");
    g_lua.luaL_loadstring = (void*)GetProcAddress(g_luaModule, "luaL_loadstring");
    g_lua.lua_pcall = (void*)GetProcAddress(g_luaModule, "lua_pcall");
    g_lua.lua_close = (void*)GetProcAddress(g_luaModule, "lua_close");
    g_lua.lua_tostring = (void*)GetProcAddress(g_luaModule, "lua_tostring");
    g_lua.lua_settop = (void*)GetProcAddress(g_luaModule, "lua_settop");
    
    return g_lua.luaL_newstate != NULL;
}

// ============================================================
// COMMAND LISTENER
// ============================================================

#define PIPE_NAME "\\\\.\\pipe\\roblox_executor"
#define BUFFER_SIZE 65536

static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static volatile BOOL g_running = TRUE;

DWORD WINAPI command_listener(LPVOID param) {
    char buffer[BUFFER_SIZE];
    DWORD bytesRead;
    
    while (g_running) {
        g_pipe = CreateNamedPipeA(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            BUFFER_SIZE, BUFFER_SIZE, 0, NULL
        );
        
        if (g_pipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }
        
        if (ConnectNamedPipe(g_pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            while (g_running) {
                if (!ReadFile(g_pipe, buffer, BUFFER_SIZE - 1, &bytesRead, NULL)) {
                    break;
                }
                buffer[bytesRead] = '\0';
                
                if (strcmp(buffer, "EXIT") == 0) {
                    g_running = FALSE;
                    break;
                }
                
                // Execute Lua code
                if (g_luaState) {
                    int status = g_lua.luaL_loadstring(g_luaState, buffer);
                    if (status == 0) {
                        status = g_lua.lua_pcall(g_luaState, 0, 0, 0);
                    }
                    if (status != 0) {
                        const char* err = g_lua.lua_tostring(g_luaState, -1);
                        LOG("Lua error: %s", err ? err : "unknown");
                        g_lua.lua_settop(g_luaState, -2);
                    }
                }
            }
            DisconnectNamedPipe(g_pipe);
        }
        CloseHandle(g_pipe);
    }
    
    return 0;
}

// ============================================================
// DLL ENTRY POINT
// ============================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hModule;
            DisableThreadLibraryCalls(hModule);
            
            LOG("DLL attached");
            
            if (!load_lua()) {
                LOG("Lua load failed");
                return FALSE;
            }
            
            g_luaState = g_lua.luaL_newstate();
            if (!g_luaState) {
                LOG("luaL_newstate failed");
                return FALSE;
            }
            g_lua.luaL_openlibs(g_luaState);
            
            CreateThread(NULL, 0, command_listener, NULL, 0, NULL);
            LOG("Executor ready");
            break;
            
        case DLL_PROCESS_DETACH:
            g_running = FALSE;
            if (g_luaState) {
                g_lua.lua_close(g_luaState);
                g_luaState = NULL;
            }
            if (g_luaModule) {
                FreeLibrary(g_luaModule);
                g_luaModule = NULL;
            }
            LOG("DLL detached");
            break;
    }
    return TRUE;
}
