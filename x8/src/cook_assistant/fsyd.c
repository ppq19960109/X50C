#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include "mlog.h"
#include "cloud.h"
#include "fsyd.h"

static void (*oil_temp_cb)(const unsigned short, enum INPUT_DIR);
void register_oil_temp_cb(void (*cb)(const unsigned short, enum INPUT_DIR))
{
    oil_temp_cb = cb;
}

static char *state_info[] = {"shake", "down_jump", "rise_jump", "rise_slow", "down_slow", "gentle", "idle", "pan_fire"
#ifdef BOIL_ENABLE
                             ,
                             "boil"
#endif
};
// 档位切换延时
static unsigned char g_gear_delay_time = INPUT_DATA_HZ * 2;

static state_handle_t g_state_func[2];
static state_hood_t state_hood = {0};

char *get_current_time_format(void)
{
    static char data_time[16];
    time_t t = time(NULL);
    struct tm *cur_tm = localtime(&t);
    sprintf(data_time, "%d:%d:%d", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    return data_time;
}
state_hood_t *get_hood_handle()
{
    return &state_hood;
}

state_handle_t *get_input_handle(enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = NULL;
    if (input_dir == INPUT_LEFT)
    {
        state_handle = &g_state_func[INPUT_LEFT];
    }
    else
    {
        state_handle = &g_state_func[INPUT_RIGHT];
    }
    return state_handle;
}

#ifdef SIMULATION
static char *dispaly_state_info[] = {"翻炒", "跳降", "跳升", "缓升", "缓降", "平缓", "空闲", "移锅小火", "煮炖"};
static char dispaly_msg[33];
static char *fire_info[] = {"小火", "大火"};
int get_fire_gear(char *msg, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    if (msg != NULL)
        strcpy(msg, fire_info[state_handle->fire_gear]);
    return state_handle->fire_gear;
}
int get_hood_gear(char *msg)
{
    if (msg != NULL)
        strcpy(msg, dispaly_msg);
    return state_hood.gear;
}
int get_cur_state(char *msg, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    if (msg != NULL)
        strcpy(msg, dispaly_state_info[state_handle->state]);
    return state_handle->state;
}
#endif

static int (*close_fire_cb)(enum INPUT_DIR input_dir);
void register_close_fire_cb(int (*cb)(enum INPUT_DIR input_dir))
{
    close_fire_cb = cb;
}
static int (*fire_gear_cb)(const int gear, enum INPUT_DIR input_dir);
void register_fire_gear_cb(int (*cb)(const int gear, enum INPUT_DIR input_dir))
{
    fire_gear_cb = cb;
}
static int (*hood_gear_cb)(const int gear);
void register_hood_gear_cb(int (*cb)(const int gear))
{
    hood_gear_cb = cb;
}

static int (*thread_lock_cb)();
void register_thread_lock_cb(int (*cb)())
{
    thread_lock_cb = cb;
}
static int (*thread_unlock_cb)();
void register_thread_unlock_cb(int (*cb)())
{
    thread_unlock_cb = cb;
}
static int (*cook_assist_remind_cb)(int);
void register_cook_assist_remind_cb(int (*cb)(int)) // 0:辅助控温3分钟 1:移锅小火3分钟
{
    cook_assist_remind_cb = cb;
}

/***********************************************************
 * 火焰档位切换函数
 * 1.type:0 无效 type:1 火焰档位同步
 ***********************************************************/
void set_fire_gear(unsigned char fire_gear, state_handle_t *state_handle, const unsigned char type)
{
    mlogPrintf("%s,set_fire_gear:%d\n", __func__, fire_gear);

    if (state_handle->fire_gear != fire_gear || type == 1)
    {
        mlogPrintf("%s,change set_fire_gear:%d type:%d\n", __func__, fire_gear, type);
        state_handle->fire_gear = fire_gear;
        if (fire_gear_cb != NULL)
            fire_gear_cb(fire_gear, state_handle->input_dir);
    }
}

/***********************************************************
 * 烟机档位切换函数
 * 1.升档降档延时切换
 * 2.type:0 无效 type:1 档位可以从关闭到打开 type:2 更新档位切换时间 type:3 档位马上切换 type:4 档位lock type:5 档位unlock
 ***********************************************************/
static void gear_change(unsigned char gear, const unsigned char type, const char *msg, state_handle_t *state_handle)
{
    mlogPrintf("%s,gear_change gear:%d type:%d msg:%s\n", __func__, gear, type, msg);
    if (state_hood.smart_smoke_switch == 0)
        return;
    if (thread_lock_cb != NULL)
    {
        if (thread_lock_cb() == 0)
        {
            mlogPrintf("%s,thread_lock_cb fail\n", __func__);
            goto fail;
        }
    }
    if (state_handle != NULL)
    {
        state_handle->hood_gear = gear;
        gear = g_state_func[INPUT_LEFT].hood_gear > g_state_func[INPUT_RIGHT].hood_gear ? g_state_func[INPUT_LEFT].hood_gear : g_state_func[INPUT_RIGHT].hood_gear;
    }
    if (type == 5)
    {
        state_hood.lock = 0;
    }
    else if (state_hood.lock)
    {
        mlogPrintf("%s,state_hood lock\n", __func__);
        goto end;
    }

    unsigned char last_gear = state_hood.gear;

    if (type == 2)
    {
        state_hood.gear_tick = 1;
    }
    else if (type == 3)
    {
        state_hood.gear_tick = g_gear_delay_time;
    }
    else if (type == 4)
    {
        state_hood.lock = 1;
    }

    if (gear == GEAR_INVALID)
        goto end;
    if (gear < state_hood.min_gear)
    {
        gear = state_hood.min_gear;
    }
    if (last_gear == gear)
    {
        state_hood.prepare_gear = GEAR_INVALID;
    }
    else
    {
        if (state_hood.gear == 0 && gear > 0 && type != 1 && type != 4)
        {
            mlogPrintf("%s,Cannot be turned on in off state type:%d\n", __func__, type);
        }
        else
        {
            if (state_hood.gear_tick >= g_gear_delay_time)
            {
                state_hood.gear = gear;
            }
            else
            {
                state_hood.prepare_gear = gear;
                mlogPrintf("%s,Prepare the gear:%d\n", __func__, gear);
            }
        }
    }

    if (last_gear != state_hood.gear)
    {
        state_hood.prepare_gear = GEAR_INVALID;
        state_hood.gear_tick = 1;

        mlogPrintf("%s,Enter the gear:%d\n", __func__, state_hood.gear);
        if (hood_gear_cb != NULL)
            hood_gear_cb(state_hood.gear);
    }
#ifdef SIMULATION
    if (msg != NULL && strcmp(msg, ""))
    {
        strcpy(dispaly_msg, msg);
    }
#endif
end:
    if (thread_unlock_cb != NULL)
        thread_unlock_cb();
fail:
    return;
}

void set_ignition_switch(unsigned char ignition_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    mlogPrintf("%s,set_ignition_switch:%d input_dir:%d\n", __func__, ignition_switch, input_dir);

    if (!state_hood.work_mode)
        return;
    if (state_handle->ignition_switch == ignition_switch)
        return;
    state_handle->ignition_switch = ignition_switch;
    mlogPrintf("%s,ignition_switch change:%d\n", __func__, ignition_switch);

    state_handle_t *state_handle_another;
    if (input_dir == INPUT_LEFT)
    {
        state_handle_another = get_input_handle(INPUT_RIGHT);
    }
    else
    {
        state_handle_another = get_input_handle(INPUT_LEFT);
    }
    if (ignition_switch)
    {
        // if (hood_gear_cb != NULL && state_handle_another->hood_gear == 0)
        //     hood_gear_cb(0);

        state_hood.close_delay_tick = 0;
        // cook_assistant_reinit(state_handle);
    }
    else
    {
        if (state_handle_another->ignition_switch > 0)
        {
            cook_assistant_reinit(state_handle);
            return;
        }
        if (state_handle->total_tick > INPUT_DATA_HZ * 50 * 10 || state_handle->shake_long > 0)
            state_hood.close_delay_tick = INPUT_DATA_HZ * 40;
        else
            state_hood.close_delay_tick = INPUT_DATA_HZ * 20;

        if (state_hood.gear > GEAR_TWO)
        {
            gear_change(2, 3, "set_ignition_switch sw 0", state_handle);
        }
        cook_assistant_reinit(state_handle);
    }
}
void set_hood_min_gear(unsigned char gear)
{
    mlogPrintf("%s,min_gear:%d\n", __func__, gear);
    if (state_hood.min_gear != gear)
    {
        state_hood.min_gear = gear;
        if (state_hood.gear < state_hood.min_gear)
        {
            state_hood.gear = state_hood.min_gear;
        }
    }
}
void recv_ecb_gear(unsigned char gear)
{
    mlogPrintf("%s,recv ecb gear:%d hood_gear:%d\n", __func__, gear, state_hood.gear);
    state_hood.gear = gear;
}

void recv_ecb_fire(unsigned char fire, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    mlogPrintf("%s,recv ecb fire:%d fire_gear:%d pan_fire_switch:%d temp_control_switch:%d\n", __func__, fire, state_handle->fire_gear, state_handle->pan_fire_switch, state_handle->temp_control_switch);
    if (state_handle->pan_fire_switch || state_handle->temp_control_switch)
    {
        if (fire != state_handle->fire_gear)
        {
            set_fire_gear(state_handle->fire_gear, state_handle, 1);
        }
    }
    else
    {
#ifndef BOIL_ENABLE
        if (fire != FIRE_BIG)
        {
            set_fire_gear(FIRE_BIG, state_handle, 1);
        }
#endif
    }
}

/***********************************************************
 * 风随烟动翻炒处理函数
 ***********************************************************/
static int state_func_shake(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 0;
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_SHAKE]);

        gear_change(3, 1, state_info[STATE_SHAKE], state_handle);

        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (state_handle->current_tick > INPUT_DATA_HZ * 30)
    {
        state_handle->shake_long = 1;
    }
    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }

    if (prepare_state == STATE_RISE_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 8 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 8 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_DOWN_JUMP)
    {
        if (state_handle->last_prepare_state_tick + 7 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_RISE_JUMP)
    {
        if (state_handle->last_prepare_state_tick + 7 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 12 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_PAN_FIRE)
    {
        mlogPrintf("%s,%s pan_fire:%d\n", __func__, state_info[STATE_SHAKE], state_handle->pan_fire_state);
        if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
        {
            return STATE_SHAKE;
        }
        //        else
        //        {
        //            if (state_handle->last_prepare_state_tick + STATE_JUDGE_DATA_SIZE/2 > state_handle->current_tick)
        //                return STATE_SHAKE;
        //        }
    }
    if (prepare_state != STATE_SHAKE)
    {
        state_handle->shake_exit_tick = state_handle->total_tick;
    }

    return prepare_state;
}
/***********************************************************
 * 风随烟动跳降处理函数
 ***********************************************************/
static int state_func_down_jump(unsigned char prepare_state, state_handle_t *state_handle) // 跳降
{
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_DOWN_JUMP]);
        if (prepare_state == STATE_PAN_FIRE)
        {
            return prepare_state;
        }

        if (state_handle->state_jump_temp > 1300)
        {
            gear_change(3, 3, state_info[STATE_DOWN_JUMP], state_handle);
            gear_change(GEAR_INVALID, 2, state_info[STATE_DOWN_JUMP], state_handle);
        }
        else if (state_handle->state_jump_temp < 800)
        {
            gear_change(1, 3, state_info[STATE_DOWN_JUMP], state_handle);
        }
        else
        {
            gear_change(GEAR_INVALID, 2, state_info[STATE_DOWN_JUMP], state_handle);
        }
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (prepare_state == STATE_RISE_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 3 > state_handle->current_tick)
            return STATE_DOWN_JUMP;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 3 > state_handle->current_tick)
            return STATE_DOWN_JUMP;
    }
    return prepare_state;
}
/***********************************************************
 * 风随烟动跳升处理函数
 ***********************************************************/
