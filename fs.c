#include "fs.h"
#include "fs_handle.h"
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <libgen.h>
#include <errno.h>
#include <glob.h>
#include <dirent.h>
#define true 1
#define false 0
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

char * fixpath(const char * path) {
    char * retval = (char*)malloc(strlen(path) + 2);
    if (path[0] != '/') {
        retval[0] = '/';
        strcpy(&retval[1], path);
    } else strcpy(retval, path);
    return retval;
}

void err(lua_State *L, char * path, const char * err) {
    char * msg = (char*)malloc(strlen(path) + strlen(err) + 3);
    sprintf(msg, "%s: %s", path, err);
    free(path);
    lua_pushstring(L, msg);
    lua_error(L);
}

char * unconst(const char * str) {
    char * retval = malloc(strlen(str) + 1);
    strcpy(retval, str);
    return retval;
}

int fs_list(lua_State *L) {
    struct dirent *dir;
    int i;
    DIR * d;
    char * path = fixpath(lua_tostring(L, 1));
    d = opendir(path);
    if (d) {
        lua_newtable(L);
        for (i = 0; (dir = readdir(d)) != NULL; i++) {
            lua_pushinteger(L, i);
            lua_pushstring(L, dir->d_name);
            lua_settable(L, -3);
        }
        closedir(d);
    } else err(L, path, "Not a directory");
    free(path);
    return 1;
}

int fs_exists(lua_State *L) {
    struct stat st;
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, stat(path, &st) == 0);
    free(path);
    return 1;
}

int fs_isDir(lua_State *L) {
    struct stat st;
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    free(path);
    return 1;
}

int fs_isReadOnly(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, access(path, W_OK) != 0);
    free(path);
    return 1;
}

int fs_getName(lua_State *L) {
    char * path = unconst(lua_tostring(L, 1));
    lua_pushstring(L, basename(path));
    free(path);
    return 1;
}

#define ERROR 0
#define SUCCESS 1

int getmntpt(char *path, char *mount_point) {
    struct stat cur_stat;
    struct stat last_stat;

    char dir_name[PATH_MAX];
    char *dirname_p = dir_name;
    char cur_cwd[255];
    char *cur_cwd_p = cur_cwd;
    char saved_cwd[PATH_MAX];
    size_t path_len, suffix_len, dir_len;
    if (getcwd(saved_cwd, PATH_MAX) == NULL) {
        errno = EIO;
        return ERROR;
    }

    if (lstat(path, &cur_stat) < 0) {
        errno = EIO;
        return ERROR;
    }

    if (S_ISDIR (cur_stat.st_mode)) {
        last_stat = cur_stat;
        if (chdir("..") < 0)
            return ERROR;
        if (getcwd(cur_cwd_p, 255) == NULL) {
            errno = EIO;
            return ERROR;
        }
    } else { /* path is a file */
        path_len = strlen(path);
        suffix_len = strlen(strrchr(path, 47)); /* 47 = '/' */
        dir_len = path_len - suffix_len;
        dirname_p = strncpy(dirname_p, path, dir_len);
        if (chdir(dirname_p) < 0) 
            return ERROR;
        if (lstat(".", &last_stat) < 0)
            return ERROR;
    }

    for (;;) {
        if (lstat("..", &cur_stat) < 0)
            return ERROR;
        if (cur_stat.st_dev != last_stat.st_dev || cur_stat.st_ino == last_stat.st_ino)
            break; /* this is the mount point */
        if (chdir("..") < 0)
            return ERROR;
        last_stat = cur_stat;
    }
    if (getcwd(mount_point, PATH_MAX) == NULL)
        return ERROR;
    if (chdir(saved_cwd) < 0)
        return ERROR;
    return SUCCESS;
}

int fs_getDrive(lua_State *L) {
    char mountpoint[PATH_MAX];
    char * path = fixpath(lua_tostring(L, 1));
    if (!getmntpt(path, mountpoint)) {free(path); return 0;}
    lua_pushstring(L, mountpoint);
    free(path);
    return 1;
}

int fs_getSize(lua_State *L) {
    struct stat st;
    char * path = fixpath(lua_tostring(L, 1));
    if (stat(path, &st) != 0) err(L, path, "No such file");
    lua_pushinteger(L, st.st_size);
    free(path);
    return 1;
}

int fs_getFreeSpace(lua_State *L) {
    struct statvfs st;
    char * path = fixpath(lua_tostring(L, 1));
    if (statvfs(path, &st) != 0) err(L, path, "No such file or directory");
    lua_pushinteger(L, st.f_bavail * st.f_bsize);
    free(path);
    return 1;
}

int recurse_mkdir(const char * path) {
    if (mkdir(path, 0777) != 0) {
        if (errno == ENOENT && strcmp(path, "/") != 0) {
            if (recurse_mkdir(dirname(unconst(path)))) return 1;
            mkdir(path, 0777);
        } else return 1;
    }
    return 0;
}

