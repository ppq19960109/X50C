#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "ota_power_task.h"
#include "link_solo.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "uart_resend.h"
#include "link_fota_posix.h"
#include "link_fota_power_posix.h"

#define POWER_OTA_CONFIG_FILE "/tmp/power.json"
#define POWER_OTA_FILE "/tmp/power_upgrade.bin"
static unsigned short ench_package_len = 256;

static set_attr_t g_ota_set_attr[];
static void *OTAState_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateNumber(get_ota_power_state());
    return item;
}

static void *OTARquest_cb(void *ptr, void *arg)
{
    if (NULL == arg)
        return NULL;
    cJSON *item = (cJSON *)arg;
    if (item->valueint == 0)
    {
        if (get_ota_state() == OTA_DOWNLOAD_START)
        {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, g_ota_set_attr[3].cloud_key, g_ota_set_attr[3].value.p);
            send_event_uds(root, NULL);
        }
        else
            link_fota_power_query_firmware();
    }
    else
    {
        set_OtaCmdPushType(1);
        link_fota_power_download_firmware();
    }
    return NULL;
}

static void *OTAProgress_cb(void *ptr, void *arg)
{
    return NULL;
}
static char NewVersion[64] = {0};
static void *OTANewVersion_cb(void *ptr, void *arg)
{
    set_attr_t *attr = (set_attr_t *)ptr;
    cJSON *item = cJSON_CreateString(attr->value.p);
    return item;
}

static set_attr_t g_ota_set_attr[] = {
    {
        cloud_key : "OTAPowerState",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTAState_cb
    },
    {
        cloud_key : "OTAPowerRquest",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : OTARquest_cb

    },
    {
        cloud_key : "OTAPowerProgress",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTAProgress_cb
    },
    {
        cloud_key : "OTAPowerNewVersion",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : OTANewVersion_cb,
        value : {p : NewVersion}
    },
};
static const int attr_len = sizeof(g_ota_set_attr) / sizeof(g_ota_set_attr[0]);
static set_attr_t *attr = g_ota_set_attr;

int ota_power_resp_get(cJSON *root, cJSON *resp)
{

    for (int i = 0; i < attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            set_attr_report_uds(resp, &attr[i]);
        }
    }
    return 0;
}

int ota_power_resp_getall(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        set_attr_report_uds(resp, &attr[i]);
    }
    return 0;
}

int ota_power_resp_set(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            set_attr_ctrl_uds(resp, &attr[i], cJSON_GetObjectItem(root, attr[i].cloud_key));
        }
    }
    return 0;
}

static int ota_state_event(const int state, void *arg)
{
    dzlog_info("ota_state_event:%d", state);
    cJSON *root = cJSON_CreateObject();

    if (OTA_NO_FIRMWARE == state)
    {
        set_OtaCmdPushType(0);
    }
    else
    {
        if (get_OtaCmdPushType() == OTA_PUSH_TYPE_SILENT)
        {
            cJSON_Delete(root);
            return -1;
        }
        if (OTA_NEW_FIRMWARE == state)
        {
            strcpy(g_ota_set_attr[3].value.p, arg);
            cJSON_AddStringToObject(root, g_ota_set_attr[3].cloud_key, arg);
        }
        else if (OTA_DOWNLOAD_FAIL == state || OTA_INSTALL_FAIL == state || OTA_INSTALL_SUCCESS == state)
        {
            set_OtaCmdPushType(0);
        }
    }
    cJSON_AddNumberToObject(root, g_ota_set_attr[0].cloud_key, state);
    return send_event_uds(root, NULL);
}

static void ota_progress_cb(const int precent)
{
    dzlog_info("ota_progress_cb:%d", precent);
    if (get_OtaCmdPushType() == OTA_PUSH_TYPE_SILENT)
        return;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, g_ota_set_attr[2].cloud_key, precent);

    send_event_uds(root, NULL);
}
static timer_t g_ota_timer = NULL;