static int state_func_rise_jump(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_RISE_JUMP]);
        if (prepare_state == STATE_PAN_FIRE)
        {
            return prepare_state;
        }
        else if (state_handle->state_jump_temp > 700 && state_handle->state_jump_temp < 1500)
        {
            gear_change(3, 1, state_info[STATE_RISE_JUMP], state_handle);
        }
        else if (state_handle->state_jump_temp > 1500)
        {
            if (state_hood.gear < GEAR_TWO)
                gear_change(2, 1, state_info[STATE_RISE_JUMP], state_handle);
        }

        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    return prepare_state;
}

/***********************************************************
 * 风随烟动缓升处理函数
 ***********************************************************/
static int state_func_rise_slow(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_RISE_SLOW]);
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (prepare_state != STATE_RISE_SLOW)
    {
        goto end;
    }
    else if (state_handle->current_tick < INPUT_DATA_HZ * 2)
        return STATE_RISE_SLOW;

    static unsigned short temp_max = 0, temp_min = 40960;
    unsigned short diff = state_handle->last_temp - state_handle->last_temp_data[state_handle->temp_data_size - 10];
    if (diff > temp_max)
    {
        temp_max = diff;
    }
    if (diff < temp_min)
    {
        temp_min = diff;
    }
    mlogPrintf("%s,%s rise value:%d max:%d min:%d \n", __func__, state_info[STATE_RISE_SLOW], diff, temp_max, temp_min);

    if (state_handle->last_temp >= 700)
    {
        if (diff < 100)
        {
            if (state_hood.gear < GEAR_TWO)
                gear_change(2, 1, state_info[STATE_RISE_SLOW], state_handle);
        }
        else
        {
            if (state_handle->last_temp >= 1800)
            {
                if (state_hood.gear < GEAR_TWO)
                    gear_change(2, 1, state_info[STATE_RISE_SLOW], state_handle);
            }
            else
            {
                if (state_hood.gear == GEAR_CLOSE)
                {
                    gear_change(1, 1, state_info[STATE_RISE_SLOW], state_handle);
                }
            }
        }
    }
    else if (state_handle->last_temp >= 500)
    {
        if (state_handle->shake_exit_tick != 0 && state_handle->shake_exit_tick + SHAKE_EXIT_TICK > state_handle->total_tick)
        {
            gear_change(2, 1, state_info[STATE_RISE_SLOW], state_handle);
        }
        else
        {
            gear_change(1, 1, state_info[STATE_RISE_SLOW], state_handle);
        }
    }
    // else
    // {
    //     if (state_hood.gear == GEAR_CLOSE)
    //     {
    //         gear_change(1, 1, state_info[STATE_RISE_SLOW], state_handle);
    //     }
    // }
