
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_ota_api.h"
#include "link_solo.h"
#include "link_fota_power_posix.h"

#define OTA_FILE "/userdata/power_upgrade.bin"
static char version[64] = {0};
static FILE *ota_fp = NULL;
static void *ota_handle = NULL;
static pthread_t g_download_thread; /* 用于HTTP的固件下载线程 */
static enum OTA_TYPE g_ota_state = OTA_IDLE;
static int last_percent = 0;
static void *g_dl_handle = NULL;
static int query_firmware_flag = 0;
static int download_fail_count = 0;

static int (*ota_state_cb)(const int, void *);
void register_ota_power_state_cb(int (*cb)(const int, void *))
{
    ota_state_cb = cb;
}
static void (*ota_progress_cb)(const int);
void register_ota_power_progress_cb(void (*cb)(const int))
{
    ota_progress_cb = cb;
}
static int (*ota_install_cb)(char *);
void register_ota_power_install_cb(int (*cb)(char *))
{
    ota_install_cb = cb;
}
static void (*ota_complete_cb)(void);
void register_ota_power_complete_cb(void (*cb)(void))
{
    ota_complete_cb = cb;
}
static int (*ota_timer_start_cb)(void);
void register_ota_power_query_timer_start_cb(int (*cb)(void))
{
    ota_timer_start_cb = cb;
}
static int (*ota_timer_stop_cb)(void);
void register_ota_power_query_timer_stop_cb(int (*cb)(void))
{
    ota_timer_stop_cb = cb;
}

int get_ota_power_state(void)
{
    return g_ota_state;
}

void set_ota_power_state(enum OTA_TYPE ota_state, void *arg)
{
    g_ota_state = ota_state;
    if (ota_state_cb != NULL)
        ota_state_cb(ota_state, arg);
}

void ota_power_query_timer_cb(void)
{
    set_ota_power_state(OTA_NO_FIRMWARE, NULL);
    query_firmware_flag = 0;
}