static int ota_query_timer_start_cb(void)
{
    POSIXTimerSet(g_ota_timer, 0, 12);
    return 0;
}
static int ota_query_timer_stop_cb(void)
{
    POSIXTimerSet(g_ota_timer, 0, 0);
    return 0;
}
//--------------------------------------------
enum ota_cmd_status_t
{
    OTA_CMD_START = 0,
    OTA_CMD_DATA,
    OTA_CMD_NULL,
    OTA_CMD_STOP,
    OTA_CMD_END,
};
static int ota_power_steps = 0;
static FILE *ota_fp = NULL;
static timer_t ota_power_timer;
static unsigned short ota_total_packages = 0;
static unsigned short ota_current_package = 0;
static void *cloud_parse_power_json(void *input, const char *str)
{
    cJSON *root = cJSON_Parse(str);
    if (root == NULL)
    {
        return NULL;
    }
    cJSON *FileSize = cJSON_GetObjectItem(root, "FileSize");
    if (FileSize == NULL)
    {
        dzlog_error("FileSize is NULL\n");
        goto fail;
    }
    cJSON *FileVersion = cJSON_GetObjectItem(root, "FileVersion");
    if (FileVersion == NULL)
    {
        dzlog_error("FileVersion is NULL\n");
        goto fail;
    }
    cJSON *FileCRC = cJSON_GetObjectItem(root, "FileCRC");
    if (FileCRC == NULL)
    {
        dzlog_error("FileCRC is NULL\n");
        goto fail;
    }
    long file_size = getFileSize(POWER_OTA_FILE);
    dzlog_warn("%s,POWER_OTA_FILE getFileSize size:%ld", __func__, file_size);
    if (file_size <= 0)
        goto fail;
    if (file_size != FileSize->valueint)
    {
        dzlog_error("FileSize is error:%ld %d\n", file_size, FileSize->valueint);
        // goto fail;
    }
    ota_total_packages = file_size / ench_package_len + (file_size % ench_package_len > 0 ? 1 : 0);
    ota_current_package = 0;
    char *data = (char *)input;
    data[0] = FileSize->valueint >> 24;
    data[1] = FileSize->valueint >> 16;
    data[2] = FileSize->valueint >> 8;
    data[3] = FileSize->valueint;

    data[4] = FileVersion->valueint;

    data[5] = FileCRC->valueint >> 8;
    data[6] = FileCRC->valueint;
    cJSON_Delete(root);
    return data;
fail:
    cJSON_Delete(root);
    return NULL;
}