end:
    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (prepare_state == STATE_GENTLE)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 3 > state_handle->current_tick)
            return STATE_RISE_SLOW;
    }

    return prepare_state;
}
/***********************************************************
 * 风随烟动缓降处理函数
 ***********************************************************/
static int state_func_down_slow(unsigned char prepare_state, state_handle_t *state_handle) // 缓降
{
    if (state_handle->current_tick == 0)
    {
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_DOWN_SLOW]);
        state_handle->current_tick = 1;
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (prepare_state != STATE_DOWN_SLOW)
    {
        return prepare_state;
    }
    else if (state_handle->current_tick < INPUT_DATA_HZ * 6)
        return STATE_DOWN_SLOW;

    if (state_handle->last_temp >= 650)
    {
        if (state_hood.gear > GEAR_TWO)
        {
            gear_change(2, 0, state_info[STATE_DOWN_SLOW], state_handle);
        }
    }
    else
    {
        gear_change(1, 0, state_info[STATE_DOWN_SLOW], state_handle);
    }
    return STATE_DOWN_SLOW;
}
/***********************************************************
 * 风随烟动平缓处理函数
 ***********************************************************/
static int state_func_gentle(unsigned char prepare_state, state_handle_t *state_handle) // 平缓
{
    if (state_handle->current_tick == 0)
    {
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_GENTLE]);
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 0;
#ifdef BOIL_ENABLE
        state_handle->boil_start_tick = 0;
        state_handle->boil_stop_count = 0;
#endif
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }
    if (prepare_state != STATE_GENTLE)
    {
        goto end;
    }
    else if (state_handle->current_tick < INPUT_DATA_HZ * 3)
        return STATE_GENTLE;

    if (state_handle->state_jump_temp >= 650 && state_handle->state_jump_temp < 1200)
    {
        gear_change(2, 1, state_info[STATE_GENTLE], state_handle);
    }
    else if (state_handle->state_jump_temp < 600)
    {
        if (state_handle->shake_exit_tick != 0 && state_handle->shake_exit_tick + SHAKE_EXIT_TICK > state_handle->total_tick)
        {
            gear_change(2, 0, state_info[STATE_GENTLE], state_handle);
        }
        else
        {
            gear_change(1, 0, state_info[STATE_GENTLE], state_handle);
        }
    }
    else
    {
    }
#ifdef BOIL_ENABLE
    if (state_handle->state_jump_temp > BOIL_LOW_TEMP && state_handle->state_jump_temp <= BOIL_HIGH_TEMP)
    {
        if ((state_handle->boil_gengle_state >= 2 && state_handle->boil_gengle_state <= 10))
        {
            if (state_handle->boil_start_tick == 0)
            {
                state_handle->boil_start_tick = state_handle->current_tick;
                state_handle->boil_stop_count = 1;
            }
        }
        else if ((state_handle->boil_gengle_small_state >= 8 && state_handle->boil_gengle_small_state <= 10))
        {
            if (state_handle->boil_start_tick == 0)
            {
                state_handle->boil_start_tick = state_handle->current_tick;
            }
        }
        else
        {
            if (state_handle->boil_stop_count == 0)
                state_handle->boil_start_tick = 0;
            if (state_handle->boil_stop_count > 0)
            {
                --state_handle->boil_stop_count;
            }
        }
    }
    else
    {
        state_handle->boil_stop_count = 0;
        state_handle->boil_start_tick = 0;
    }

    mlogPrintf("-----------------%s,%s current_tick:%d boil_start_tick:%d boil_gengle_state:%d boil_gengle_small_state:%d\n", __func__, state_info[STATE_GENTLE], state_handle->current_tick, state_handle->boil_start_tick, state_handle->boil_gengle_state, state_handle->boil_gengle_small_state);
    if (state_handle->boil_start_tick > 0 && state_handle->current_tick >= state_handle->boil_start_tick + BOIL_START_TICK)
    {
        state_handle->boil_start_tick = 0;
        return STATE_BOIL;
    }
#endif
end:
    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (prepare_state == STATE_RISE_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 5 > state_handle->current_tick)
            return STATE_GENTLE;
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 10 > state_handle->current_tick)
            return STATE_GENTLE;
    }

    return prepare_state;
}

/***********************************************************
 * 风随烟动空闲处理函数
 ***********************************************************/
static int state_func_idle(unsigned char prepare_state, state_handle_t *state_handle) // 空闲
{
    if (state_handle->current_tick == 0)
    {
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_IDLE]);
        state_handle->current_tick = 1;
    }
    return prepare_state;
}
/***********************************************************
 * 移锅小火处理函数
 ***********************************************************/
static int state_func_pan_fire(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (!state_handle->pan_fire_switch || state_handle->temp_control_switch)
    {
        goto exit;
    }

    if (state_handle->current_tick == 0)
    {
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_PAN_FIRE]);
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 0;
#ifdef FIRE_CONFIRM_ENABLE
        state_handle->pan_fire_state = PAN_FIRE_CLOSE;
#else
        state_handle->pan_fire_state = PAN_FIRE_ENTER;
        set_fire_gear(FIRE_SMALL, state_handle, 0);
