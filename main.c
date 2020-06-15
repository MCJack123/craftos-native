#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "bit.h"
#include "fs.h"
#include "os.h"
#include "term.h"
#include "redstone.h"
#include "keys.h"

int main() {
    int status, result, i;
    double sum;
    lua_State *L;
    lua_State *coro;
start:
    /*
     * All Lua contexts are held in this structure. We work with it almost
     * all the time.
     */
    L = luaL_newstate();

    coro = lua_newthread(L);

    luaL_openlibs(coro); /* Load Lua libraries */
    load_library(coro, bit_lib);
    load_library(coro, fs_lib);
    load_library(coro, os_lib);
    load_library(coro, rs_lib);
    lua_getglobal(coro, "redstone");
    lua_setglobal(coro, "rs");
    load_library(coro, term_lib);
    termInit();
    initKeys();

    lua_pushstring(L, "");
    lua_setglobal(L, "_CC_DEFAULT_SETTINGS");
    lua_pushstring(L, "ComputerCraft 1.80 (craftos-native)");
    lua_setglobal(L, "_HOST");

    /* Set up pcall fix */
    status = luaL_loadstring(L, "_G.xpcall = function( _fn, _fnErrorHandler )\n\
    local typeT = type( _fn )\n\
    assert( typeT == \"function\", \"bad argument #1 to xpcall (function expected, got \"..typeT..\")\" )\n\
    local co = coroutine.create( _fn )\n\
    local tResults = { coroutine.resume( co ) }\n\
    while coroutine.status( co ) ~= \"dead\" do\n\
        tResults = { coroutine.resume( co, coroutine.yield() ) }\n\
    end\n\
    if tResults[1] == true then\n\
        return true, unpack( tResults, 2 )\n\
    else\n\
        return false, _fnErrorHandler( tResults[2] )\n\
    end\n\
end\n\
\n\
_G.pcall = function( _fn, ... )\n\
    local typeT = type( _fn )\n\
    assert( typeT == \"function\", \"bad argument #1 to pcall (function expected, got \"..typeT..\")\" )\n\
    local tArgs = { ... }\n\
    return xpcall(\n\
        function()\n\
            return _fn( unpack( tArgs ) )\n\
        end,\n\
        function( _error )\n\
            return _error\n\
        end\n\
    )\n\
end");
    if (status) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        fprintf(stderr, "Couldn't load pcall fix: %s\n", lua_tostring(L, -1));
        exit(1);
    }
    lua_call(L, 0, 0);

    /* Load the file containing the script we are going to run */
    status = luaL_loadfile(coro, "/bios.lua");
    if (status) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
        exit(1);
    }

    /* Ask Lua to run our little script */
    status = LUA_YIELD;
    int narg = 0;
    while (status == LUA_YIELD && running == 1) {
        status = lua_resume(coro, narg);
        if (status == LUA_YIELD) {
            if (lua_isstring(coro, -1)) narg = getNextEvent(coro, lua_tostring(coro, -1));
            else narg = getNextEvent(coro, "");
        } else if (status != 0) {
            usleep(5000000);
            termClose();
            printf("%s\n", lua_tostring(coro, -1));
            lua_close(L);
            exit(1);
        }
    }

    termClose();
    closeKeys();
    lua_close(L);   /* Cya, Lua */

    if (running == 2) {
        usleep(1000000);
        goto start;
    }

    return 0;
}
