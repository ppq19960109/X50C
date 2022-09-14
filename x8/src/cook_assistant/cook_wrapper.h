#ifndef _COOK_WRAPPER_H_
#define _COOK_WRAPPER_H_
#ifdef __cplusplus
extern "C"
{
#endif

    enum INPUT_DIR
    {
        INPUT_LEFT = 0, //左灶
        INPUT_RIGHT,    //右灶
        INPUT_MAX
    };
// #define SIMULATION
#ifdef SIMULATION
    int get_cur_state(char *msg, enum INPUT_DIR input_dir);
    int get_hood_gear(char *msg);
    int get_fire_gear(char *msg, enum INPUT_DIR input_dir);
#endif
    //本地对外函数
    void prepare_gear_change_task();
    void cook_assistant_init(enum INPUT_DIR input_dir);                                                        //烹饪助手初始化
    void cook_assistant_input(enum INPUT_DIR input_dir, unsigned short temp, unsigned short environment_temp); //温度输入

    void set_ignition_switch(unsigned char ignition_switch, enum INPUT_DIR input_dir); //点火开关控制

    void recv_ecb_gear(unsigned char gear, unsigned char gear_change_state); //风机状态
    void recv_ecb_fire(unsigned char fire, enum INPUT_DIR input_dir);        //大小火状态

    void register_close_fire_cb(int (*cb)(enum INPUT_DIR input_dir));                //关火回调
    void register_hood_gear_cb(int (*cb)(const int gear));                           //风机回调
    void register_fire_gear_cb(int (*cb)(const int gear, enum INPUT_DIR input_dir)); //大小火回调

    void register_thread_lock_cb(int (*cb)());
    void register_thread_unlock_cb(int (*cb)());
    void register_cook_assist_remind_cb(int (*cb)());
    //云端对外函数
    void set_work_mode(unsigned char mode);
    void set_smart_smoke_switch(unsigned char smart_smoke_switch);
    void set_pan_fire_switch(unsigned char pan_fire_switch, enum INPUT_DIR input_dir);
    void set_dry_switch(unsigned char dry_switch, enum INPUT_DIR input_dir);
    void set_temp_control_switch(unsigned char temp_control_switch, enum INPUT_DIR input_dir);
    void set_temp_control_target_temp(unsigned short temp, enum INPUT_DIR input_dir);
    void set_temp_control_p(float kp);
    void set_temp_control_i(float ki);
    void set_temp_control_d(float kd);

    void register_work_mode_cb(int (*cb)(const unsigned char));
    void register_smart_smoke_switch_cb(int (*cb)(const unsigned char));
    void register_pan_fire_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR));
    void register_dry_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR));
    void register_temp_control_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR));
    void register_temp_control_target_temp_cb(int (*cb)(const unsigned short, enum INPUT_DIR));
    void register_oil_temp_cb(void (*cb)(const unsigned short, enum INPUT_DIR));
#ifdef __cplusplus
}
#endif
#endif
