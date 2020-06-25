/*
@module  disp
@summary 显示屏控制
@version 1.0
@data    2020.03.30
*/
#include "luat_base.h"
#include "luat_malloc.h"

#include "luat_disp.h"
#include "luat_gpio.h"
#include "luat_timer.h"
#include "u8g2.h"

#define TAG "luat.disp"
#define LUAT_LOG_TAG "luat.disp"
#include "luat_log.h"

static u8g2_t* u8g2;
static int u8g2_lua_ref;
/*
显示屏初始化
@function disp.init(id, type, port)
@int 显示器id, 默认值0, 当前只支持0,单个显示屏
@string 显示屏类型,当前仅支持ssd1306,默认值也是ssd1306
@string 接口类型,当前仅支持i2c1,默认值也是i2c1
@return int 正常初始化1,已经初始化过2,内存不够3
@usage
-- 初始化i2c1的ssd1306
if disp.init() == 1 then
    log.info("显示屏初始化成功")
end
--disp.init(0, "ssd1306", "i2c1")
*/
static int l_disp_init(lua_State *L) {
    if (u8g2 != NULL) {
        LLOGI("disp is aready inited");
        lua_pushinteger(L, 2);
        return 1;
    }
    u8g2 = (u8g2_t*)lua_newuserdata(L, sizeof(u8g2_t));
    if (u8g2 == NULL) {
        LLOGE("lua_newuserdata return NULL, out of memory ?!");
        lua_pushinteger(L, 3);
        return 1;
    }
    // TODO: 暂时只支持SSD1306 12864, I2C接口-> i2c1soft, 软件模拟
    luat_disp_conf_t conf = {0};
    conf.pinType = 2; // I2C 硬件(或者是个假硬件)
    conf.ptr = u8g2;
    if (lua_istable(L, 1)) {
        // 参数解析
        lua_pushliteral(L, "mode");
        lua_gettable(L, 1);
        if (lua_isstring(L, -1)) {
            const char* mode = luaL_checkstring(L, -1);
            if (strcmp("i2c_sw", mode) == 0) {
                conf.pinType = 1;
            }
            else if (strcmp("i2c_hw", mode) == 0) {
                conf.pinType = 2;
            }
            else if (strcmp("spi_sw_3pin", mode) == 0) {
                conf.pinType = 3;
            }
            else if (strcmp("spi_sw_4pin", mode) == 0) {
                conf.pinType = 4;
            }
            else if (strcmp("spi_hw_4pin", mode) == 0) {
                conf.pinType = 5;
            }
        }
        lua_pop(L, 1);

        // 解析pin0 ~ pin7
        lua_pushliteral(L, "pin0");
        lua_gettable(L, 1);
        if (lua_isinteger(L, -1)) {
            conf.pin0 = luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);
        
        lua_pushliteral(L, "pin1");
        lua_gettable(L, 1);
        if (lua_isinteger(L, -1)) {
            conf.pin1 = luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);
        
        lua_pushliteral(L, "pin2");
        lua_gettable(L, 1);
        if (lua_isinteger(L, -1)) {
            conf.pin2 = luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);
        
        lua_pushliteral(L, "pin3");
        lua_gettable(L, 1);
        if (lua_isinteger(L, -1)) {
            conf.pin3 = luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

        // pin4 ~ pin7暂时用不到,先不设置了
    }
    if (luat_disp_setup(&conf)) {
        u8g2 = NULL;
        LLOGW("disp init fail");
        return 0; // 初始化失败
    }
    
    u8g2_lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    
    lua_pushinteger(L, 1);
    return 1;
}

/*
关闭显示屏
@function disp.close(id) 
@int 显示器id, 默认值0, 当前只支持0,单个显示屏
@usage
disp.close()
*/
static int l_disp_close(lua_State *L) {
    if (u8g2_lua_ref != 0) {
        lua_geti(L, LUA_REGISTRYINDEX, u8g2_lua_ref);
        if (lua_isuserdata(L, -1)) {
            luaL_unref(L, LUA_REGISTRYINDEX, u8g2_lua_ref);
        }
        u8g2_lua_ref = 0;
    }
    lua_gc(L, LUA_GCCOLLECT, 0);
    u8g2 = NULL;
    return 0;
}
/*
@function disp.clear(id) 清屏
@int 显示器id, 默认值0, 当前只支持0,单个显示屏
@usage
disp.clear(0)
*/
static int l_disp_clear(lua_State *L) {
    if (u8g2 == NULL) return 0;
    u8g2_ClearBuffer(u8g2);
    return 0;
}
/*
把显示数据更新到屏幕
@function disp.update(id)
@int 显示器id, 默认值0, 当前只支持0,单个显示屏
@usage
disp.update(0)
*/
static int l_disp_update(lua_State *L) {
    if (u8g2 == NULL) return 0;
    u8g2_SendBuffer(u8g2);
    return 0;
}

/*
在显示屏上画一段文字,要调用disp.update才会更新到屏幕
@function disp.drawStr(id, content, x, y) 
@int 显示器id, 默认值0, 当前只支持0,单个显示屏, 这参数暂时不要传.
@string 文件内容
@int 横坐标
@int 竖坐标
@usage
disp.drawStr("wifi is ready", 10, 20)
*/
static int l_disp_draw_text(lua_State *L) {
    if (u8g2 == NULL) return 0;
    u8g2_SetFont(u8g2, u8g2_font_ncenB08_tr);
    size_t len;
    size_t x, y;
    const char* str = luaL_checklstring(L, 1, &len);
    x = luaL_checkinteger(L, 2);
    y = luaL_checkinteger(L, 3);
    
    u8g2_DrawStr(u8g2, x, y, str);
    return 0;
}

#include "rotable.h"
static const rotable_Reg reg_disp[] =
{
    { "init", l_disp_init, 0},
    { "close", l_disp_close, 0},
    { "clear", l_disp_clear, 0},
    { "update", l_disp_update, 0},
    { "drawStr", l_disp_draw_text, 0},
	{ NULL, NULL, 0}
};

LUAMOD_API int luaopen_disp( lua_State *L ) {
    u8g2_lua_ref = 0;
    u8g2 = NULL;
    rotable_newlib(L, reg_disp);
    return 1;
}

uint8_t luat_u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

LUAT_WEAK int luat_disp_setup(luat_disp_conf_t *conf) {
    if (conf->pinType == 1) {
        u8g2_t* u8g2 = (u8g2_t*)conf->ptr;
        u8g2_Setup_ssd1306_i2c_128x64_noname_f( u8g2, U8G2_R0, u8x8_byte_sw_i2c, luat_u8x8_gpio_and_delay);
        u8g2->u8x8.pins[U8X8_PIN_I2C_CLOCK] = conf->pin0;
        u8g2->u8x8.pins[U8X8_PIN_I2C_DATA] = conf->pin1;
        LLOGD("setup disp i2c.sw SCL=%ld SDA=%ld", conf->pin0, conf->pin1);
        u8g2_InitDisplay(u8g2);
        u8g2_SetPowerSave(u8g2, 0);
        return 0;
    }
    LLOGI("only i2c sw mode is support, by default impl");
    return -1;
}

LUAT_WEAK int luat_disp_close(luat_disp_conf_t *conf) {
    return 0;
} 

LUAT_WEAK uint8_t luat_u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch(msg)
    {
        case U8X8_MSG_DELAY_NANO:            // delay arg_int * 1 nano second
            __asm__ volatile("nop");
            break;
        
        case U8X8_MSG_DELAY_100NANO:        // delay arg_int * 100 nano seconds
            __asm__ volatile("nop");
            break;
        
        case U8X8_MSG_DELAY_10MICRO:        // delay arg_int * 10 micro seconds
            for (uint16_t n = 0; n < 320; n++)
            {
                __asm__ volatile("nop");
            }
        break;
        
        case U8X8_MSG_DELAY_MILLI:            // delay arg_int * 1 milli second
            luat_timer_mdelay(arg_int);
            break;
        
        case U8X8_MSG_GPIO_AND_DELAY_INIT:  
            // Function which implements a delay, arg_int contains the amount of ms  
            
            // set spi pin mode 
            luat_gpio_mode(u8x8->pins[U8X8_PIN_SPI_CLOCK],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);//d0 a5 15 d1 a7 17 res b0 18 dc b1 19 cs a4 14  
            luat_gpio_mode(u8x8->pins[U8X8_PIN_SPI_DATA],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_RESET],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_DC],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_CS],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            
            // set i2c pin mode
            luat_gpio_mode(u8x8->pins[U8X8_PIN_I2C_DATA],Luat_GPIO_OUTPUT, Luat_GPIO_DEFAULT, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_I2C_CLOCK],Luat_GPIO_OUTPUT, Luat_GPIO_DEFAULT, Luat_GPIO_HIGH);
            
            // set 8080 pin mode
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D0],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D1],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D2],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D3],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D4],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D5],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D6],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_D7],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_E],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_DC],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            luat_gpio_mode(u8x8->pins[U8X8_PIN_RESET],Luat_GPIO_OUTPUT, Luat_GPIO_PULLUP, Luat_GPIO_HIGH);
            
            // set value
            //luat_gpio_set(u8x8->pins[U8X8_PIN_SPI_CLOCK],1);
            //luat_gpio_set(u8x8->pins[U8X8_PIN_SPI_DATA],1);
            //luat_gpio_set(u8x8->pins[U8X8_PIN_RESET],1);
            //luat_gpio_set(u8x8->pins[U8X8_PIN_DC],1);
            //luat_gpio_set(u8x8->pins[U8X8_PIN_CS],1);
            break;
        
        case U8X8_MSG_DELAY_I2C:
            // arg_int is the I2C speed in 100KHz, e.g. 4 = 400 KHz
            // arg_int=1: delay by 5us, arg_int = 4: delay by 1.25us
            for (uint16_t n = 0; n < (arg_int<=2?160:40); n++)
            {
                __asm__ volatile("nop");
            }
            break;

        //case U8X8_MSG_GPIO_D0:                // D0 or SPI clock pin: Output level in arg_int
        //case U8X8_MSG_GPIO_SPI_CLOCK:

        //case U8X8_MSG_GPIO_D1:                // D1 or SPI data pin: Output level in arg_int
        //case U8X8_MSG_GPIO_SPI_DATA:

        case U8X8_MSG_GPIO_D2:                // D2 pin: Output level in arg_int
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_D2],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_D2],0);
            break;

        case U8X8_MSG_GPIO_D3:                // D3 pin: Output level in arg_int
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_D3],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_D3],0);
            break;

        case U8X8_MSG_GPIO_D4:                // D4 pin: Output level in arg_int
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_D4],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_D4],0);
            break;

        case U8X8_MSG_GPIO_D5:                // D5 pin: Output level in arg_int
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_D5],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_D5],0);
            break;

        case U8X8_MSG_GPIO_D6:                // D6 pin: Output level in arg_int
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_D6],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_D6],0);
            break;

        case U8X8_MSG_GPIO_D7:                // D7 pin: Output level in arg_int
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_D7],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_D7],0);
            break;

        case U8X8_MSG_GPIO_E:                // E/WR pin: Output level in arg_int
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_E],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_E],0);
            break;

        case U8X8_MSG_GPIO_I2C_CLOCK:
            // arg_int=0: Output low at I2C clock pin
            // arg_int=1: Input dir with pullup high for I2C clock pin
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_I2C_CLOCK],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_I2C_CLOCK],0);  
            break;

        case U8X8_MSG_GPIO_I2C_DATA:
            // arg_int=0: Output low at I2C data pin
            // arg_int=1: Input dir with pullup high for I2C data pin
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_I2C_DATA],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_I2C_DATA],0);              
      break;

        case U8X8_MSG_GPIO_SPI_CLOCK:  
            //Function to define the logic level of the clockline  
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_SPI_CLOCK],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_SPI_CLOCK],0);
            break;

        case U8X8_MSG_GPIO_SPI_DATA:
            //Function to define the logic level of the data line to the display  
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_SPI_DATA],1);  
            else luat_gpio_set(u8x8->pins[U8X8_PIN_SPI_DATA],0);  
            break;

        case U8X8_MSG_GPIO_CS:
            // Function to define the logic level of the CS line  
            if(arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_CS],1);
            else luat_gpio_set(u8x8->pins[U8X8_PIN_CS],0);
            break;

        case U8X8_MSG_GPIO_DC:
            //Function to define the logic level of the Data/ Command line  
            if(arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_DC],1);
            else luat_gpio_set(u8x8->pins[U8X8_PIN_DC],0);
            break;

        case U8X8_MSG_GPIO_RESET:
            //Function to define the logic level of the RESET line
            if (arg_int) luat_gpio_set(u8x8->pins[U8X8_PIN_RESET],1);
            else luat_gpio_set(u8x8->pins[U8X8_PIN_RESET],0);
            break;

        default:
            //A message was received which is not implemented, return 0 to indicate an error 
            return 0; 
    }
    return 1;
}