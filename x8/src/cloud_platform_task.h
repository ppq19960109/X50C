#ifndef _CLOUD_PLATFORM_TASK_H_
#define _CLOUD_PLATFORM_TASK_H_

#include "cJSON.h"

#define DEFAULT_PRODUCT_KEY "a1h3msfxU9d"
#define DEFAULT_PRODUCT_SECRET "4Q27bIDkFlqDqIPm"

#define PROFILE_NAME "DevProfile.json"
#define QUAD_NAME "DevQuad.json"
#define ETH_NAME "wlan0"
#define SOFTER_VER "0.2.0"

enum OTA_PUSH_TYPE
{
    OTA_PUSH_TYPE_SILENT = 0x00,
    OTA_PUSH_TYPE_CONFIRM,
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
    REPORT_WORK_STATE_PREHEAT_PAUSE,
};

enum LINK_VALUE_TYPE
{
    LINK_VALUE_TYPE_NUM = 0x00,
    LINK_VALUE_TYPE_STRING_NUM,
    LINK_VALUE_TYPE_STRING,
    LINK_VALUE_TYPE_STRUCT,
    LINK_VALUE_TYPE_ARRAY,
};
enum LINK_FUN_TYPE
{
    LINK_FUN_TYPE_ATTR_REPORT_CTRL = 0x00,
    LINK_FUN_TYPE_ATTR_REPORT,
    LINK_FUN_TYPE_ATTR_CTRL,
    LINK_FUN_TYPE_EVENT,
    LINK_FUN_TYPE_SERVICE,
};

typedef struct
{
    char cloud_key[28];
    enum LINK_VALUE_TYPE cloud_value_type;
    enum LINK_FUN_TYPE cloud_fun_type;
    signed short uart_cmd;
    unsigned char uart_byte_len;
    char *value;
} cloud_attr_t;

typedef struct
{
    char product_key[33];
    char product_secret[33];
    char device_name[33];
    char device_secret[33];
    char device_category[33];
    char device_model[16];
    char token[33];
    char mac[16];
    char hardware_ver[6];
    char software_ver[6];
    char mcook_url[48];
    char lRemindText[3][48];
    char rRemindText[3][48];
    char update_log[640];
    cloud_attr_t *attr;
    int attr_len;
} cloud_dev_t;

typedef struct
{
    char cloud_key[28];
    enum LINK_FUN_TYPE fun_type;
    void *(*cb)(void *, void *);

    union
    {
        char c;
        int n;
        double f;
        char *p;
    } value;
} set_attr_t;

int cJSON_Object_isNull(cJSON *object);
int report_msg_all_platform(cJSON *root);

int cloud_init(void);
void cloud_deinit(void);
void *cloud_task(void *arg);

void send_data_to_cloud(const unsigned char *value, const int value_len, const unsigned char command);
void register_send_data_ecb_cb(int (*cb)(unsigned char *, const int));

int set_attr_report_uds(cJSON *root, set_attr_t *attr);
int set_attr_ctrl_uds(cJSON *root, set_attr_t *attr, cJSON *item);
int cloud_resp_get(cJSON *root, cJSON *resp);
int cloud_resp_getall(cJSON *root, cJSON *resp);
int cloud_resp_set(cJSON *root, cJSON *resp);
int send_all_to_cloud(void);

cloud_dev_t *get_cloud_dev(void);

cloud_attr_t *get_attr_ptr(const char *name);
unsigned int get_ErrorCode(void);
unsigned char get_ErrorCodeShow(void);
signed char get_HoodSpeed(void);
signed char get_StoveStatus(void);
signed char get_OtaCmdPushType(void);
signed char set_OtaCmdPushType(char type);
void get_quad(void);
void uds_report_reset(void);
#endif