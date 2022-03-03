#ifndef _CLOUD_PLATFORM_TASK_H_
#define _CLOUD_PLATFORM_TASK_H_

#include "cJSON.h"

#define DEFAULT_PRODUCT_KEY "a1YTZpQDGwn"
#define DEFAULT_PRODUCT_SECRET "oE99dmyBcH5RAWE3"

#define PROFILE_NAME "DevProfile"
#define QUAD_NAME "DevQuad"
#define ETH_NAME "wlan0"
#define SOFTER_VER "1.0.2"

#define UART_CMD_MULTISTAGE_SET (77)
#define UART_CMD_MULTISTAGE_STATE (78)

enum LINK_VALUE_TYPE
{
    LINK_VALUE_TYPE_NUM = 0x00,
    LINK_VALUE_TYPE_STRING_NUM,
    LINK_VALUE_TYPE_STRING,
    LINK_VALUE_TYPE_STRUCT,
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
    char cloud_key[33];
    enum LINK_VALUE_TYPE cloud_value_type;
    enum LINK_FUN_TYPE cloud_fun_type;
    unsigned char uart_cmd;
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
    char after_sales_phone[16];
    char hardware_ver[6];
    char software_ver[6];
    char remind[3][48];
    char update_log[1024];
    cloud_attr_t *attr;
    int attr_len;
} cloud_dev_t;

typedef struct
{
    char cloud_key[33];
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

void send_data_to_cloud(const unsigned char *value, const int value_len);
void register_send_data_ecb_cb(int (*cb)(unsigned char *, const int));

int set_attr_report_uds(cJSON *root, set_attr_t *attr);
int set_attr_ctrl_uds(cJSON *root, set_attr_t *attr, cJSON *item);
int cloud_resp_get(cJSON *root, cJSON *resp);
int cloud_resp_getall(cJSON *root, cJSON *resp);
int cloud_resp_set(cJSON *root, cJSON *resp);

cloud_dev_t *get_cloud_dev(void);
// unsigned char get_BuzControl(void);
unsigned int get_ErrorCode(void);
unsigned char get_ErrorCodeShow(void);
void get_dev_version(char *hardware_ver, char *software_ver);
#endif