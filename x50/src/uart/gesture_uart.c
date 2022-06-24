#include "main.h"

#include "uart_task.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "cloud_platform_task.h"
#include "uds_protocol.h"
static unsigned short gesture_seq_id = 0;
static int fd = -1;
static pthread_mutex_t lock;
static struct Select_Client_Event select_client_event;
static cloud_dev_t *g_gesture_dev = NULL;
static cloud_attr_t g_gesture_attr[] = {
    {
        cloud_key : "IceStOvMode",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        uart_cmd : 0x71,
        uart_byte_len : 1,
    },
    {
        cloud_key : "IceStOvState",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        uart_cmd : 0x72,
        uart_byte_len : 1,
    },
    {
        cloud_key : "IceStOvSetTemp",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        uart_cmd : 0x73,
        uart_byte_len : 2,
    },
    {
        cloud_key : "IceStOvSetTimer",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        uart_cmd : 0x75,
        uart_byte_len : 2,
    },
    {
        cloud_key : "IceStOvSetTimerLeft",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        uart_cmd : 0x76,
        uart_byte_len : 2,
    },
    {
        cloud_key : "IceHoodLight",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        uart_cmd : 0x79,
        uart_byte_len : 1,
    },
    // {
    //     cloud_key : "IceStOvOrderTimerLeft",
    //     cloud_value_type : LINK_VALUE_TYPE_NUM,
    //     cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT,
    //     uart_cmd : 0x7a,
    //     uart_byte_len : 2,
    // },
    {
        cloud_key : "IceStOvOperation",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        uart_cmd : 0x78,
        uart_byte_len : 1,
    },
    {
        cloud_key : "IceStOvRealTemp",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        uart_cmd : 0x74,
        uart_byte_len : 2,
    },
    {
        cloud_key : "IceHoodSpeed",
        cloud_value_type : LINK_VALUE_TYPE_NUM,
        cloud_fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        uart_cmd : 0x77,
        uart_byte_len : 1,
    }};

cloud_dev_t *get_gesture_dev()
{
    return g_gesture_dev;
}

static void *gesture_parse_json()
{
    int i;
    cloud_dev_t *cloud_dev = (cloud_dev_t *)malloc(sizeof(cloud_dev_t));
    if (cloud_dev == NULL)
    {
        dzlog_error("cloud_dev malloc error\n");
        return NULL;
    }
    cloud_dev->attr_len = sizeof(g_gesture_attr) / sizeof(g_gesture_attr[0]);
    cloud_dev->attr = g_gesture_attr;

    for (i = 0; i < cloud_dev->attr_len; ++i)
    {
        if (cloud_dev->attr[i].uart_byte_len > 0)
        {
            cloud_dev->attr[i].value = (char *)malloc(cloud_dev->attr[i].uart_byte_len);
            if (cloud_dev->attr[i].value == NULL)
            {
                dzlog_error("cloud_dev->attr[i].value malloc error\n");
                return NULL;
            }
            memset(cloud_dev->attr[i].value, 0, cloud_dev->attr[i].uart_byte_len);
        }
    }
    return cloud_dev;
}

int gesture_uart_send(unsigned char *in, int in_len)
{
    dzlog_warn("uart send to gesture--------------------------");
    hdzlog_info(in, in_len);
    if (fd < 0)
    {
        dzlog_error("gesture_uart_send fd error\n");
        return -1;
    }
    int res = 0;
    if (pthread_mutex_lock(&lock) == 0)
    {
        if (in == NULL || in_len <= 0)
        {
            res = 0;
            goto fail;
        }

        res = write(fd, in, in_len);

    fail:
        pthread_mutex_unlock(&lock);
        return res;
    }
    else
    {
        dzlog_error("pthread_mutex_lock error\n");
    }
    return res;
}

int gesture_uart_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, int seq_id)
{
    int index = 0;
    unsigned char *send_msg = (unsigned char *)malloc(ECB_MSG_MIN_LEN + msg_len);
    send_msg[index++] = 0xe6;
    send_msg[index++] = 0xe6;
    if (seq_id < 0)
        seq_id = gesture_seq_id++;

    send_msg[index++] = seq_id >> 8;
    send_msg[index++] = seq_id & 0xff;
    send_msg[index++] = command;
    send_msg[index++] = msg_len >> 8;
    send_msg[index++] = msg_len & 0xff;
    if (msg_len > 0 && msg != NULL)
    {
        memcpy(&send_msg[index], msg, msg_len);
        index += msg_len;
    }
    unsigned short crc16 = CRC16_MAXIM((const unsigned char *)(send_msg + 2), index - 2);
    send_msg[index++] = crc16 >> 8;
    send_msg[index++] = crc16 & 0xff;
    send_msg[index++] = 0x6e;
    send_msg[index++] = 0x6e;
    int res = gesture_uart_send(send_msg, ECB_MSG_MIN_LEN + msg_len);
    free(send_msg);

    return res;
}

int gesture_uart_send_cloud_msg(unsigned char *msg, const int msg_len)
{
    return gesture_uart_send_msg(ECB_UART_COMMAND_SET, msg, msg_len, -1);
}
int gesture_uart_msg_get()
{
    return gesture_uart_send_msg(ECB_UART_COMMAND_GET, NULL, 0, -1);
}

static int gesture_recv_cb(void *arg)
{
    static unsigned char uart_read_buf[256];
    int uart_read_len;
    static int uart_read_buf_index = 0;

    uart_read_len = read(fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf) - uart_read_buf_index);
    if (uart_read_len > 0)
    {
        uart_read_buf_index += uart_read_len;
        dzlog_warn("recv from gesture-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, ecb_uart_parse_msg,1);
        dzlog_warn("gesture uart_read_buf_index:%d", uart_read_buf_index);
        // hdzlog_info(uart_read_buf, uart_read_buf_index);
    }
    return 0;
}

void gesture_uart_deinit(void)
{
    for (int i = 0; i < g_gesture_dev->attr_len; ++i)
    {
        if (g_gesture_dev->attr[i].uart_byte_len > 0)
            free(g_gesture_dev->attr[i].value);
    }
    free(g_gesture_dev->attr);
    free(g_gesture_dev);
    close(fd);
    pthread_mutex_destroy(&lock);
}
// ntpdate pool.ntp.org
/*********************************************************************************
 *Function:  uart_ecb_task
 *Description： 手势任务函数，接收手势控制板的数据并处理
 *Input:
 *Return:
 **********************************************************************************/
void gesture_uart_init(void)
{
    fd = uart_init("/dev/ttyS3", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_BLOCK);
    if (fd < 0)
    {
        dzlog_error("gesture_uart uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("gesture_uart,fd:%d", fd);
    g_gesture_dev = gesture_parse_json();
    if (g_gesture_dev == NULL)
    {
        dzlog_error("gesture_parse_json error");
        return;
    }
    pthread_mutex_init(&lock, NULL);

    select_client_event.fd = fd;
    select_client_event.read_cb = gesture_recv_cb;
    add_select_client_uart(&select_client_event);
    gesture_uart_msg_get();
}
