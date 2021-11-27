#ifndef _CLOUD_PROCESS_H_
#define _CLOUD_PROCESS_H_

#define PROFILE_PATH "."
#define PROFILE_NAME "DevProfile"
#define QUAD_NAME "DevQuad"
#define ETH_NAME "wlan0"

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
    char device_type[16];
    int hardware_ver;
    int software_ver;
    char remind[3][48];
    cloud_attr_t *attr;
    int attr_len;
} cloud_dev_t;

int cloud_init(void);
void cloud_deinit(void);
void send_data_to_cloud(const unsigned char *value, const int value_len);
void register_send_data_to_ecb_cb(int (*cb)(unsigned char *, const int));

cloud_dev_t *get_cloud_dev(void);
void get_dev_version(char *hardware_ver, char *software_ver);
#endif