#endif
        state_handle->pan_fire_tick = 0;

        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }
    mlogPrintf("%s,%s pan_fire_state:%d pan_fire_tick:%d current_tick:%d\n", __func__, state_info[STATE_PAN_FIRE], state_handle->pan_fire_state, state_handle->pan_fire_tick, state_handle->current_tick);

    if (state_handle->pan_fire_state == PAN_FIRE_CLOSE) // 假设移锅小火
    {
        mlogPrintf("%s,%s %s\n", __func__, state_info[STATE_PAN_FIRE], "set small fire");
        set_fire_gear(FIRE_SMALL, state_handle, 0);
        if (state_handle->total_tick < INPUT_DATA_HZ * 20)
        {
            state_handle->pan_fire_tick = state_handle->current_tick + PAN_FIRE_DOWN_JUMP_EXIT_TICK / 2;
            state_handle->pan_fire_state = PAN_FIRE_ENTER;
        }
        else
        {
            state_handle->pan_fire_tick = state_handle->current_tick;
            state_handle->pan_fire_state = PAN_FIRE_START;
        }
    }
    else if (state_handle->pan_fire_state == PAN_FIRE_ENTER) // 开关小火，温度跳降，确定是移锅小火
    {
        if (state_handle->pan_fire_enter_start_tick < INPUT_DATA_HZ * state_handle->pan_fire_close_delay_tick)
        {
            ++state_handle->pan_fire_enter_start_tick;
            if (state_handle->pan_fire_enter_start_tick == INPUT_DATA_HZ * state_handle->pan_fire_close_delay_tick)
            {
                if (cook_assist_remind_cb != NULL)
                    cook_assist_remind_cb(1);
            }
        }
        if (state_handle->pan_fire_tick == 0)
        {
            state_handle->pan_fire_tick = state_handle->current_tick;
            gear_change(1, 0, state_info[STATE_PAN_FIRE], state_handle);
        }
        else if (state_handle->total_tick >= INPUT_DATA_HZ * 20 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 1] < 900 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 2] < 900 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 3] < 900 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 4] < 900)
        {
            goto exit;
        }
    }
    else if (state_handle->pan_fire_state == PAN_FIRE_START && state_handle->pan_fire_tick + PAN_FIRE_ENTER_TICK < state_handle->current_tick) // 开关小火，规定时间温度没有跳降，不是移锅小火
    {
        mlogPrintf("%s, %s pan_fire_state:%d\n", __func__, "set small fire errors", state_handle->pan_fire_state);
        // set_fire_gear(FIRE_BIG, state_handle, 0);
        // state_handle->pan_fire_state = PAN_FIRE_ERROR_CLOSE;
        goto exit;
    }

    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (prepare_state == STATE_RISE_SLOW)
    {
        mlogPrintf("%s,%s pan_fire enter total_tick:%d last_prepare_state_tick:%d current_tick:%d\n", __func__, state_info[STATE_RISE_SLOW], state_handle->total_tick, state_handle->last_prepare_state_tick, state_handle->current_tick);
        if (state_handle->total_tick <= INPUT_DATA_HZ * 12)
        {
            state_handle->last_prepare_state_tick = state_handle->current_tick;
            return STATE_PAN_FIRE;
        }
        else
        {
            if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 4 > state_handle->current_tick)
                return STATE_PAN_FIRE;
        }
        mlogPrintf("%s pan_fire quit total_tick:%d\n", __func__, state_handle->total_tick);
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        // if (state_handle->last_temp > 1800)
        // {
        //     state_handle->last_prepare_state_tick = state_handle->current_tick;
        //     return STATE_PAN_FIRE;
        // }
        // else
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 5 > state_handle->current_tick)
            return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        // if (state_handle->last_temp > 1800)
        // {
        //     state_handle->last_prepare_state_tick = state_handle->current_tick;
        //     return STATE_PAN_FIRE;
        // }
        // else
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 5 > state_handle->current_tick)
            return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_RISE_JUMP)
    {
        if (state_handle->pan_fire_state == PAN_FIRE_ENTER)
        {
            if (state_handle->last_prepare_state_tick + 7 > state_handle->current_tick)
                return STATE_PAN_FIRE;
            if (state_handle->state_jump_diff > 500)
            {
                state_handle->pan_fire_rise_jump_exit_lock_tick = PAN_FIRE_RISE_JUMP_EXIT_LOCK_TICK;
                goto exit;
            }
        }
        else
        {
            if (state_handle->last_prepare_state_tick + 7 >= state_handle->current_tick)
                return STATE_PAN_FIRE;
        }
    }
    else if (prepare_state == STATE_DOWN_JUMP)
    {
        mlogPrintf("%s,pan_fire state_jump_diff:%d\n", __func__, state_handle->state_jump_diff);
        if (state_handle->pan_fire_state == PAN_FIRE_START && state_handle->pan_fire_tick + PAN_FIRE_ENTER_TICK >= state_handle->current_tick)
        {
            if (state_handle->state_jump_diff > PAN_FIRE_HIGH_TEMP)
            {
                goto exit;
            }
            if (state_handle->pan_fire_enter_type == 1) // 翻炒进入移锅小火
            {
                if (state_handle->last_temp >= 800)
                {
                    state_handle->pan_fire_state = PAN_FIRE_ENTER;
                    state_handle->pan_fire_tick = 0;
                    return STATE_PAN_FIRE;
                }
            }
            else
            {
                state_handle->pan_fire_state = PAN_FIRE_ENTER;
                state_handle->pan_fire_tick = 0;
                return STATE_PAN_FIRE;
            }
        }
        else if (state_handle->pan_fire_state == PAN_FIRE_ENTER)
        {
            if (state_handle->pan_fire_tick + PAN_FIRE_DOWN_JUMP_EXIT_TICK > state_handle->current_tick)
                return STATE_PAN_FIRE;
        }
    }
    else if (prepare_state == STATE_SHAKE)
    {
        return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_PAN_FIRE)
    {
        return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_IDLE)
    {
        return STATE_PAN_FIRE;
    }

    if (state_handle->pan_fire_state == PAN_FIRE_ENTER && state_handle->pan_fire_high_temp_exit_lock_tick == 0)
    {
        if (state_handle->last_temp >= PAN_FIRE_LOW_TEMP)
            state_handle->pan_fire_high_temp_exit_lock_tick = PAN_FIRE_HIGH_TEMP_EXIT_LOCK_TICK;
    }
exit:
    state_handle->pan_fire_state = PAN_FIRE_CLOSE;
    state_handle->pan_fire_enter_start_tick = 0;
    if (state_handle->temp_control_switch == 0)
    {
        set_fire_gear(FIRE_BIG, state_handle, 0);
        mlogPrintf("%s,%s :%s\n", __func__, state_info[STATE_PAN_FIRE], "set big fire");
    }
    return prepare_state;
}
#ifdef BOIL_ENABLE
static int state_func_boil(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (state_handle->current_tick == 0)
    {
        mlogPrintf("%s,enter state:%s\n", __func__, state_info[STATE_BOIL]);
        state_handle->current_tick = 1;
        set_fire_gear(FIRE_SMALL, state_handle, 0);
        gear_change(2, 0, state_info[STATE_BOIL], state_handle);
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (state_handle->last_temp <= BOIL_LOW_TEMP + 4)
    {
    }
    else if (prepare_state == STATE_RISE_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 10 > state_handle->current_tick)
            return STATE_BOIL;
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 2 > state_handle->current_tick)
            return STATE_BOIL;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        return STATE_BOIL;
    }
    set_fire_gear(FIRE_BIG, state_handle, 0);
    return prepare_state;
}
#endif
static state_func_def g_state_func_handle[] = {state_func_shake, state_func_down_jump, state_func_rise_jump, state_func_rise_slow, state_func_down_slow, state_func_gentle, state_func_idle, state_func_pan_fire
#ifdef BOIL_ENABLE
                                               ,
                                               state_func_boil
#endif
};
/***********************************************************
 * 重新初始化函数
 ***********************************************************/
