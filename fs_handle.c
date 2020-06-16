#include "fs_handle.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int handle_close(lua_State *L) {
    fclose((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    return 0;
}

char checkChar(char c) {
    if (c == 127) return '?';
    if ((c >= 32 && c < 127) || c == '\n' || c == '\t' || c == '\r') return c;
    else return '?';
}

int handle_readAll(lua_State *L) {
    long pos, size;
    char * retval;
    int i;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    size = ftell(fp) - pos;
    retval = (char*)malloc(size + 1);
    memset(retval, 0, size + 1);
    fseek(fp, pos, SEEK_SET);
    for (i = 0; !feof(fp) && i < size; i++)
        retval[i] = checkChar(fgetc(fp));
    lua_pushstring(L, retval);
    return 1;
}

int handle_readLine(lua_State *L) {
    int i;
    char * retval;
    long size;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    size = 0;
    while (fgetc(fp) != '\n' && !feof(fp)) size++;
    fseek(fp, 0 - size - 1, SEEK_CUR);
    retval = (char*)malloc(size + 1);
    for (i = 0; i < size; i++) retval[i] = checkChar(fgetc(fp));
    fgetc(fp);
    lua_pushstring(L, retval);
    return 1;
}

int handle_readChar(lua_State *L) {
    char retval[2];
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    retval[0] = checkChar(fgetc(fp));
    lua_pushstring(L, retval);
    return 1;
}

int handle_readByte(lua_State *L) {
    char retval;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    retval = fgetc(fp);
    lua_pushinteger(L, retval);
    return 1;
}

int handle_writeString(lua_State *L) {
    FILE * fp;
    const char * str = lua_tostring(L, 1);
    fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    return 0;
}

int handle_writeLine(lua_State *L) {
    FILE * fp;
    const char * str = lua_tostring(L, 1);
    fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    fputc('\n', fp);
    return 0;
}

int handle_writeByte(lua_State *L) {
    FILE * fp;
    const char b = lua_tointeger(L, 1) & 0xFF;
    fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fputc(b, fp);
    return 0;
}

int handle_flush(lua_State *L) {
    fflush((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    return 0;
}