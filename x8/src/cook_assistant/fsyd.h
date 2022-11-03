#ifndef _FSYD_H_
#define _FSYD_H_
#ifdef __cplusplus
extern "C"
{
#endif
#include "ring_buffer.h"
#include "cook_wrapper.h"

// #define BOIL_ENABLE

#define STATE_JUDGE_DATA_SIZE (10)
#define INPUT_DATA_HZ (4)

    //大小火
    enum FIRE
    {
        FIRE_SMALL = 0,
        FIRE_BIG
    };
    //烟机档位
    enum GEAR
    {
        GEAR_CLOSE = 0,
        GEAR_ONE,
        GEAR_TWO,
        GEAR_THREE,
        GEAR_INVALID = 0xff
    };

    //移锅小火状态
    enum PAN_FIRE
    {
        PAN_FIRE_CLOSE = 0,
        PAN_FIRE_ERROR_CLOSE,
        PAN_FIRE_START,
        PAN_FIRE_ENTER,
    };
    //风随烟动状态
    enum STATE_FSYD
    {
        STATE_SHAKE = 0,
        STATE_DOWN_JUMP,
        STATE_RISE_JUMP,
        STATE_RISE_SLOW,
        STATE_DOWN_SLOW,
        STATE_GENTLE,
        STATE_IDLE,
        STATE_PAN_FIRE,
#ifdef BOIL_ENABLE
        STATE_BOIL,
#endif
    };
    enum TEMP_VALUE
    {
        PAN_FIRE_LOW_TEMP = 2000,
        PAN_FIRE_HIGH_TEMP = 2300,
        SHAKE_PERMIT_TEMP = 1100,
#ifdef BOIL_ENABLE
        BOIL_LOW_TEMP = 830,
        BOIL_HIGH_TEMP = 1050,
#endif
    };

    enum TICK_VALUE
    {
        START_FIRST_MINUTE_TICK = INPUT_DATA_HZ * 40,

        PAN_FIRE_ENTER_TICK = INPUT_DATA_HZ * 5,
        PAN_FIRE_DOWN_JUMP_EXIT_TICK = INPUT_DATA_HZ * 5,
        PAN_FIRE_RISE_JUMP_EXIT_LOCK_TICK = INPUT_DATA_HZ * 3,
        PAN_FIRE_HIGH_TEMP_EXIT_LOCK_TICK = INPUT_DATA_HZ * 5,
        PAN_FIRE_ERROR_LOCK_TICK = INPUT_DATA_HZ * 40,
        SHAKE_EXIT_TICK = INPUT_DATA_HZ * 20,
        TEMP_CONTROL_LOCK_TICK = INPUT_DATA_HZ * 5,
#ifdef BOIL_ENABLE
        BOIL_START_TICK = INPUT_DATA_HZ * 30,
#endif
    };

    typedef struct
    {
        enum INPUT_DIR input_dir;

        unsigned char pan_fire_switch;
        unsigned char temp_control_switch;
        //---------------------
        unsigned char last_prepare_state;
        unsigned int last_prepare_state_tick;

        unsigned char state; //总状态

        unsigned char pan_fire_state;
        unsigned char pan_fire_high_temp_exit_lock_tick; //高温时，移锅小火退出后，锁定时间
        unsigned char pan_fire_rise_jump_exit_lock_tick; //跳升退出移锅小火后，锁定时间
        unsigned char pan_fire_first_error;              //开始一分钟内,移锅小火第一次判断错误
        unsigned int pan_fire_error_lock_tick;           //移锅小火判断错误，锁定时间
        unsigned int pan_fire_tick;                      //移锅小火进入判断时间
        unsigned char pan_fire_enter_type;               //移锅小火进入类型 1:翻炒进入 0:其他进入 2:防干烧进入
        unsigned short pan_fire_enter_start_tick;

        unsigned short temp_control_first;
        unsigned char temp_control_lock_countdown; //控温，锁定时间
        unsigned short temp_control_enter_start_tick;
        unsigned short temp_control_target_value;

        unsigned int shake_permit_start_tick; //允许翻炒开始的时间
        unsigned int shake_exit_tick;
        unsigned char shake_long;

        unsigned char ignition_switch;
        unsigned short ignition_switch_close_temp;

        unsigned char fire_gear;
        unsigned char hood_gear;
        unsigned int current_tick;
        unsigned int total_tick;

        unsigned char temp_data_size;
        unsigned short last_temp;                             //最新温度
        unsigned short last_temp_data[STATE_JUDGE_DATA_SIZE]; //最新温度数组

        unsigned short state_jump_temp;
        unsigned short state_jump_diff;

#ifdef BOIL_ENABLE
        unsigned char boil_gengle_state;
        unsigned short boil_start_tick;
#endif
        ring_buffer_t ring_buffer;
    } state_handle_t;

    typedef int (*state_func_def)(unsigned char prepare_state, state_handle_t *state_handle);

    typedef struct
    {
        unsigned char work_mode;
        unsigned char smart_smoke_switch;
        unsigned char lock;
        unsigned char gear;
        unsigned char prepare_gear;
        unsigned int gear_tick;
        unsigned int close_delay_tick;
    } state_hood_t;

    void cook_assistant_reinit(state_handle_t *state_handle);
    state_handle_t *get_input_handle(enum INPUT_DIR input_dir);
    state_hood_t *get_hood_handle();
    char *get_current_time_format(void);
    void set_fire_gear(unsigned char fire_gear, state_handle_t *state_handle, const unsigned char type);

#ifdef __cplusplus
}
#endif
#endif