void cook_assistant_reinit(state_handle_t *state_handle)
{
    mlogPrintf("%s,cook_assistant_reinit\n", __func__);
    state_handle->state = STATE_IDLE;

    state_handle->pan_fire_state = PAN_FIRE_CLOSE;
    state_handle->pan_fire_high_temp_exit_lock_tick = 0;
    state_handle->pan_fire_rise_jump_exit_lock_tick = 0;
    state_handle->pan_fire_first_error = 0;
    state_handle->pan_fire_error_lock_tick = 0;

    state_handle->temp_control_first = 0;
    state_handle->temp_control_lock_countdown = 0;

    state_handle->shake_permit_start_tick = 0;
    state_handle->shake_exit_tick = 0;
    state_handle->shake_long = 0;

    // state_handle->ignition_switch = 0;
    // state_handle->ignition_switch_close_temp = 0;

    state_handle->current_tick = 0;
    state_handle->total_tick = 0;

    set_fire_gear(FIRE_BIG, state_handle, 1);
    state_handle->fire_gear = FIRE_BIG;
    state_handle->hood_gear = GEAR_CLOSE;

    // state_hood.gear = GEAR_CLOSE;
    // state_hood.prepare_gear = GEAR_INVALID;
    // state_hood.gear_tick = 0;
}
/***********************************************************
 * 初始化函数
 ***********************************************************/
void cook_assistant_init(enum INPUT_DIR input_dir)
{
    mlogPrintf("%s,cook_assistant_init\n", __func__);
    state_handle_t *state_handle = get_input_handle(input_dir);
    ring_buffer_init(&state_handle->ring_buffer, STATE_JUDGE_DATA_SIZE, 2);
    state_handle->input_dir = input_dir;
    cook_assistant_reinit(state_handle);

    state_handle->ignition_switch = 0;
    state_handle->ignition_switch_close_temp = 0;

    if (input_dir == INPUT_LEFT)
        set_pan_fire_switch(0, input_dir);
    else
        set_pan_fire_switch(0, input_dir);

    set_temp_control_target_temp(150, input_dir);

    state_handle->pan_fire_close_delay_tick = 180;
}
/***********************************************************
 * 状态转化处理函数
 ***********************************************************/
