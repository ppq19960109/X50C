#ifndef _ECB_PARSE_H_
#define _ECB_PARSE_H_

typedef struct
{
    signed short uart_cmd;
    unsigned char uart_byte_len;
    unsigned char change;
    char *value;
} ecb_attr_t;

typedef struct
{
    unsigned char mode;
    unsigned short temp;
    unsigned short time;
    unsigned char custom;
} multistage_step_t;
typedef struct
{
    unsigned char valid;
    unsigned char total_step;
    unsigned char current_step;
    multistage_step_t step[3];
} multistage_state_t;

enum work_dir
{
    WORK_DIR_LEFT,
    WORK_DIR_RIGHT,
};
enum work_state
{
    WORK_STATE_NOWORK,
    WORK_STATE_PREHEAT,
    WORK_STATE_PREHEAT_PAUSE,
    WORK_STATE_RUN,
    WORK_STATE_PAUSE,
    WORK_STATE_FINISH,
    WORK_STATE_RESERVE,
    WORK_STATE_ERROR,
};
enum work_operation
{
    WORK_OPERATION_RUN,
    WORK_OPERATION_PAUSE,
    WORK_OPERATION_STOP,
    WORK_OPERATION_FINISH,
    WORK_OPERATION_RUN_NOW,
};
enum report_work_state
{
    REPORT_WORK_STATE_STOP,
    REPORT_WORK_STATE_RESERVE,
    REPORT_WORK_STATE_PREHEAT,
    REPORT_WORK_STATE_RUN,
    REPORT_WORK_STATE_FINISH,
    REPORT_WORK_STATE_PAUSE,
    REPORT_WORK_STATE_RESERVE_PAUSE,
};

enum event_set_state_enum
{
    EVENT_SET_SysPower = 0X05,
    EVENT_ErrorCode = 0X0A,
    EVENT_ErrorCodeShow = 0X0B,
    EVENT_LStove = 0X11,
    EVENT_RStove = 0X12,
    EVENT_HoodStoveLink = 0X13,
    EVENT_RHoodLightLink = 0X14,
    EVENT_SET_HoodSpeed = 0X31,
    EVENT_SET_HoodLight = 0X32,
    EVENT_HoodOffLeftTime = 0X34,
    EVENT_SET_HoodOffTimer = 0X38,
    SET_LStOvOperation = 0X41,
    EVENT_LStOvState = 0X42,
    EVENT_SET_LStOvMode = 0X43,
    EVENT_SET_LStOvSetTemp = 0X44,
    EVENT_LStOvRealTemp = 0X45,
    EVENT_SET_LStOvSetTimer = 0X46,
    EVENT_LStOvSetTimerLeft = 0X47,
    EVENT_SET_LStOvOrderTimer = 0X48,
    EVENT_LStOvOrderTimerLeft = 0X49,
    EVENT_LStOvDoorState = 0X4A,
    EVENT_LStOvLightState = 0X4B,
    SET_LMultiStageContent = 0X4C,
    EVENT_LMultiStageState = 0X4D,
    EVENT_SET_LMultiMode = 0X4F,
    EVENT_SET_LCookbookID = 0X50,
    SET_RStOvOperation = 0X51,
    EVENT_RStOvState = 0X52,
    EVENT_SET_RStOvSetTemp = 0X53,
    EVENT_RStOvRealTemp = 0X54,
    EVENT_SET_RStOvSetTimer = 0X55,
    EVENT_RStOvSetTimerLeft = 0X56,
    EVENT_SET_RStOvOrderTimer = 0X57,
    EVENT_RStOvOrderTimerLeft = 0X58,
    EVENT_RStOvDoorState = 0X59,
    EVENT_RStOvLightState = 0X5A,
    EVENT_SET_RStOvMode = 0X5B,
    SET_RMultiStageContent = 0X5C,
    EVENT_RMultiStageState = 0X5D,
    EVEN_SET_RMultiMode = 0X5f,
    EVENT_SET_RCookbookID = 0X60,
    EVENT_SET_LSteamGear = 0X80,
    EVENT_SET_RIceSteamID = 0X81,
    EVENT_SET_DataReportReason = 0Xf6,
    SET_BuzControl = 0Xf7,
};
void ecb_parse_init();
void ecb_parse_deinit();
int ecb_parse_set_heart();
int ecb_parse_event_uds(unsigned char cmd);
int ecb_parse_event_msg(unsigned char *data, unsigned int len);
int ecb_parse_set_msg(const unsigned char *data, unsigned int len);
int right_ice_time_left(unsigned short time);
int right_ice_state_finish();
int right_ice_hood_speed(unsigned char speed);
int right_ice_hood_light(unsigned char light);
#endif