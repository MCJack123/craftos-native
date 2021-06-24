#include "lib.h"

void load_library(lua_State *L, library_t lib) {
    int i;
    lua_newtable(L);
    for (i = 0; i < lib.count; i++) {
        lua_pushstring(L, lib.keys[i]);
        lua_pushcfunction(L, lib.values[i]);
        lua_settable(L, -3);
    }
    lua_setglobal(L, lib.name);
}
