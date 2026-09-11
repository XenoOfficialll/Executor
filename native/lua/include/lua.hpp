-- lua/include/lua.hpp
-- LuaJIT C++ header wrapper
-- This file is a wrapper around lua.h, lauxlib.h, and lualib.h

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