/* 下载收包回调, 用户调用 aiot_download_recv() 后, SDK收到数据会进入这个函数, 把下载到的数据交给用户 */
/* TODO: 一般来说, 设备升级时, 会在这个回调中, 把下载到的数据写到Flash上 */
static void demo_download_recv_handler(void *handle, const aiot_download_recv_t *packet, void *userdata)
{
    uint32_t data_buffer_len = 0;
    int32_t percent = 0;

    /* 目前只支持 packet->type 为 AIOT_DLRECV_HTTPBODY 的情况 */
    if (!packet || AIOT_DLRECV_HTTPBODY != packet->type)
    {
        return;
    }
    percent = packet->data.percent;

    data_buffer_len = packet->data.len;

    /* 如果 percent 为负数, 说明发生了收包异常或者digest校验错误 */
    if (percent < 0)
    {
        printf("power exception: percent = %d\r\n", percent);
        return;
    }
    printf("power %s,percent = %d\r\n", __func__, percent);
    /*
     * TODO: 下载一段固件成功, 这个时候, 用户应该将
     *       起始地址为 packet->data.buffer, 长度为 packet->data.len 的内存, 保存到flash上
     *
     *       如果烧写flash失败, 还应该调用 aiot_download_report_progress(handle, -4) 将失败上报给云平台
     *       备注:协议中, 与云平台商定的错误码在 aiot_ota_protocol_errcode_t 类型中, 例如
     *           -1: 表示升级失败
     *           -2: 表示下载失败
     *           -3: 表示校验失败
     *           -4: 表示烧写失败
     *
     *       详情可见 https://help.aliyun.com/document_detail/85700.html
     */
    fwrite(packet->data.buffer, packet->data.len, 1, ota_fp);

    /* percent 入参的值为 100 时, 说明SDK已经下载固件内容全部完成 */
    if (percent == 100)
    {
        /*
         * TODO: 这个时候, 一般用户就应该完成所有的固件烧录, 保存当前工作, 重启设备, 切换到新的固件上启动了
                 并且, 新的固件必须要以

                 aiot_ota_report_version(ota_handle, new_version);

                 这样的操作, 将升级后的新版本号(比如1.0.0升到1.1.0, 则new_version的值是"1.1.0")上报给云平台
                 云平台收到了新的版本号上报后, 才会判定升级成功, 否则会认为本次升级失败了
                 如果下载成功后升级失败, 还应该调用 aiot_download_report_progress(handle, -1) 将失败上报给云平台
         */
        printf("power ota end,start install...\n");
        if (ota_fp != NULL)
        {
            fclose(ota_fp);
            ota_fp = NULL;
        }
        sync();

        set_ota_power_state(OTA_INSTALL_START, NULL);
        // aiot_download_report_progress(handle, AIOT_OTAERR_FETCH_FAILED);
        // link_fota_power_report_version("1.5");
        if (ota_install_cb == NULL)
        {
            aiot_download_report_progress(handle, AIOT_OTAERR_UPGRADE_FAILED);
            return;
        }
        else
        {
            if (ota_install_cb(OTA_FILE) != 0)
            {
                aiot_download_report_progress(handle, AIOT_OTAERR_UPGRADE_FAILED);
                return;
            }
        }
        set_ota_power_state(OTA_INSTALL_SUCCESS, NULL);
        sync();
        if (ota_progress_cb)
            ota_progress_cb(percent);
        sleep(1);
        if (ota_complete_cb)
            ota_complete_cb();
    }

    /* 简化输出, 只有距离上次的下载进度增加5%以上时, 才会打印进度, 并向服务器上报进度 */
    if (percent - last_percent >= 5 || percent == 100)
    {
        printf("power download %03d%% done, +%d bytes\r\n", percent, data_buffer_len);
        if (percent != 100)
            aiot_download_report_progress(handle, percent);

        last_percent = percent;
        if (ota_progress_cb)
            ota_progress_cb(percent);
    }
}
/* 执行aiot_download_recv的线程, 实现固件内容的请求和接收 */
static void *demo_ota_download_thread(void *dl_handle)
{
    int32_t ret = 0;

    printf("power starting download thread in 2 seconds ......\r\n");
    sleep(2);

    /* 向固件服务器请求下载 */
    /*
     * TODO: 下面这样的写法, 就是以1个请求, 获取全部的固件内容
     *       设备资源比较少, 或者网络较差时, 也可以分段下载, 需要组合
     *
     *       aiot_download_setopt(dl_handle, AIOT_DLOPT_RANGE_START, ...);
     *       aiot_download_setopt(dl_handle, AIOT_DLOPT_RANGE_END, ...);
     *       aiot_download_send_request(dl_handle);
     *
     *       实现, 这种情况下, 需要把以上组合语句放置到循环中, 多次 send_request 和 recv
     *
     */

    set_ota_power_state(OTA_DOWNLOAD_START, NULL);
    download_fail_count = 0;
    ota_fp = fopen(OTA_FILE, "w+");

    aiot_download_send_request(dl_handle);

    last_percent = 0;
    while (1)
    {
        /* 从网络收取服务器回应的固件内容 */
        ret = aiot_download_recv(dl_handle);

        /* 固件全部下载完时, aiot_download_recv() 的返回值会等于 STATE_DOWNLOAD_FINISHED, 否则是当次获取的字节数 */
        if (STATE_DOWNLOAD_FINISHED == ret)
        {
            printf("power download completed\r\n");
            break;
        }
        if (STATE_DOWNLOAD_RENEWAL_REQUEST_SENT == ret)
        {
            printf("power download renewal request has been sent successfully\r\n");
            continue;
        }
        if (ret <= STATE_SUCCESS)
        {
            printf("power download failed, error code is %d, try to send renewal request :%d\r\n", ret, download_fail_count);
            if (++download_fail_count > 5)
            {
                aiot_download_report_progress(dl_handle, AIOT_OTAERR_FETCH_FAILED);
                break;
            }
            continue;
        }
        else
        {
            download_fail_count = 0;
        }
    }
    /* 下载所有固件内容完成, 销毁下载会话, 线程自行退出 */
    printf("power download thread exit:%d\r\n", ret);
    aiot_download_deinit(&dl_handle);

    if (ota_fp != NULL)
    {
        fclose(ota_fp);
        ota_fp = NULL;
    }
    if (STATE_DOWNLOAD_FINISHED != ret || OTA_INSTALL_SUCCESS != g_ota_state)
    {
        last_percent = -1;
        set_ota_power_state(OTA_DOWNLOAD_FAIL, NULL);
    }
    else
    {
        last_percent = 0;
        set_ota_power_state(OTA_IDLE, NULL);
    }
    return NULL;
}

int link_fota_power_download_firmware(void)
{
    int res = 0;
    /* 启动专用的下载线程, 去完成固件内容的下载 */
    res = pthread_create(&g_download_thread, NULL, demo_ota_download_thread, g_dl_handle);
    if (res != 0)
    {
        printf("power pthread_create demo_ota_download_thread failed: %d\r\n", res);
        aiot_download_deinit(&g_dl_handle);
    }
    else
    {
        /* 下载线程被设置为 detach 类型, 固件内容获取完毕后可自主退出 */
        pthread_detach(g_download_thread);
    }
    g_dl_handle = NULL;
    return 0;
}

