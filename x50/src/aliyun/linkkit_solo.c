/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include "infra_config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "infra_state.h"
#include "infra_types.h"
#include "infra_defs.h"
#include "infra_compat.h"
#ifdef INFRA_MEM_STATS
#include "infra_mem_stats.h"
#endif
#include "dev_model_api.h"
#include "wrappers.h"
#include "cJSON.h"

#ifdef ATM_ENABLED
#include "at_api.h"
#endif
// #include "wifi_provision_api.h"
#include "linkkit_solo.h"
#include "linkkit_func.h"

// char g_product_key[IOTX_PRODUCT_KEY_LEN + 1] = "a1YTZpQDGwn";
// char g_product_secret[IOTX_PRODUCT_SECRET_LEN + 1] = "oE99dmyBcH5RAWE3";
// char g_device_name[IOTX_DEVICE_NAME_LEN + 1] = "X50_test1";
// char g_device_secret[IOTX_DEVICE_SECRET_LEN + 1] = "5fe43d0b7a6b2928c4310cc0d5fcb4b6";
static iotx_linkkit_dev_meta_info_t master_meta_info = {0};

typedef struct
{
    int master_devid;
    int cloud_connected;
    int master_initialized;
    int linkkit_runing;
} user_example_ctx_t;

static user_example_ctx_t g_user_example_ctx;

int (*property_set_event_cb)(const int, const char *, const int);
void register_property_set_event_cb(int (*cb)(const int, const char *, const int))
{
    property_set_event_cb = cb;
}
int (*property_report_all_cb)();
void register_property_report_all_cb(int (*cb)())
{
    property_report_all_cb = cb;
}
int (*service_request_event_cb)(const int, const char *, const int, const char *, const int, char **, int *);
void register_service_request_event_cb(int (*cb)(const int, const char *, const int, const char *, const int, char **, int *))
{
    service_request_event_cb = cb;
}
void (*connected_cb)(int);
void register_connected_cb(void (*cb)(int))
{
    connected_cb = cb;
}
int (*unbind_cb)(void);
void register_unbind_cb(int (*cb)())
{
    unbind_cb = cb;
}
int (*dynreg_device_secret_cb)(const char *);
void register_dynreg_device_secret_cb(int (*cb)(const char *))
{
    dynreg_device_secret_cb = cb;
}

int get_linkkit_connected_state()
{
    return g_user_example_ctx.cloud_connected;
}
/** cloud connected event callback */
static int user_connected_event_handler(void)
{
    EXAMPLE_TRACE("Cloud Connected");
    if (unbind_cb != NULL)
        unbind_cb();
    g_user_example_ctx.cloud_connected = 1;
    if (connected_cb != NULL)
        connected_cb(1);
    if (property_report_all_cb != NULL)
        property_report_all_cb();

    return 0;
}

static int user_connect_fail_event_handler(void)
{
    EXAMPLE_TRACE("user_connect_fail_event_handler");
    g_user_example_ctx.cloud_connected = 0;
    if (connected_cb != NULL)
        connected_cb(0);
    return 0;
}

/** cloud disconnected event callback */
static int user_disconnected_event_handler(void)
{
    EXAMPLE_TRACE("Cloud Disconnected");
    g_user_example_ctx.cloud_connected = 0;
    if (connected_cb != NULL)
        connected_cb(0);
    return 0;
}

/* device initialized event callback */
static int user_initialized(const int devid)
{
    EXAMPLE_TRACE("Device Initialized:%d", devid);
    g_user_example_ctx.master_initialized = 1;

    return 0;
}

/** recv property post response message from cloud **/
static int user_report_reply_event_handler(const int devid, const int msgid, const int code, const char *reply,
                                           const int reply_len)
{
    EXAMPLE_TRACE("Message Post Reply Received, Message ID: %d, Code: %d, Reply: %.*s", msgid, code,
                  reply_len,
                  (reply == NULL) ? ("NULL") : (reply));
    return 0;
}

/** recv event post response message from cloud **/
static int user_trigger_event_reply_event_handler(const int devid, const int msgid, const int code, const char *eventid,
                                                  const int eventid_len, const char *message, const int message_len)
{
    EXAMPLE_TRACE("Trigger Event Reply Received, Message ID: %d, Code: %d, EventID: %.*s, Message: %.*s",
                  msgid, code,
                  eventid_len,
                  eventid, message_len, message);

    return 0;
}

