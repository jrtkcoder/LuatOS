
/*
@module  pm
@summary 电源管理
@version 1.0
@date    2020.07.02
*/
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_pm.h"
#include "luat_msgbus.h"

static int lua_event_cb = 0;

/**
请求进入指定的休眠模式
@function pm.request(mode)
@int 休眠模式,例如pm.IDLE/LIGHT/DEEP/HIB
@return boolean 处理结果,即使返回成功,也不一定会进入, 也不会马上进入
@usage
-- 请求进入休眠模式
pm.request(pm.HIB)
 */
static int l_pm_request(lua_State *L) {
    int mode = luaL_checkinteger(L, 1);
    if (luat_pm_request(mode) == 0)
        lua_pushboolean(L, 1);
    else
        lua_pushboolean(L, 0);
    return 1;
}

// static int l_pm_release(lua_State *L) {
//     int mode = luaL_checkinteger(L, 1);
//     if (luat_pm_release(mode) == 0)
//         lua_pushboolean(L, 1);
//     else
//         lua_pushboolean(L, 0);
//     return 1;
// }

/**
启动底层定时器,在休眠模式下依然生效. 只触发一次
@function pm.dtimerStart(id, timeout)
@int 定时器id,通常是0-3
@int 定时时长,单位毫秒
@return boolean 处理结果
@usage
-- 添加底层定时器
pm.dtimerStart(0, 300 * 1000) -- 5分钟后唤醒
 */
static int l_pm_dtimer_start(lua_State *L) {
    int dtimer_id = luaL_checkinteger(L, 1);
    int timeout = luaL_checkinteger(L, 2);
    if (luat_pm_dtimer_start(dtimer_id, timeout)) {
        lua_pushboolean(L, 0);
    }
    else {
        lua_pushboolean(L, 1);
    }
    return 1;
}

/**
关闭底层定时器
@function pm.dtimerStop(id)
@int 定时器id
@usage
-- 添加底层定时器
pm.dtimerStart(0) -- 关闭id=0的底层定时器
 */
static int l_pm_dtimer_stop(lua_State *L) {
    int dtimer_id = luaL_checkinteger(L, 1);
    luat_pm_dtimer_stop(dtimer_id);
    return 0;
}

static int l_pm_on(lua_State *L) {
    if (lua_isfunction(L, 1)) {
        if (lua_event_cb != 0) {
            luaL_unref(L, LUA_REGISTRYINDEX, lua_event_cb);
        }
        lua_event_cb = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (lua_event_cb != 0) {
        luaL_unref(L, LUA_REGISTRYINDEX, lua_event_cb);
    }
    return 0;
}

/**
开机原因,用于判断是从休眠模块开机,还是电源/复位开机
@function pm.request(mode)
@int 休眠模式,例如pm.IDLE/LIGHT/DEEP/HIB
@return boolean 处理结果,即使返回成功,也不一定会进入, 也不会马上进入
@usage
-- 请求进入休眠模式
pm.request(pm.HIB)
 */
static int l_pm_last_reson(lua_State *L) {
    lua_pushinteger(L, luat_pm_last_state());
    return 1;
}

/**
强制进入指定的休眠模式
@function pm.force(mode)
@int 休眠模式,仅pm.DEEP/HIB
@return boolean 处理结果,若返回成功,大概率会马上进入该休眠模式
@usage
-- 请求进入休眠模式
pm.force(pm.HIB)
 */
static int l_pm_force(lua_State *L) {
    lua_pushinteger(L, luat_pm_force(luaL_checkinteger(L, 1)));
    return 1;
}

/**
检查休眠状态
@function pm.check()
@return boolean 处理结果,如果能顺利进入休眠,返回true,否则返回false
@usage
-- 请求进入休眠模式,然后检查是否能真的休眠
pm.request(pm.HIB)
if pm.check() then
    log.info("pm", "it is ok to hib")
else
    pm.force(pm.HIB) -- 强制休眠
end
 */
static int l_pm_check(lua_State *L) {
    lua_pushinteger(L, luat_pm_check());
    return 1;
}

static int luat_pm_msg_handler(lua_State *L, void* ptr) {
    rtos_msg_t* msg = (rtos_msg_t*)lua_topointer(L, -1);
    if (lua_event_cb == 0) {
        return 0;
    }
    lua_geti(L, LUA_REGISTRYINDEX, lua_event_cb);
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, msg->arg1);
        lua_pushinteger(L, msg->arg2);
        lua_call(L, 2, 0);
    }
    return 0;
}

void luat_pm_cb(int event, int arg, void* args) {
    if (lua_event_cb != 0) {
        rtos_msg_t msg;
        msg.handler = luat_pm_msg_handler;
        msg.arg1 = event;
        msg.arg2 = arg;
        msg.ptr = NULL;
        luat_msgbus_put(&msg, 0);
    }
}

#include "rotable.h"
static const rotable_Reg reg_pm[] =
{
    { "request" ,       l_pm_request , 0},
    // { "release" ,       l_pm_release,  0},
    { "dtimerStart",    l_pm_dtimer_start,0},
    { "dtimerStop" ,    l_pm_dtimer_stop, 0},
    { "on",             l_pm_on,   0},
    { "force",          l_pm_force, 0},
    { "check",          l_pm_check, 0},
    { "lastReson",      l_pm_last_reson, 0},
    { "IDLE",           NULL, LUAT_PM_SLEEP_MODE_IDLE},
    { "LIGHT",          NULL, LUAT_PM_SLEEP_MODE_LIGHT},
    { "DEEP",           NULL, LUAT_PM_SLEEP_MODE_DEEP},
    { "HIB",            NULL, LUAT_PM_SLEEP_MODE_STANDBY},
	{ NULL,          NULL ,       0}
};

LUAMOD_API int luaopen_pm( lua_State *L ) {
    rotable_newlib(L, reg_pm);
    return 1;
}
