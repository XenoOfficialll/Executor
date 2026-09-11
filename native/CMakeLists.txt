// native/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(roblox_executor_native)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2")

# LuaJIT library
set(LUA_DIR "${CMAKE_SOURCE_DIR}/../lua")
include_directories(${LUA_DIR}/include)
link_directories(${LUA_DIR}/lib)

add_library(injector SHARED
    injector.c
    hook.c
    memory.c
)

target_link_libraries(injector
    lua51
    user32
    kernel32
    psapi
)

set_target_properties(injector PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