/** recv property setting message from cloud **/
static int user_property_set_event_handler(const int devid, const char *request, const int request_len)
{
    EXAMPLE_TRACE("Property Set Received, %d,Request: %s", devid, request);

    // res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
    //                          (unsigned char *)request, request_len);
    // EXAMPLE_TRACE("Post Property Message ID: %d", res);
    if (property_set_event_cb != NULL)
        property_set_event_cb(devid, request, request_len);
    return 0;
}

static int user_service_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
                                              const char *request, const int request_len,
                                              char **response, int *response_len)
{
    EXAMPLE_TRACE("Service Request Received, Service ID: %.*s, Payload: %s", serviceid_len, serviceid, request);

    if (service_request_event_cb != NULL)
        service_request_event_cb(devid, serviceid, serviceid_len, request, request_len, response, response_len);
    return 0;
}

static int user_timestamp_reply_event_handler(const char *timestamp)
{
    EXAMPLE_TRACE("Current Timestamp: %s", timestamp);

    return 0;
}

/** fota event handler **/
static int user_fota_event_handler(int type, const char *version)
{
    // char buffer[1024] = {0};
    // int buffer_length = 1024;
    EXAMPLE_TRACE("user_fota_event_handler");
    /* 0 - new firmware exist, query the new firmware */
    if (type == 0)
    {
        EXAMPLE_TRACE("user_fota_event_handler New Firmware Version: %s,%d", version, strlen(version));
        // if (get_ota_state() == OTA_IDLE)
        // {
        fota_event_handler(version);
        // }
        // else
        // {
        //     fota_event_handler(version);
        //     download_fota_image();
        //     // IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
        // }
    }

    return 0;
}

static int user_fota_module_event_handler(int type, const char *version, const char *module)
{
    char buffer[1024] = {0};
    int buffer_length = 1024;
    EXAMPLE_TRACE("user_fota_module_event_handler");
    /* 0 - new firmware exist, query the new firmware */
    if (type == 0)
    {
        EXAMPLE_TRACE("user_fota_module_event_handler New Firmware Version: %s, module: %s", version, module);

        IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
    }

    return 0;
}

/* cota event handler */
static int user_cota_event_handler(int type, const char *config_id, int config_size, const char *get_type,
                                   const char *sign, const char *sign_method, const char *url)
{
    char buffer[128] = {0};
    int buffer_length = 128;

    /* type = 0, new config exist, query the new config */
    if (type == 0)
    {
        EXAMPLE_TRACE("New Config ID: %s", config_id);
        EXAMPLE_TRACE("New Config Size: %d", config_size);
        EXAMPLE_TRACE("New Config Type: %s", get_type);
        EXAMPLE_TRACE("New Config Sign: %s", sign);
        EXAMPLE_TRACE("New Config Sign Method: %s", sign_method);
        EXAMPLE_TRACE("New Config URL: %s", url);

        IOT_Linkkit_Query(EXAMPLE_MASTER_DEVID, ITM_MSG_QUERY_COTA_DATA, (unsigned char *)buffer, buffer_length);
    }

    return 0;
}

#ifdef DEV_BIND_ENABLED
static int user_dev_bind_handler(const char *detail)
{
    EXAMPLE_TRACE("get bind event:%s", detail);
    return 0;
}