static int status_judge(state_handle_t *state_handle, const unsigned short *data, const int len)
{
#define START (2)

    mlogPrintf("%s,total_tick:%d pan_fire_state:%d pan_fire_first_error:%d\n", __func__, state_handle->total_tick, state_handle->pan_fire_state, state_handle->pan_fire_first_error);
    mlogPrintf("%s,ignition_switch_close_temp:%d last_temp:%d\n", __func__, state_handle->ignition_switch_close_temp, state_handle->last_temp);
    // STATE_PAN_FIRE
    if (!state_handle->temp_control_switch && state_handle->pan_fire_switch && state_handle->pan_fire_state == PAN_FIRE_CLOSE && state_handle->fire_gear == FIRE_BIG)
    {
        if (state_handle->total_tick <= 5 * INPUT_DATA_HZ && data[len - 1] > 1200 && (data[len - 1] > state_handle->ignition_switch_close_temp + 500))
        {
            mlogPrintf("%s judge STATE_PAN_FIRE\n", __func__);
            state_handle->pan_fire_enter_type = 0;
            return STATE_PAN_FIRE;
        }
        else if (data[len - 1] >= 3800)
        {
            mlogPrintf("%s judge STATE_PAN_FIRE\n", __func__);
            state_handle->pan_fire_enter_type = 0;
            return STATE_PAN_FIRE;
        }
    }

    int i;
    int average = 0, gentle_average = 0, gentle_average_before = 0, gentle_average_after = 0;

    for (i = 0; i < len; ++i)
    {
        average += data[i];
        //        mlogPrintf("%d ", data[i]);
        if (i < len / 2)
        {
            gentle_average_before += data[i];
        }
        else
        {
            gentle_average_after += data[i];
        }
        if (i >= START)
        {
            gentle_average += data[i];
        }
    }
    average /= len;
    gentle_average /= (len - START);
    gentle_average_before /= (len / 2);
    gentle_average_after /= (len / 2);
    //    mlogPrintf("\n");
    mlogPrintf("%s,average:%d,gentle_average:%d,gentle_average_before:%d,gentle_average_after:%d\n", __func__, average, gentle_average, gentle_average_before, gentle_average_after);

    // STATE_SHAKE
    unsigned short min = data[len - 1];
    unsigned short max = data[len - 1];
    char data_average = data[len - 1] > average;
    char data_average_count = 0;

    char data_trend = data[len - 2] > data[len - 1];
    char data_trend_count = 0;

    mlogPrintf("%s,pan_fire_high_temp_exit_lock_tick:%d\n", __func__, state_handle->pan_fire_high_temp_exit_lock_tick);
    if (!state_handle->temp_control_switch && state_handle->pan_fire_switch && state_handle->pan_fire_state == PAN_FIRE_CLOSE && state_handle->fire_gear == FIRE_BIG && state_handle->pan_fire_high_temp_exit_lock_tick == 0)
    {
        for (i = len - 2; i > 0; --i)
        {
            if (data[i] < min)
                min = data[i];
            else if (data[i] > max)
                max = data[i];

            if (data[i - 1] > data[i])
            {
                if (data_trend == 0)
                    ++data_trend_count;
                data_trend = 1;
            }
            else
            {
                if (data_trend == 1)
                    ++data_trend_count;
                data_trend = 0;
            }
            if (data_trend_count >= 3)
            {
                mlogPrintf("%s,shake pan_fire max:%d min:%d\n", __func__, max, min);
                if (state_handle->total_tick <= INPUT_DATA_HZ * 10 && state_handle->pan_fire_first_error == 0)
                {
                    if (average < 1700 || max - min < 100)
                        break;
                }
                else if (state_handle->total_tick <= START_FIRST_MINUTE_TICK && state_handle->pan_fire_first_error == 0)
                {
                    if (average < PAN_FIRE_LOW_TEMP || max - min < 100)
                        break;
                }
                else if (state_handle->total_tick <= INPUT_DATA_HZ * 80 && state_handle->pan_fire_first_error == 0)
                {
                    if (average < PAN_FIRE_HIGH_TEMP || max - min < 100)
                        break;
                }
                else
                {
                    if (average < 2500 || max - min < 100)
                        break;
                }
                // if (max - min < 1500)
                // {
                state_handle->pan_fire_enter_type = 1;
                return STATE_PAN_FIRE;
                // }
                // break;
            }
        }
    }

    mlogPrintf("%s,shake_permit_start_tick:%d\n", __func__, state_handle->shake_permit_start_tick);
    if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE && state_handle->shake_permit_start_tick != 0 && state_handle->total_tick > 10 * INPUT_DATA_HZ && state_handle->total_tick < state_handle->shake_permit_start_tick + INPUT_DATA_HZ * 40)
    {
        min = data[len - 1];
        max = data[len - 1];
        for (i = len - 2; i > 0; --i)
        {
            if (data[i] < min)
                min = data[i];
            else if (data[i] > max)
                max = data[i];

            if (data[i] > average)
            {
                if (data_average == 0)
                    ++data_average_count;
                data_average = 1;
            }
            else
            {
                if (data_average > 0)
                    ++data_average_count;
                data_average = 0;
            }

            if (data_average_count >= 4)
            {
                mlogPrintf("%s,shake max:%d min:%d\n", __func__, max, min);
                if (average > 430)
                {
                    if (max - min > 80)
                    {
                        state_handle->shake_permit_start_tick = state_handle->total_tick;
                        state_handle->state_jump_temp = average;
                        return STATE_SHAKE;
                    }
                }
                else
                {
                    break;
                }
            }
        }
    }

    // STATE_DOWN_JUMP
    signed short diff0_0, diff0_1, diff0_2, diff1_0, diff1_1, diff1_2, diff2_0, diff2_1, diff2_2;
    signed short before, after;
    signed short JUMP_RISE_VALUE = 250, JUMP_DOWN_VALUE = -250;

    if ((state_handle->state == STATE_GENTLE || (state_handle->state == STATE_RISE_SLOW && data[len - 1] - data[0] < 50 && data[len - 1] - data[0] > 0)) && state_handle->current_tick >= INPUT_DATA_HZ * 2 && state_handle->last_temp < 1000 && state_handle->last_temp > 650)
    {
#ifndef BOIL_ENABLE
        // JUMP_DOWN_VALUE = -150;
        JUMP_RISE_VALUE = 40;
#endif
    }
    else if (state_handle->state == STATE_SHAKE)
    {
        JUMP_DOWN_VALUE = -350;
        JUMP_RISE_VALUE = 350;
    }

    for (i = len - 1; i >= 6; --i)
    {
        // if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE && state_handle->state != STATE_GENTLE)
        if (0) // JUMP_RISE_VALUE <= 50
        {
            before = data[i - 4];
            after = data[i - 1];
            diff0_0 = data[i] - data[i - 3];
            diff0_1 = data[i] - data[i - 4];
            diff0_2 = data[i] - data[i - 5];

            diff1_0 = data[i - 1] - data[i - 3];
            diff1_1 = data[i - 1] - data[i - 4];
            diff1_2 = data[i - 1] - data[i - 5];

            diff2_0 = data[i - 2] - data[i - 3];
            diff2_1 = data[i - 2] - data[i - 4];
            diff2_2 = data[i - 2] - data[i - 5];
            // signed short diff_jump = abs(diff2_0);
            // if (abs(data[i - 3] - data[i - 5]) >= diff_jump * 0.7)
            // {
            //     continue;
            // }
            // else if (abs(data[i] - data[i - 2]) >= diff_jump * 0.6)
            // {
            //     continue;
            // }
            // if (diff0_2 < 0)
            // {
            //     if (abs(data[i] - data[i - 5]) >= diff_jump * 1.7)
            //     {
            //         continue;
            //     }
            // }
            // else
            // {
            //     if (state_handle->state == STATE_RISE_SLOW || state_handle->state == STATE_RISE_JUMP)
            //     {
            //         if (abs(data[i] - data[i - 5]) >= diff_jump * 1.3)
            //         {
            //             continue;
            //         }
            //     }
            //     else
            //     {
            //         if (abs(data[i] - data[i - 5]) >= diff_jump * 1.5)
            //         {
            //             continue;
            //         }
            //     }
            // }
        }
        else
        {
            before = data[i - 5];
            after = data[i - 1];
            diff0_0 = data[i] - data[i - 4];
            diff0_1 = data[i] - data[i - 5];
            diff0_2 = data[i] - data[i - 6];

            diff1_0 = data[i - 1] - data[i - 4];
            diff1_1 = data[i - 1] - data[i - 5];
            diff1_2 = data[i - 1] - data[i - 6];

            diff2_0 = data[i - 2] - data[i - 4];
            diff2_1 = data[i - 2] - data[i - 5];
            diff2_2 = data[i - 2] - data[i - 6];

            // signed short diff_jump = abs(diff2_0);
            // if (fabs(data[i - 4] - data[i - 6]) >= diff_jump * 0.7)
            // {
            //     continue;
            // }
            // else if (fabs(data[i] - data[i - 2]) >= diff_jump * 0.5)
            // {
            //     continue;
            // }
            // else if (fabs(data[i] - data[i - 6]) >= diff_jump * 1.7)
            // {
            //     continue;
            // }
        }
        mlogPrintf("%s,i:%d JUMP_DOWN_VALUE:%d JUMP_RISE_VALUE:%d diff2_0:%d\n", __func__, i, JUMP_DOWN_VALUE, JUMP_RISE_VALUE, diff2_0);
        state_handle->state_jump_diff = abs(diff2_0);
        if (diff2_0 < 0)
        {
            unsigned char jump_down = diff0_0 < JUMP_DOWN_VALUE && diff0_1 < JUMP_DOWN_VALUE && diff0_2 < JUMP_DOWN_VALUE;
            jump_down += diff1_0 < JUMP_DOWN_VALUE && diff1_1 < JUMP_DOWN_VALUE && diff1_2 < JUMP_DOWN_VALUE;
            jump_down += diff2_0 < JUMP_DOWN_VALUE && diff2_1 < JUMP_DOWN_VALUE && diff2_2 < JUMP_DOWN_VALUE;

            if (jump_down >= 3)
            {
                state_handle->state_jump_temp = before;
                mlogPrintf("%s,judge STATE_DOWN_JUMP state_jump_temp:%d\n", __func__, state_handle->state_jump_temp);

                if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
                    state_handle->pan_fire_state = PAN_FIRE_CLOSE;

                return STATE_DOWN_JUMP;
            }
        }
        else
        {
            // STATE_RISE_JUMP
            unsigned char jump_rise = diff0_0 >= JUMP_RISE_VALUE && diff0_1 >= JUMP_RISE_VALUE && diff0_2 >= JUMP_RISE_VALUE;
            jump_rise += diff1_0 >= JUMP_RISE_VALUE && diff1_1 >= JUMP_RISE_VALUE && diff1_2 >= JUMP_RISE_VALUE;
            jump_rise += diff2_0 >= JUMP_RISE_VALUE && diff2_1 >= JUMP_RISE_VALUE && diff2_2 >= JUMP_RISE_VALUE;
            if ((JUMP_RISE_VALUE <= 50 && jump_rise >= 2) || jump_rise >= 3)
            {
                state_handle->state_jump_temp = after;
                mlogPrintf("%s,i:%d judge STATE_RISE_JUMP state_jump_temp:%d %d %d\n", __func__, i, state_handle->state_jump_temp, state_handle->fire_gear, state_handle->pan_fire_rise_jump_exit_lock_tick);

                if (!state_handle->temp_control_switch && state_handle->pan_fire_switch && state_handle->pan_fire_state == PAN_FIRE_CLOSE && state_handle->fire_gear == FIRE_BIG && state_handle->pan_fire_rise_jump_exit_lock_tick == 0)
                {
                    if (abs(diff2_0) > 500)
                    {

                        if (state_handle->total_tick <= INPUT_DATA_HZ * 10)
                        {
                            if (after < 1500)
                                continue;
                        }
                        else if (state_handle->total_tick <= INPUT_DATA_HZ * 40)
                        {
                            if (after < 1800)
                                continue;
                        }
                        else if (state_handle->total_tick <= INPUT_DATA_HZ * 80)
                        {
                            if (after < 2000)
                                continue;
                        }
                        else if (after < 2200)
                            continue;

                        if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
                            state_handle->pan_fire_state = PAN_FIRE_CLOSE;

                        state_handle->pan_fire_enter_type = 0;
                        return STATE_PAN_FIRE;
                    }
                }
                signed short diff_jump = abs(diff2_0);
                if (diff_jump > 6 && abs(data[i] - data[i - 6]) > diff_jump * 2.5)
                {
                    continue;
                }
                if (after > 650)
                {
                    if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
                        state_handle->pan_fire_state = PAN_FIRE_CLOSE;

                    if ((state_hood.gear > GEAR_CLOSE))
                        return STATE_RISE_JUMP;
                }
            }
        }
    }
    // STATE_RISE_SLOW