static int ota_power_send_data(const char cmd)
{
    static unsigned char buf[256 + 16];
    int len = 0;
    buf[len++] = 0xf9;
    buf[len++] = 0x01;
    buf[len++] = cmd;
    switch (cmd)
    {
    case OTA_CMD_START:
        if (get_dev_profile("/tmp", &buf[len], "power.json", cloud_parse_power_json) == NULL)
        {
            ota_power_steps = -1;
            break;
        }
        ecb_resend_list_clear();
#ifdef OTA_RESEND
        set_resend_wait_tick(2000);
#endif
        len += 7;
        buf[len++] = ench_package_len >> 8;
        buf[len++] = ench_package_len;
        ota_power_steps = OTA_CMD_START + 1;
        break;
    case OTA_CMD_DATA:
    {
        buf[len++] = ota_total_packages >> 8;
        buf[len++] = ota_total_packages;
        buf[len++] = ota_current_package >> 8;
        buf[len++] = ota_current_package;
        ++ota_current_package;
        buf[len++] = 0;
        buf[len++] = 0;
        size_t read_len = fread(&buf[len], 1, ench_package_len, ota_fp);
        dzlog_warn("ota_power_send_data read data len:%ld,ota_total_packages:%d,ota_current_package:%d", read_len, ota_total_packages, ota_current_package);
        buf[len - 2] = read_len >> 8;
        buf[len - 1] = read_len;
        len += read_len;
        if (read_len == 0)
        {
            len = 2;
            buf[len++] = OTA_CMD_END;
            ota_power_steps = OTA_CMD_END + 2;
        }
        else if (read_len < ench_package_len)
            ota_power_steps = OTA_CMD_END + 1;
        else
            ota_power_steps = OTA_CMD_DATA + 1;
    }
    break;
    case OTA_CMD_STOP:
        ota_power_steps = OTA_CMD_STOP + 1;
        break;
    case OTA_CMD_END:
        ota_power_steps = OTA_CMD_END + 2;
        break;
    }

    if (OTA_CMD_START == cmd)
    {
        ecb_uart_send_ota_msg(buf, len, 0);
        POSIXTimerSet(ota_power_timer, 0, 10);
    }
    else if (OTA_CMD_END == cmd)
    {
        ecb_uart_send_ota_msg(buf, len, 0);
        POSIXTimerSet(ota_power_timer, 0, 25);
    }
    else
    {
#ifdef OTA_RESEND
        ecb_uart_send_ota_msg(buf, len, 1);
#else
        ecb_uart_send_ota_msg(buf, len, 0);
#endif
        if (OTA_CMD_STOP != cmd)
            POSIXTimerSet(ota_power_timer, 0, 5);
    }
    return 0;
}
void ota_power_ack(const unsigned char *data)
{
    if (data[0] == 0)
    {
        dzlog_warn("ota_power_ack :%d", ota_power_steps);
        if (ota_power_steps == OTA_CMD_START + 1)
        {
            if (ota_fp != NULL)
            {
                fclose(ota_fp);
                ota_fp = NULL;
            }
            ota_fp = fopen(POWER_OTA_FILE, "r");
            if (ota_fp == NULL)
            {
                dzlog_error("fopen error NULL");
                ota_power_steps = 0;
                return;
            }
            ota_power_send_data(OTA_CMD_DATA);
        }
        else if (ota_power_steps == OTA_CMD_DATA + 1)
        {
            if (ota_total_packages <= ota_current_package)
                ota_power_send_data(OTA_CMD_END);
            else
                ota_power_send_data(OTA_CMD_DATA);
        }
        else if (ota_power_steps == OTA_CMD_STOP + 1)
        {
            ota_power_steps = -1;
        }
        else if (ota_power_steps == OTA_CMD_END + 1)
        {
            if (ota_fp != NULL)
            {
                fclose(ota_fp);
                ota_fp = NULL;
            }
            ota_power_send_data(OTA_CMD_END);
        }
        else if (ota_power_steps == OTA_CMD_END + 2)
        {
            if (ota_fp != NULL)
            {
                fclose(ota_fp);
                ota_fp = NULL;
            }
            ota_power_steps = 0;
        }
        else
        {
        }
    }
    else
    {
        dzlog_error("ota_power_ack error:%d", data[1]);
        if (data[1] == 5 || data[1] == 7)
        {
            // if (data[1] == 5)
            // {
            //     if (ota_fp != NULL)
            //     {
            //         fclose(ota_fp);
            //         ota_fp = NULL;
            //     }
            //     ota_fp = fopen(POWER_OTA_FILE, "r");
            //     if (ota_fp == NULL)
            //     {
            //         dzlog_error("fopen error NULL");
            //         ota_power_steps = 0;
            //         return;
            //     }
            //     ota_power_send_data(OTA_CMD_DATA);
            // }
            // else
            ota_power_send_data(OTA_CMD_START);
        }
        else
        {
            ota_power_send_data(OTA_CMD_STOP);
            ota_power_steps = -1;
        }
    }
}
static int ota_install_cb(char *text)
{
    int ret = -1;
    long size = getFileSize(text);
    dzlog_warn("ota_power_install_cb size:%ld", size);
    if (size <= 0)
        goto fail;
    char cmd[48] = {0};
    sprintf(cmd, "sh %s", text);
    ret = system(cmd);
    if (ret != 0)
        goto fail;
    set_ecb_ota_power_state(1);
    ota_power_send_data(OTA_CMD_START);
    while (ota_power_steps > 0)
    {
    }
    dzlog_warn("ota_power_install_cb end...");
    POSIXTimerSet(ota_power_timer, 0, 0);
#ifdef OTA_RESEND
    set_resend_wait_tick(0);
#endif
    set_ecb_ota_power_state(0);
    ret = ota_power_steps;
fail:
    dzlog_warn("ota_power_install_cb ret:%d", ret);
    // sprintf(cmd, "rm -rf %s", text);
    // system(cmd);
    return ret;
}
void power_ota_install()
{
    ota_install_cb("/data/power_upgrade.bin");
}
static void POSIXTimer_cb(union sigval val)
{
    dzlog_warn("%s, sigval:%d", __func__, val.sival_int);
    if (val.sival_int == 0)
    {
        ota_power_query_timer_end();
    }
    else if (val.sival_int == 1)
    {
        ota_power_send_data(OTA_CMD_STOP);
        ota_power_steps = -1;
    }
}
int ota_power_task_init(void)
{
    register_ota_power_state_cb(ota_state_event);
    register_ota_power_progress_cb(ota_progress_cb);
    register_ota_power_install_cb(ota_install_cb);
    register_ota_power_query_timer_start_cb(ota_query_timer_start_cb);
    register_ota_power_query_timer_stop_cb(ota_query_timer_stop_cb);
    g_ota_timer = POSIXTimerCreate(0, POSIXTimer_cb);
    ota_power_timer = POSIXTimerCreate(1, POSIXTimer_cb);
    return 0;
}
void ota_power_task_deinit(void)
{
    if (ota_fp != NULL)
    {
        fclose(ota_fp);
        ota_fp = NULL;
    }
    if (ota_power_timer != NULL)
    {
        POSIXTimerDelete(ota_power_timer);
        ota_power_timer = NULL;
    }
    if (g_ota_timer != NULL)
    {
        POSIXTimerDelete(g_ota_timer);
        g_ota_timer = NULL;
    }
}