static int user_state_dev_bind(int ev, const char *msg)
{
    switch (ev)
    {
    case STATE_BIND_ASSEMBLE_APP_TOKEN_FAILED:
        EXAMPLE_TRACE("state: -0x%04X(%s)", -ev, msg);
        EXAMPLE_TRACE("STATE_BIND_ASSEMBLE_APP_TOKEN_FAILED\n");
        break;
    case STATE_BIND_TOKEN_EXPIRED:
        EXAMPLE_TRACE("state: -0x%04X(%s)", -ev, msg);
        EXAMPLE_TRACE("STATE_BIND_TOKEN_EXPIRED\n");
        break;
    case STATE_BIND_REPORT_RESET_SUCCESS:
        EXAMPLE_TRACE("state: -0x%04X(%s)", -ev, msg);
        EXAMPLE_TRACE("STATE_BIND_REPORT_RESET_SUCCESS\n");
        break;
    case STATE_BIND_RECV_CLOUD_NOTIFY:
        EXAMPLE_TRACE("state: -0x%04X(%s)", -ev, msg);
        EXAMPLE_TRACE("STATE_BIND_RECV_CLOUD_NOTIFY\n");
        {
            cJSON *root = cJSON_Parse(msg);
            cJSON *value = cJSON_GetObjectItem(root, "value");
            if (value != NULL)
            {
                cJSON *Operation = cJSON_GetObjectItem(value, "Operation");
                if (Operation != NULL)
                {
                    // char bind_flag = 0;
                    if (strcmp("Bind", Operation->valuestring) == 0)
                    {
                        // bind_flag = 1;
                        EXAMPLE_TRACE("STATE_BIND Bind............\n");
                    }
                    else if (strcmp("Unbind", Operation->valuestring) == 0)
                    {
                        EXAMPLE_TRACE("STATE_BIND Unbind............\n");
                    }
                }
            }
            cJSON_Delete(root);
        }
        break;
    }
    return 0;
}
#endif
void linkkit_user_post_property(const char *property_payload)
{
    int res = 0;
    if (property_payload == NULL)
        return;
    EXAMPLE_TRACE("user_post_property:%s", property_payload);

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)property_payload, strlen(property_payload));

    EXAMPLE_TRACE("Post Property Message ID: %d", res);
}

void linkkit_user_post_event(char *event_id, char *event_payload)
{
    int res = 0;
    if (event_id == NULL)
        return;
    EXAMPLE_TRACE("Post Event %s,%s", event_id, event_payload);

    if (event_payload != NULL)
    {
        res = IOT_Linkkit_TriggerEvent(EXAMPLE_MASTER_DEVID, event_id, strlen(event_id),
                                       event_payload, strlen(event_payload));
    }
    else
    {
        res = IOT_Linkkit_TriggerEvent(EXAMPLE_MASTER_DEVID, event_id, strlen(event_id),
                                       "{}", strlen("{}"));
    }
    EXAMPLE_TRACE("Post Event Message ID: %d", res);
}

static int user_cloud_error_handler(const int code, const char *data, const char *detail)
{
    EXAMPLE_TRACE("code =%d ,data=%s, detail=%s", code, data, detail);
    if (-code == ERROR_TOPO_RELATION_NOT_EXIST)
    {
        cJSON *root = cJSON_Parse(data);
        cJSON *deviceName = cJSON_GetObjectItem(root, "deviceName");
        if (deviceName != NULL)
        {
        }
        cJSON_Delete(root);
    }
    return 0;
}

static int dynreg_device_secret(const char *device_secret)
{
    EXAMPLE_TRACE("device secret: %s", device_secret);
    if (dynreg_device_secret_cb != NULL)
        dynreg_device_secret_cb(device_secret);
    return 0;
}

static int user_sdk_state_dump(int ev, const char *msg)
{
    EXAMPLE_TRACE("received state: -0x%04X(%s)\n", -ev, msg);
    return 0;
}