#ifdef BOIL_ENABLE
    state_handle->boil_gengle_state = 0;
    state_handle->boil_gengle_small_state = 0;
#endif
#define STEP (2)
    signed short slow_rise_value = -200;

    signed short diff0 = data[START] - data[len - 1];
    signed short diff1 = data[START] - data[START + STEP];
    signed short diff2 = data[START + STEP] - data[START + STEP * 2];
    // signed short diff3 = data[START+STEP * 2] - data[START+STEP * 3];
    if (diff0 < 0)
    {
        // if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE)
        // {
        unsigned char slow_rise = diff0 <= -10 && diff0 > slow_rise_value;
        slow_rise += diff1 <= 0;
        slow_rise += diff2 <= 0;
        // slow_rise += diff3 <= 0;
        if (slow_rise >= 3)
        {
#ifdef BOIL_ENABLE
            // state_handle->boil_gengle_state=0x0f;
            state_handle->boil_gengle_small_state = 0x0f;
#endif
            for (i = START + 1; i < len - 1; ++i)
            {
                if (data[START] > data[i])
                    break;
                if (data[len - 1] < data[i])
                    break;
            }
            if (i >= len - 1)
                return STATE_RISE_SLOW;
        }
        // }
        // else
        // {
        //     mlogPrintf("%s,status_judge RISE_SLOW pan_fire_state:%d\n", __func__, state_handle->pan_fire_state);
        //     if (data[len-1] > data[START])
        //     {
        //         for (i = START; i < len-1; ++i)
        //         {
        //             if (data[i] > data[i + 1])
        //                 break;
        //         }
        //         if (i >= len-1)
        //             return STATE_RISE_SLOW;
        //     }
        // }
    }
    else
    {
        // STATE_DOWN_SLOW
        if (state_hood.gear > GEAR_CLOSE)
        {
            unsigned char slow_down = diff0 > 10 && diff0 < 100;
            slow_down += diff1 >= 0;
            slow_down += diff2 >= 0;
            // slow_down += diff3 >= 0;
            if (slow_down >= 3)
            {
                for (i = START + 1; i < len - 1; ++i)
                {
                    if (data[START] < data[i])
                        break;
                    if (data[len - 1] > data[i])
                        break;
                }
                if (i >= len - 1)
                    return STATE_DOWN_SLOW;
            }
        }
    }
    // STATE_GENTLE
    if (state_hood.gear > GEAR_CLOSE || state_handle->pan_fire_state == PAN_FIRE_ENTER)
    {
#ifdef BOIL_ENABLE
        for (i = len - 1; i >= 0; --i)
        {
            if (abs(data[i] - average) >= 15)
                ++state_handle->boil_gengle_state;
            if (abs(data[i] - average) < 9)
                ++state_handle->boil_gengle_small_state;
        }
#endif
        char gentle_range;
        if (state_handle->pan_fire_state == PAN_FIRE_ENTER)
        {
            gentle_range = 13;
        }
        else
        {
            gentle_range = 17;
        }

        for (i = len - 1; i >= START; --i)
        {
            if (abs(data[i] - gentle_average) >= gentle_range)
            {
                break;
            }
        }
        if (i < START)
        {
            state_handle->state_jump_temp = gentle_average;
            return STATE_GENTLE;
        }
    }
    if (state_hood.gear > GEAR_CLOSE && state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE && state_handle->state != STATE_SHAKE)
    {
        if (abs(gentle_average_before - gentle_average_after) < 10)
        {
#ifdef BOIL_ENABLE
            state_handle->boil_gengle_state = 10;
#endif
            state_handle->state_jump_temp = gentle_average_before > gentle_average_after ? gentle_average_before : gentle_average_after;
            return STATE_GENTLE;
        }
    }

    return STATE_IDLE;
}
/***********************************************************
 * 辅助控火处理函数
 ***********************************************************/
static void temp_control_func(state_handle_t *state_handle)
{
    int i, average;

    if (state_handle->temp_control_lock_countdown < TEMP_CONTROL_LOCK_TICK)
    {
        ++state_handle->temp_control_lock_countdown;
    }
    // if (state_handle->pan_fire_state > PAN_FIRE_ERROR_CLOSE)
    // {
    //     return;
    // }
    // if (state_handle->last_temp < state_handle->temp_control_target_value / 2)
    // {
    //     for (i = 0; i < INPUT_DATA_HZ; ++i)
    //     {
    //         if (state_handle->last_temp_data[state_handle->temp_data_size - 1 - i] > state_handle->temp_control_target_value / 2)
    //         {
    //             break;
    //         }
    //     }
    //     if (i < INPUT_DATA_HZ)
    //     {
    //         return;
    //     }
    // }
    average = state_handle->last_temp;
    for (i = 1; i < 4; ++i)
    {
        average += state_handle->last_temp_data[state_handle->temp_data_size - 1 - i];
    }
    average /= 4;
    mlogPrintf("%s,temp_control_func average:%d\n", __func__, average);
    if (state_handle->temp_control_first == 0)
    {
        if (state_handle->temp_control_target_value - 40 < average)
        {
            state_handle->temp_control_enter_start_tick = INPUT_DATA_HZ * 60 * 2;
            state_handle->temp_control_first = 1;
            set_fire_gear(FIRE_SMALL, state_handle, 0);
            state_handle->temp_control_lock_countdown = 0;
        }
        else
        {
            set_fire_gear(FIRE_BIG, state_handle, 0);
            state_handle->temp_control_lock_countdown = 0;
        }
    }
    else
    {
        if (state_handle->temp_control_target_value + 10 < average)
        {
            if (state_handle->temp_control_lock_countdown >= TEMP_CONTROL_LOCK_TICK)
            {
                set_fire_gear(FIRE_SMALL, state_handle, 0);
                state_handle->temp_control_lock_countdown = 0;
            }
        }
        else if (state_handle->temp_control_target_value - 40 > average)
        {
            if (state_handle->temp_control_lock_countdown >= TEMP_CONTROL_LOCK_TICK || abs(state_handle->temp_control_target_value - average) > state_handle->temp_control_target_value * 0.1)
            {
                set_fire_gear(FIRE_BIG, state_handle, 0);
                state_handle->temp_control_lock_countdown = 0;
            }
        }
    }

    if (state_handle->temp_control_enter_start_tick < INPUT_DATA_HZ * 60 * 2)
    {
        ++state_handle->temp_control_enter_start_tick;
        if (state_handle->temp_control_enter_start_tick == INPUT_DATA_HZ * 60 * 2)
        {
            if (cook_assist_remind_cb != NULL)
                cook_assist_remind_cb(0);
        }
    }
}