/* 用户通过 aiot_ota_setopt() 注册的OTA消息处理回调, 如果SDK收到了OTA相关的MQTT消息, 会自动识别, 调用这个回调函数 */
static void demo_ota_recv_handler(void *ota_handle, aiot_ota_recv_t *ota_msg, void *userdata)
{
    switch (ota_msg->type)
    {
    case AIOT_OTARECV_FOTA:
    {
        if (NULL == ota_msg->task_desc || ota_msg->task_desc->protocol_type != AIOT_OTA_PROTOCOL_HTTPS)
        {
            break;
        }
        if (ota_timer_stop_cb)
            ota_timer_stop_cb();
        printf("power OTA target firmware version: %s, size: %u Bytes \r\n", ota_msg->task_desc->version,
               ota_msg->task_desc->size_total);
        if (NULL != ota_msg->task_desc->extra_data)
        {
            printf("power extra data: %s\r\n", ota_msg->task_desc->extra_data);
        }
        if (NULL != ota_msg->task_desc->module)
        {
            printf("power module: %s\r\n", ota_msg->task_desc->module);
        }
        set_ota_power_state(OTA_NEW_FIRMWARE, ota_msg->task_desc->version);

        uint16_t port = 443;
        uint32_t max_buffer_len = (8 * 1024);
        aiot_sysdep_network_cred_t cred;
        void *dl_handle = NULL;

        dl_handle = aiot_download_init();
        if (NULL == dl_handle)
        {
            break;
        }
        g_dl_handle = dl_handle;
        memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
        cred.option = AIOT_SYSDEP_NETWORK_CRED_SVRCERT_CA;
        cred.max_tls_fragment = 16384;
        cred.x509_server_cert = ali_ca_cert;
        cred.x509_server_cert_len = strlen(ali_ca_cert);

        /* 设置下载时为TLS下载 */
        aiot_download_setopt(dl_handle, AIOT_DLOPT_NETWORK_CRED, (void *)(&cred));
        /* 设置下载时访问的服务器端口号 */
        aiot_download_setopt(dl_handle, AIOT_DLOPT_NETWORK_PORT, (void *)(&port));
        /* 设置下载的任务信息, 通过输入参数 ota_msg 中的 task_desc 成员得到, 内含下载地址, 固件大小, 固件签名等 */
        aiot_download_setopt(dl_handle, AIOT_DLOPT_TASK_DESC, (void *)(ota_msg->task_desc));
        /* 设置下载内容到达时, SDK将调用的回调函数 */
        aiot_download_setopt(dl_handle, AIOT_DLOPT_RECV_HANDLER, (void *)(demo_download_recv_handler));
        /* 设置单次下载最大的buffer长度, 每当这个长度的内存读满了后会通知用户 */
        aiot_download_setopt(dl_handle, AIOT_DLOPT_BODY_BUFFER_MAX_LEN, (void *)(&max_buffer_len));
        /* 设置 AIOT_DLOPT_RECV_HANDLER 的不同次调用之间共享的数据, 比如例程把进度存在这里 */

        aiot_download_setopt(dl_handle, AIOT_DLOPT_USERDATA, (void *)NULL);

        if (query_firmware_flag == 0)
        {
            link_fota_power_download_firmware();
        }
        break;
    }

    default:
        break;
    }
}

int link_fota_power_start(void *mqtt_handle)
{
    /* 与MQTT例程不同的是, 这里需要增加创建OTA会话实例的语句 */
    ota_handle = aiot_ota_init();
    if (NULL == ota_handle)
    {
        printf("aiot_ota_init failed\r\n");
        return -2;
    }

    /* 用以下语句, 把OTA会话和MQTT会话关联起来 */
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_MQTT_HANDLE, mqtt_handle);
    /* 用以下语句, 设置OTA会话的数据接收回调, SDK收到OTA相关推送时, 会进入这个回调函数 */
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_RECV_HANDLER, demo_ota_recv_handler);
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_MODULE, "power");
    return 0;
}

int link_fota_power_report_version(char *cur_version)
{
    if (NULL == ota_handle)
    {
        printf("%s,ota_handle NULL\r\n", __func__);
        return -1;
    }
    if (get_link_connected_state() == 0)
    {
        printf("%s,not link_connected\r\n", __func__);
        return -1;
    }
    if (strcmp(version, cur_version) == 0)
    {
        printf("%s,version is same\r\n", __func__);
        return -1;
    }
    int32_t res = STATE_SUCCESS;

    res = aiot_ota_report_version(ota_handle, cur_version);
    if (res < STATE_SUCCESS)
    {
        printf("power aiot_ota_report_version failed: -0x%04X\r\n", -res);
    }
    else
        strcpy(version, cur_version);
    printf("power aiot_ota_report_version %s res:%d\r\n", cur_version, res);
    return res;
}

void link_fota_power_stop(void)
{
    /* 销毁OTA实例, 一般不会运行到这里 */
    aiot_ota_deinit(&ota_handle);
}

int link_fota_power_query_firmware(void)
{
    set_ota_power_state(OTA_IDLE, NULL);
    int res = aiot_ota_query_firmware(ota_handle);
    if (STATE_SUCCESS == res)
    {
        query_firmware_flag = 1;
        if (ota_timer_start_cb)
            ota_timer_start_cb();
    }
    else
    {
        set_ota_power_state(OTA_NO_FIRMWARE, NULL);
    }
    return res;
}