int fs_makeDir(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (recurse_mkdir(path) != 0) err(L, path, strerror(errno));
    free(path);
    return 0;
}

int fs_move(lua_State *L) {
    char * fromPath, *toPath;
    fromPath = fixpath(lua_tostring(L, 1));
    toPath = fixpath(lua_tostring(L, 2));
    if (rename(fromPath, toPath) != 0) {
        free(toPath);
        err(L, fromPath, strerror(errno));
    }
    free(fromPath);
    free(toPath);
    return 0;
}

int fs_copy(lua_State *L) {
    char * fromPath, *toPath;
    FILE * fromfp, *tofp;
    char tmp[1024];
    int read;
    fromPath = fixpath(lua_tostring(L, 1));
    toPath = fixpath(lua_tostring(L, 2));

    fromfp = fopen(fromPath, "r");
    if (fromfp == NULL) {
        free(toPath);
        err(L, fromPath, "Cannot read file");
    }
    tofp = fopen(toPath, "w");
    if (tofp == NULL) {
        free(fromPath);
        fclose(fromfp);
        err(L, toPath, "Cannot write file");
    }

    while (!feof(fromfp)) {
        read = fread(tmp, 1, 1024, fromfp);
        fwrite(tmp, read, 1, tofp);
        if (read < 1024) break;
    }

    fclose(fromfp);
    fclose(tofp);
    free(fromPath);
    free(toPath);
    return 0;
}

int fs_delete(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (unlink(path) != 0) err(L, path, strerror(errno));
    free(path);
    return 0;
}

int fs_combine(lua_State *L) {
    char * localPath, *retval;
    const char * basePath = lua_tostring(L, 1);
    localPath = fixpath(lua_tostring(L, 2));
    if (basePath[0] == '/') basePath = basePath + 1;
    if (basePath[strlen(basePath)-1] == '/') localPath = localPath + 1;
    retval = (char*)malloc(strlen(basePath) + strlen(localPath) + 1);
    strcpy(retval, basePath);
    strcat(retval, localPath);
    if (basePath[strlen(basePath)-1] == '/') localPath = localPath - 1;
    free(localPath);
    lua_pushstring(L, retval);
    return 1;
}

int fs_open(lua_State *L) {
    const char * mode;
    FILE * fp;
    char * path = fixpath(lua_tostring(L, 1));
    mode = lua_tostring(L, 2);
    fp = fopen(path, mode);
    if (fp == NULL) err(L, path, strerror(errno));
    free(path);
    lua_newtable(L);
    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, fp);
    lua_pushcclosure(L, handle_close, 1);
    lua_settable(L, -3);
    if (strcmp(mode, "r") == 0) {
        lua_pushstring(L, "readAll");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_readAll, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "readLine");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_readLine, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "read");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_readChar, 1);
        lua_settable(L, -3);
    } else if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0) {
        lua_pushstring(L, "write");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_writeString, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "writeLine");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_writeLine, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "flush");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_flush, 1);
        lua_settable(L, -3);
    } else if (strcmp(mode, "rb") == 0) {
        lua_pushstring(L, "read");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_readByte, 1);
        lua_settable(L, -3);
    } else if (strcmp(mode, "wb") == 0) {
        lua_pushstring(L, "write");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_writeByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "flush");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, handle_flush, 1);
        lua_settable(L, -3);
    } else {
        lua_remove(L, -1);
        err(L, unconst(mode), "Invalid mode");
    }
    return 1;
}

int fs_find(lua_State *L) {
    glob_t g;
    int i;
    int rval = 0;
    const char * wildcard = lua_tostring(L, 1);
    lua_newtable(L);
    rval = glob(wildcard, 0, NULL, &g);
    if (rval == 0) {
        for (i = 0; i < g.gl_pathc; i++) {
            lua_pushnumber(L, i + 1);
            lua_pushstring(L, g.gl_pathv[i]);
            lua_settable(L, -3);
        }
        globfree(&g);
    }
    return 1;
}

int fs_getDir(lua_State *L) {
    char * path = unconst(lua_tostring(L, 1));
    lua_pushstring(L, dirname(path));
    free(path);
    return 1;
}

const char * fs_keys[16] = {
    "list",
    "exists",
    "isDir",
    "isReadOnly",
    "getName",
    "getDrive",
    "getSize",
    "getFreeSpace",
    "makeDir",
    "move",
    "copy",
    "delete",
    "combine",
    "open",
    "find",
    "getDir"
};

lua_CFunction fs_values[16] = {
    fs_list,
    fs_exists,
    fs_isDir,
    fs_isReadOnly,
    fs_getName,
    fs_getDrive,
    fs_getSize,
    fs_getFreeSpace,
    fs_makeDir,
    fs_move,
    fs_copy,
    fs_delete,
    fs_combine,
    fs_open,
    fs_find,
    fs_getDir
};

library_t fs_lib = {"fs", 16, fs_keys, fs_values};
