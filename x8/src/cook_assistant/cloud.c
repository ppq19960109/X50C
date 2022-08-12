#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mlog.h"
#include "cloud.h"

static PID_ParaDef PID_Para;

PID_ParaDef *get_pid_para(void)
{
    return &PID_Para;
}

static int (*work_mode_cb)(const unsigned char);
void register_work_mode_cb(int (*cb)(const unsigned char))
{
    work_mode_cb = cb;
}
static int (*pan_fire_switch_cb)(const unsigned char, enum INPUT_DIR);
void register_pan_fire_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR))
{
    pan_fire_switch_cb = cb;
}
static int (*dry_switch_cb)(const unsigned char, enum INPUT_DIR);
void register_dry_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR))
{
    dry_switch_cb = cb;
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

void set_pan_fire_switch(unsigned char pan_fire_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    mlogPrintf("%s,set_pan_fire_switch:%d\n", get_current_time_format(), pan_fire_switch);
    state_handle->pan_fire_switch = pan_fire_switch;
    if (pan_fire_switch_cb != NULL)
        pan_fire_switch_cb(pan_fire_switch, input_dir);
}

void set_dry_switch(unsigned char dry_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    mlogPrintf("%s,set_dry_switch:%d\n", get_current_time_format(), dry_switch);
    state_handle->dry_switch = dry_switch;
    if (dry_switch_cb != NULL)
        dry_switch_cb(dry_switch, input_dir);
}

void set_temp_control_switch(unsigned char temp_control_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    mlogPrintf("%s,set_temp_control_switch:%d\n", get_current_time_format(), temp_control_switch);
    state_handle->temp_control_switch = temp_control_switch;
    if (state_handle->input_dir != INPUT_MAX)
    {
        if (temp_control_switch)
        {
            state_handle->PID_Type.Ek = 0;
            state_handle->PID_Type.Ek1 = 0;
            state_handle->PID_Type.Ek2 = 0;
            state_handle->PID_Type.LocSum = 0;
        }
        else
        {
            if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE && state_handle->state != STATE_DRY)
                set_fire_gear(FIRE_BIG, state_handle, 1);
        }
    }
    if (temp_control_switch_cb != NULL)
        temp_control_switch_cb(temp_control_switch, input_dir);
}

void set_temp_control_target_temp(unsigned short temp, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    state_handle->PID_Type.Target_value = temp * 10;
    mlogPrintf("%s,set_temp_control_target_temp:%d\n", get_current_time_format(), state_handle->PID_Type.Target_value);
    if (temp_control_target_temp_cb != NULL)
        temp_control_target_temp_cb(temp, input_dir);
}

void set_temp_control_p(float kp)
{
    PID_Para.Kp = kp;
    mlogPrintf("%s,set_temp_control_p:%f\n", get_current_time_format(), PID_Para.Kp);
    if (temp_control_p_cb != NULL)
        temp_control_p_cb(kp);
}

void set_temp_control_i(float ki)
{
    PID_Para.Ki = ki;
    mlogPrintf("%s,set_temp_control_i:%f\n", get_current_time_format(), PID_Para.Ki);
    if (temp_control_i_cb != NULL)
        temp_control_i_cb(ki);
}

void set_temp_control_d(float kd)
{
    PID_Para.Kd = kd;
    mlogPrintf("%s,set_temp_control_d:%f\n", get_current_time_format(), PID_Para.Kd);
    if (temp_control_d_cb != NULL)
        temp_control_d_cb(kd);
}
