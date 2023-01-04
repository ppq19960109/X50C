#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mlog.h"
#include "cloud.h"
#include "fsyd.h"

static int (*work_mode_cb)(const unsigned char);
void register_work_mode_cb(int (*cb)(const unsigned char))
{
    work_mode_cb = cb;
}
static int (*smart_smoke_switch_cb)(const unsigned char);
void register_smart_smoke_switch_cb(int (*cb)(const unsigned char))
{
    smart_smoke_switch_cb = cb;
}
static int (*pan_fire_switch_cb)(const unsigned char, enum INPUT_DIR);
void register_pan_fire_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR))
{
    pan_fire_switch_cb = cb;
}

static int (*temp_control_switch_cb)(const unsigned char, enum INPUT_DIR);
void register_temp_control_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR))
{
    temp_control_switch_cb = cb;
}
static int (*temp_control_target_temp_cb)(const unsigned short, enum INPUT_DIR);
void register_temp_control_target_temp_cb(int (*cb)(const unsigned short, enum INPUT_DIR))
{
    temp_control_target_temp_cb = cb;
}
static int (*temp_control_p_cb)(const float);
void register_temp_control_p_cb(int (*cb)(const float))
{
    temp_control_p_cb = cb;
}
static int (*temp_control_i_cb)(const float);
void register_temp_control_i_cb(int (*cb)(const float))
{
    temp_control_i_cb = cb;
}
static int (*temp_control_d_cb)(const float);
void register_temp_control_d_cb(int (*cb)(const float))
{
    temp_control_d_cb = cb;
}

void set_work_mode(unsigned char mode)
{
    state_hood_t *hood_handle = get_hood_handle();
    mlogPrintf("%s,set_work_mode:%d\n", get_current_time_format(), mode);
    if (hood_handle->work_mode == mode)
        return;
    hood_handle->work_mode = mode;

    if (hood_handle->work_mode)
    {
    }
    else
    {
        hood_handle->lock = 0;
        hood_handle->close_delay_tick = 0;
        hood_handle->prepare_gear = GEAR_INVALID;
        hood_handle->gear_tick = 0;
        state_handle_t *state_handle = get_input_handle(INPUT_LEFT);
        cook_assistant_reinit(state_handle);
        state_handle = get_input_handle(INPUT_RIGHT);
        cook_assistant_reinit(state_handle);
    }

    if (work_mode_cb != NULL)
        work_mode_cb(mode);
}

void set_smart_smoke_switch(unsigned char smart_smoke_switch)
{
    state_hood_t *hood_handle = get_hood_handle();
    mlogPrintf("%s,set_smart_smoke_switch:%d\n", get_current_time_format(), smart_smoke_switch);
    hood_handle->smart_smoke_switch = smart_smoke_switch;
    if (smart_smoke_switch)
    {
    }
    else
    {
        hood_handle->lock = 0;
        hood_handle->close_delay_tick = 0;
        hood_handle->prepare_gear = GEAR_INVALID;
        hood_handle->gear_tick = 0;

        state_handle_t *state_handle = get_input_handle(INPUT_LEFT);
        state_handle->hood_gear = GEAR_CLOSE;
        state_handle->shake_permit_start_tick = 0;
        state_handle->shake_exit_tick = 0;
        state_handle->shake_long = 0;
        state_handle = get_input_handle(INPUT_RIGHT);
        state_handle->hood_gear = GEAR_CLOSE;
        state_handle->shake_permit_start_tick = 0;
        state_handle->shake_exit_tick = 0;
        state_handle->shake_long = 0;
    }
    if (smart_smoke_switch_cb != NULL)
        smart_smoke_switch_cb(smart_smoke_switch);
}

void set_pan_fire_switch(unsigned char pan_fire_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    mlogPrintf("%s,set_pan_fire_switch:%d\n", get_current_time_format(), pan_fire_switch);
    state_handle->pan_fire_switch = pan_fire_switch;
    if (pan_fire_switch)
    {
    }
    else
    {
        if (state_handle->temp_control_switch == 0)
        {
            state_handle->pan_fire_state = PAN_FIRE_CLOSE;
            state_handle->pan_fire_high_temp_exit_lock_tick = 0;
            state_handle->pan_fire_rise_jump_exit_lock_tick = 0;
            state_handle->pan_fire_first_error = 0;
            state_handle->pan_fire_error_lock_tick = 0;
            set_fire_gear(FIRE_BIG, state_handle, 0);
        }
    }
    if (pan_fire_switch_cb != NULL)
        pan_fire_switch_cb(pan_fire_switch, input_dir);
}

void set_temp_control_switch(unsigned char temp_control_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    mlogPrintf("%s,set_temp_control_switch:%d\n", get_current_time_format(), temp_control_switch);
    state_handle->temp_control_switch = temp_control_switch;
    if (temp_control_switch)
    {
    }
    else
    {
        state_handle->temp_control_first = 0;
        state_handle->temp_control_lock_countdown = 0;
        if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE)
            set_fire_gear(FIRE_BIG, state_handle, 1);
    }
    state_handle->temp_control_enter_start_tick = 0;
    if (temp_control_switch_cb != NULL)
        temp_control_switch_cb(temp_control_switch, input_dir);
}

void set_temp_control_target_temp(unsigned short temp, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    state_handle->temp_control_target_value = temp * 10;
    mlogPrintf("%s,set_temp_control_target_temp:%d\n", get_current_time_format(), state_handle->temp_control_target_value);
    if (temp_control_target_temp_cb != NULL)
        temp_control_target_temp_cb(temp, input_dir);
    state_handle->temp_control_enter_start_tick = 0;
}
