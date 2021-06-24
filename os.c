#include "os.h"
#include "keys.h"
#include <lauxlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "queue.h"
#include <ncurses.h>
#ifdef UPTIME
#include <sys/sysinfo.h>
#endif

struct time_t_list {
    time_t val;
    int id;
    struct time_t_list * next;
};

struct double_list {
    double val;
    int id;
    struct double_list * next;
};

int running = 1;
const char * label;
bool label_defined = false;
queue_t eventQueue;
lua_State * paramQueue;
struct time_t_list * timers = NULL;
struct double_list * alarms = NULL;
int nextTimerID = 0;
int nextAlarmID = 0;

int getNextEvent(lua_State *L, const char * filter) {
    const char * ev;
    struct time_t_list * oldt;
    struct double_list * olda;
    time_t t;
    struct tm tm;
    int ch, cch, count;
    char tmp[2];
    lua_State *param = lua_newthread(paramQueue);
    do {
        while (queue_size(&eventQueue) == 0) {
            if (timers && timers->val == 0) {
                oldt = timers;
                timers = timers->next;
                free(oldt);
            }
            if (alarms && alarms->val == -1) {
                olda = alarms;
                alarms = alarms->next;
                free(olda);
            }
            t = time(NULL);
            tm = *localtime(&t);
            for (oldt = timers; oldt; oldt = oldt->next) {
                if (t == oldt->val) {
                    lua_pushinteger(param, oldt->id);
                    queue_push(&eventQueue, "timer");
                    param = lua_newthread(paramQueue);
                }
            }
            for (olda = alarms; olda; olda = olda->next) {
                if ((double)tm.tm_hour + ((double)tm.tm_min/60.0) + ((double)tm.tm_sec/3600.0) == olda->val) {
                    lua_pushinteger(param, olda->id);
                    queue_push(&eventQueue, "alarm");
                    param = lua_newthread(paramQueue);
                }
            }
            ch = getch();
            if (ch != 0) {
                if ((ch >= 32 && ch < 128)) {
                    tmp[0] = ch;
                    lua_pushstring(param, tmp);
                    queue_push(&eventQueue, "char");
                    param = lua_newthread(paramQueue);
                }
                cch = getKey(ch);
                if (cch != 0) {
                    lua_pushinteger(param, cch);
                    lua_pushboolean(param, false);
                    queue_push(&eventQueue, "key");
                    param = lua_newthread(paramQueue);
                }
            }
        }
        ev = queue_front(&eventQueue);
        queue_pop(&eventQueue);
    } while (strlen(filter) > 0 && strcmp(ev, filter) != 0);
    lua_pop(paramQueue, 1);
    param = lua_tothread(paramQueue, 1);
    if (param == NULL) return 0;
    count = lua_gettop(param);
    if (!lua_checkstack(L, count + 1)) {
        printf("Could not allocate enough space in the stack for %d elements, skipping event \"%s\"\n", count, ev);
        return 0;
    }
    lua_pushstring(L, ev);
    lua_xmove(param, L, count);
    lua_remove(paramQueue, 1);
    return count + 1;
}

int os_getComputerID(lua_State *L) {lua_pushinteger(L, 0); return 1;}
int os_getComputerLabel(lua_State *L) {
    if (!label_defined) return 0;
    lua_pushstring(L, label);
    return 1;
}

int os_setComputerLabel(lua_State *L) {
    label = lua_tostring(L, 1);
    label_defined = true;
    return 0;
}

int os_queueEvent(lua_State *L) {
    const char * name;
    lua_State *param;
    int count = lua_gettop(L);
    name = lua_tostring(L, 1);
    param = lua_newthread(paramQueue);
    lua_xmove(L, param, count - 1);
    queue_push(&eventQueue, name);
    return 0;
}

int os_clock(lua_State *L) {
    #ifdef UPTIME
    struct sysinfo info;
    sysinfo(&info);
    lua_pushinteger(L, info.uptime);
    #else
    lua_pushinteger(L, clock() / CLOCKS_PER_SEC);
    #endif
    return 1;
}

int os_startTimer(lua_State *L) {
    struct time_t_list * n = (struct time_t_list*)malloc(sizeof(struct time_t_list));
    n->val = time(0) + lua_tointeger(L, 1);
    n->id = nextTimerID++;
    n->next = timers;
    timers = n;
    lua_pushinteger(L, n->id);
    return 1;
}