void get_linkkit_dev_quad(char *product_key, char *product_secret, char *device_name, char *device_secret)
{
    if (product_key)
        strcpy(product_key, master_meta_info.product_key);
    if (product_secret)
        strcpy(product_secret, master_meta_info.product_secret);
    if (device_name)
        strcpy(device_name, master_meta_info.device_name);
    if (device_secret)
        strcpy(device_secret, master_meta_info.device_secret);
}
void linkkit_close(void)
{

    EXAMPLE_TRACE("linkkit_close..............................");
    g_user_example_ctx.linkkit_runing = 0;
}
int linkkit_main(const char *product_key, const char *product_secret, const char *device_name, const char *device_secret)
{
    int res = 0;

    int domain_type = 0, dynamic_register = 0, post_reply_need = 0, fota_timeout = 30;

#ifdef ATM_ENABLED
    if (IOT_ATM_Init() < 0)
    {
        EXAMPLE_TRACE("IOT ATM init failed!\n");
        return -1;
    }
#endif

    memset(&g_user_example_ctx, 0, sizeof(user_example_ctx_t));

    memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
    // memcpy(master_meta_info.product_key, g_product_key, strlen(g_product_key));
    // memcpy(master_meta_info.product_secret, g_product_secret, strlen(g_product_secret));
    // memcpy(master_meta_info.device_name, g_device_name, strlen(g_device_name));
    // memcpy(master_meta_info.device_secret, g_device_secret, strlen(g_device_secret));
    strcpy(master_meta_info.product_key, product_key);
    strcpy(master_meta_info.product_secret, product_secret);
    strcpy(master_meta_info.device_name, device_name);
    strcpy(master_meta_info.device_secret, device_secret);
    EXAMPLE_TRACE("product_key:%s", master_meta_info.product_key);
    EXAMPLE_TRACE("product_secret:%s", master_meta_info.product_secret);
    EXAMPLE_TRACE("device_name:%s", master_meta_info.device_name);
    EXAMPLE_TRACE("device_secret:%s\n", master_meta_info.device_secret);
    IOT_SetLogLevel(IOT_LOG_DEBUG);

    /* Register Callback */
    IOT_RegisterCallback(ITE_STATE_EVERYTHING, user_sdk_state_dump);
    IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
    IOT_RegisterCallback(ITE_CONNECT_FAIL, user_connect_fail_event_handler);
    IOT_RegisterCallback(ITE_DISCONNECTED, user_disconnected_event_handler);
    IOT_RegisterCallback(ITE_SERVICE_REQUEST, user_service_request_event_handler);
    IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_event_handler);
    IOT_RegisterCallback(ITE_REPORT_REPLY, user_report_reply_event_handler);
    IOT_RegisterCallback(ITE_TRIGGER_EVENT_REPLY, user_trigger_event_reply_event_handler);
    IOT_RegisterCallback(ITE_TIMESTAMP_REPLY, user_timestamp_reply_event_handler);
    IOT_RegisterCallback(ITE_INITIALIZE_COMPLETED, user_initialized);
    IOT_RegisterCallback(ITE_FOTA, user_fota_event_handler);
    IOT_RegisterCallback(ITE_FOTA_MODULE, user_fota_module_event_handler);
    IOT_RegisterCallback(ITE_COTA, user_cota_event_handler);
    IOT_RegisterCallback(ITE_CLOUD_ERROR, user_cloud_error_handler);
    IOT_RegisterCallback(ITE_DYNREG_DEVICE_SECRET, dynreg_device_secret);
#ifdef DEV_BIND_ENABLED
    IOT_RegisterCallback(ITE_BIND_EVENT, user_dev_bind_handler);
    IOT_RegisterCallback(ITE_STATE_DEV_BIND, user_state_dev_bind);
#endif
    domain_type = IOTX_CLOUD_REGION_SHANGHAI;
    IOT_Ioctl(IOTX_IOCTL_SET_DOMAIN, (void *)&domain_type);

    /* Choose Login Method */
    if (strlen(master_meta_info.device_secret) == 0)
    {
        dynamic_register = 1;
    }
    else
    {
        dynamic_register = 0;
    }

    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);

    /* post reply doesn't need */
    post_reply_need = 1;
    IOT_Ioctl(IOTX_IOCTL_RECV_EVENT_REPLY, (void *)&post_reply_need);

    IOT_Ioctl(IOTX_IOCTL_FOTA_TIMEOUT_MS, (void *)&fota_timeout);

    do
    {
        g_user_example_ctx.master_devid = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
        if (g_user_example_ctx.master_devid >= 0)
        {
            break;
        }
        EXAMPLE_TRACE("IOT_Linkkit_Open failed! retry after %d ms\n", 2000);
        HAL_SleepMs(2000);
    } while (1);
    EXAMPLE_TRACE("g_user_example_ctx.master_devid %d\n", g_user_example_ctx.master_devid);

    do
    {
        res = IOT_Linkkit_Connect(g_user_example_ctx.master_devid);
        if (res >= 0)
        {
            break;
        }
        EXAMPLE_TRACE("IOT_Linkkit_Connect failed! retry after %d ms\n", 5000);
        HAL_SleepMs(5000);
    } while (1);
    g_user_example_ctx.linkkit_runing = 1;
    while (g_user_example_ctx.linkkit_runing)
    {
        IOT_Linkkit_Yield(EXAMPLE_YIELD_TIMEOUT_MS);
    }

    IOT_Linkkit_Close(g_user_example_ctx.master_devid);

    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_SetLogLevel(IOT_LOG_NONE);

    return 0;
}