/***********************************************************
 * 风随烟动逻辑状态切换函数
 ***********************************************************/
static void change_state(state_handle_t *state_handle)
{
    unsigned char prepare_state, next_state;

    state_handle->temp_data_size = ring_buffer_peek(&state_handle->ring_buffer, state_handle->last_temp_data, STATE_JUDGE_DATA_SIZE);
    state_handle->last_temp = state_handle->last_temp_data[state_handle->temp_data_size - 1];
    mlogPrintf("%s,last_temp:%d\n", __func__, state_handle->last_temp);
    MLOG_INT_PRINTF("ring_buffer_peek:", state_handle->last_temp_data, STATE_JUDGE_DATA_SIZE, 2);
    // 点火开关判断
    if (!state_handle->ignition_switch)
    {
        state_handle->temp_control_enter_start_tick = 0;
        state_handle->pan_fire_enter_start_tick = 0;
        state_handle->ignition_switch_close_temp = state_handle->last_temp_data[state_handle->temp_data_size - 5];
        return;
    }
    else
    {
        ++state_handle->total_tick;
    }

    if (state_hood.gear == GEAR_CLOSE)
    {
        if (state_handle->last_temp > 500 && state_handle->state != STATE_PAN_FIRE && state_handle->total_tick > INPUT_DATA_HZ * 20)
            gear_change(1, 1, "init gear_change", state_handle);
    }
    // 控温
    if (state_handle->temp_control_switch)
    {
        temp_control_func(state_handle);
    }
    if (state_hood.smart_smoke_switch == 0 && state_handle->pan_fire_switch == 0)
    {
        state_handle->state = STATE_IDLE;
        return;
    }
    // 翻炒允许温度
    if (state_handle->last_temp > SHAKE_PERMIT_TEMP)
    {
        state_handle->shake_permit_start_tick = state_handle->total_tick;
    }
    // 高温时，移锅小火退出后，锁定时间
    if (state_handle->pan_fire_high_temp_exit_lock_tick > 0)
    {
        --state_handle->pan_fire_high_temp_exit_lock_tick;
    }
    // 跳升退出移锅小火后，锁定时间
    if (state_handle->pan_fire_rise_jump_exit_lock_tick > 0)
    {
        --state_handle->pan_fire_rise_jump_exit_lock_tick;
    }
    // 移锅小火判断错误
    if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
    {
        // 开始一分钟内,移锅小火第一次判断错误
        if (state_handle->total_tick < START_FIRST_MINUTE_TICK && state_handle->pan_fire_first_error == 0)
        {
            state_handle->pan_fire_first_error = 1;
        }
        // 移锅小火判断错误，锁定时间
        if (state_handle->pan_fire_error_lock_tick == 0)
            state_handle->pan_fire_error_lock_tick = state_handle->total_tick;
        else
        {
            if (state_handle->pan_fire_error_lock_tick + INPUT_DATA_HZ * 20 < state_handle->total_tick)
            {
                state_handle->pan_fire_error_lock_tick = 0;
                state_handle->pan_fire_state = PAN_FIRE_CLOSE;
            }
        }
    }

    // 下一个状态判断
    prepare_state = status_judge(state_handle, &state_handle->last_temp_data[state_handle->temp_data_size - STATE_JUDGE_DATA_SIZE], STATE_JUDGE_DATA_SIZE);
    mlogPrintf("%s,prepare_state:%s\n", __func__, state_info[prepare_state]);

    if (prepare_state == STATE_IDLE && state_handle->state != STATE_PAN_FIRE)
    {
        if (state_handle->current_tick != 0)
            ++state_handle->current_tick;
        return;
    }

    // 状态切换
    next_state = g_state_func_handle[state_handle->state](prepare_state, state_handle);
    if (state_handle->state != next_state)
    {
        state_handle->current_tick = 0;
        g_state_func_handle[next_state](state_handle->state, state_handle);
        state_handle->state = next_state;
    }
}
/***********************************************************
 * 烟机预档位切换函数
 * 1.主要完成升降档延时切换
 ***********************************************************/
void prepare_gear_change_task()
{
    if (!state_hood.work_mode)
        return;
    mlogPrintf("%s,prepare_gear:%d gear_tick:%d\n", __func__, state_hood.prepare_gear, state_hood.gear_tick);
    if (state_hood.prepare_gear != GEAR_INVALID)
    {
        gear_change(state_hood.prepare_gear, 0, "", NULL);
    }
    if (state_hood.gear_tick <= g_gear_delay_time)
    {
        ++state_hood.gear_tick;
    }

    if (state_hood.close_delay_tick > 0)
    {
        --state_hood.close_delay_tick;
        mlogPrintf("%s,close_delay_tick:%d\n", __func__, state_hood.close_delay_tick);
        if (state_hood.close_delay_tick == 0)
        {
            gear_change(0, 3, "ignition switch close", NULL);
        }
    }
}
/***********************************************************
 * 输入函数
 ***********************************************************/
void cook_assistant_input(enum INPUT_DIR input_dir, unsigned short temp, unsigned short environment_temp)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    if (oil_temp_cb != NULL)
        oil_temp_cb(temp, input_dir);

    if (!state_hood.work_mode)
        return;
    if (environment_temp > 750)
    {
        if (state_hood.lock == 0)
            gear_change(3, 4, "environment_temp lock", state_handle);
    }
    else
    {
        if (state_hood.lock > 0)
            gear_change(2, 5, "environment_temp unlock", state_handle);
    }

    ring_buffer_push(&state_handle->ring_buffer, &temp);
    if (!ring_buffer_is_full(&state_handle->ring_buffer))
        return;

    mlogPrintf("%s,%s input_dir:%d temp:%d total_tick:%d current_tick:%d current status:%s current gear:%d ignition_switch:%d environment_temp:%d state_hood lock:%d\n", get_current_time_format(), __func__, input_dir, temp, state_handle->total_tick, state_handle->current_tick, state_info[state_handle->state], state_hood.gear, state_handle->ignition_switch, environment_temp, state_hood.lock);
    change_state(state_handle);
}