int os_cancelTimer(lua_State *L) {
    struct time_t_list * old, *old2;
    int id = lua_tointeger(L, 1);
    if (timers && id == timers->id) {
        old = timers;
        timers = timers->next;
        free(old);
    } else {
        for (old = timers, old2 = NULL; old; old2 = old, old = old->next) {
            if (old->id == id) {
                if (old2) old2->next = old->next;
                free(old);
                return 0;
            }
        }
    }
    return 0;
}

int os_time(lua_State *L) {
    struct tm rightNow;
    time_t t;
    int hour, minute, second;
    const char * type = "ingame";
    if (lua_gettop(L) > 0) type = lua_tostring(L, 1);
    t = time(NULL);
    if (strcmp(type, "utc") == 0) rightNow = *gmtime(&t);
    else rightNow = *localtime(&t);
    hour = rightNow.tm_hour;
    minute = rightNow.tm_min;
    second = rightNow.tm_sec;
    lua_pushnumber(L, (double)hour + ((double)minute/60.0) + ((double)second/3600.0));
    return 1;
}

int os_epoch(lua_State *L) {
    time_t t;
    struct tm rightNow;
    int hour, minute, second;
    double m_time, m_day;
    const char * type = "ingame";
    if (lua_gettop(L) > 0) type = lua_tostring(L, 1);
    if (strcmp(type, "utc") == 0) {
        lua_pushinteger(L, (long long)time(NULL) * 1000LL);
    } else if (strcmp(type, "local") == 0) {
        t = time(NULL);
        lua_pushinteger(L, (long long)mktime(localtime(&t)) * 1000LL);
    } else {
        t = time(NULL);
        rightNow = *localtime(&t);
        hour = rightNow.tm_hour;
        minute = rightNow.tm_min;
        second = rightNow.tm_sec;
        m_time = (double)hour + ((double)minute/60.0) + ((double)second/3600.0);
        m_day = rightNow.tm_yday;
        lua_pushinteger(L, m_day * 86400000 + (int) (m_time * 3600000.0f));
    }
    return 1;
}

int os_day(lua_State *L) {
    time_t t;
    struct tm rightNow;
    const char * type = "ingame";
    if (lua_gettop(L) > 0) type = lua_tostring(L, 1);
    t = time(NULL);
    if (strcmp(type, "ingame") == 0) {
        rightNow = *localtime(&t);
        lua_pushinteger(L, rightNow.tm_yday);
        return 1;
    } else if (strcmp(type, "local")) t = mktime(localtime(&t));
    lua_pushinteger(L, t/(60*60*24));
    return 1;
}

int os_setAlarm(lua_State *L) {
    struct double_list * n = (struct double_list*)malloc(sizeof(struct double_list));
    n->val = lua_tonumber(L, 1);
    n->id = nextAlarmID++;
    n->next = alarms;
    alarms = n;
    lua_pushinteger(L, n->id);
    return 1;
}

int os_cancelAlarm(lua_State *L) {
    struct double_list * old, *old2;
    int id = lua_tointeger(L, 1);
    if (alarms && id == alarms->id) {
        old = alarms;
        alarms = alarms->next;
        free(old);
    } else {
        for (old = alarms, old2 = NULL; old; old2 = old, old = old->next) {
            if (old->id == id) {
                if (old2) old2->next = old->next;
                free(old);
                return 0;
            }
        }
    }
    return 0;
}

int os_shutdown(lua_State *L) {
    sync();
    running = 0;
    return 0;
}

int os_reboot(lua_State *L) {
    sync();
    running = 2;
    return 0;
}

int os_system(lua_State *L) {
    system(lua_tostring(L, 1));
    return 0;
}

const char * os_keys[15] = {
    "getComputerID",
    "getComputerLabel",
    "setComputerLabel",
    "queueEvent",
    "clock",
    "startTimer",
    "cancelTimer",
    "time",
    "epoch",
    "day",
    "setAlarm",
    "cancelAlarm",
    "shutdown",
    "reboot",
    "system"
};

lua_CFunction os_values[15] = {
    os_getComputerID,
    os_getComputerLabel,
    os_setComputerLabel,
    os_queueEvent,
    os_clock,
    os_startTimer,
    os_cancelTimer,
    os_time,
    os_epoch,
    os_day,
    os_setAlarm,
    os_cancelAlarm,
    os_shutdown,
    os_reboot,
    os_system
};

library_t os_lib = {"os", 15, os_keys, os_values};
