
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "app_uart_ecb.h"
#include "ecb_parsing.h"
#include "uart_resend.h"
#include "cloud.h"

extern void ecb_resend_list_add(resend_t *resend);
extern void ecb_resend_list_del_by_id(const int resend_seq_id);

static const char *TAG = "uart_ecb_protocol";
static unsigned short ecb_seq_id = 0;

typedef enum
{
    ECB_UART_READ_VALID = 0,
    ECB_UART_READ_NO_HEADER,
    ECB_UART_READ_LEN_SMALL,
    ECB_UART_READ_CHECK_ERR,
    ECB_UART_READ_TAILER_ERR
} ecb_uart_read_status_t;

static void InvertUint16(unsigned short *dBuf, unsigned short *srcBuf)
{
    int i;
    unsigned short tmp[4] = {0};

    for (i = 0; i < 16; i++)
    {
        if (srcBuf[0] & (1 << i))
            tmp[0] |= 1 << (15 - i);
    }
    dBuf[0] = tmp[0];
}

unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen)
{
    unsigned short wCRCin = 0x0000;
    unsigned short wCPoly = 0x8005;

    InvertUint16(&wCPoly, &wCPoly);
    while (datalen--)
    {
        wCRCin ^= *(data++);
        for (int i = 0; i < 8; i++)
        {
            if (wCRCin & 0x01)
                wCRCin = (wCRCin >> 1) ^ wCPoly;
            else
                wCRCin = wCRCin >> 1;
        }
    }
    return (wCRCin ^ 0xFFFF);
}

int ecb_uart_send_msg(unsigned char command, unsigned short msg_len, unsigned char *msg)
{
    int index = 0;
    unsigned char *send_msg = (unsigned char *)pvPortMalloc(ECB_MSG_MIN_LEN + msg_len);
    send_msg[index++] = 0xe6;
    send_msg[index++] = 0xe6;
    unsigned short seq_id = ecb_seq_id++;
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
    return uart_send_data(CONFIG_ECB_UART_PORT_NUM, send_msg, ECB_MSG_MIN_LEN + msg_len, 0, 0);
}

int ecb_uart_send_nak(unsigned char error_code)
{
    return ecb_uart_send_msg(ECB_UART_COMMAND_NAK, 1, &error_code);
}

static int uart_data_parse(const unsigned char *in, const int in_len, int *end)
{
    int index, i;
    if (in_len < 2)
    {
        *end = 0;
        ESP_LOGE(TAG, "input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }

    for (i = 0; i < in_len - 1; ++i)
    {
        if (in[i] == 0xe6 && in[i + 1] == 0xe6)
            break;
    }
    if (i >= in_len - 1)
    {
        ESP_LOGE(TAG, "no header was detected");
        return ECB_UART_READ_NO_HEADER;
    }
    index = i;

    if (index + in_len < ECB_MSG_MIN_LEN)
    {
        *end = index;
        ESP_LOGE(TAG, "input len too small");
        return ECB_UART_READ_LEN_SMALL;
    }
    int msg_index = 2;

    const unsigned short seq_id = in[index + msg_index] * 256 + in[index + msg_index + 1];
    msg_index += 2;
    unsigned char command = in[index + msg_index];
    msg_index += 1;
    int data_len = in[index + msg_index] * 256 + in[index + msg_index + 1];
    msg_index += 2;
    const unsigned char *payload = &in[index + msg_index];
    msg_index += data_len;
    if (index + msg_index + 2 + 2 > in_len)
    {
        *end = index;
        ESP_LOGE(TAG, "input data len too small");
        return ECB_UART_READ_LEN_SMALL;
    }
    else if (in[index + msg_index + 2] != 0x6e || in[index + msg_index + 2 + 1] != 0x6e)
    {
        *end = index + msg_index + 2 + 2;
        ESP_LOGE(TAG, "no tailer was detected");
        ecb_uart_send_nak(ECB_NAK_TAILER);
        return ECB_UART_READ_TAILER_ERR;
    }
    unsigned short crc16 = CRC16_MAXIM(&in[index + 2], msg_index - 2);
    unsigned short check_sum = in[index + msg_index] * 256 + in[index + msg_index + 1];
    ESP_LOGW(TAG, "crc16:%x,check_sum:%x", crc16, check_sum);
    msg_index += 2;
    msg_index += 2;
    ESP_LOG_BUFFER_HEXDUMP(TAG, &in[index], msg_index, ESP_LOG_INFO);
    *end = index + msg_index;
    // if (crc16 != check_sum)
    // {
    //     ESP_LOGE(TAG, "data check error");
    //     ecb_uart_send_nak(ECB_NAK_CHECKSUM);
    //     return ECB_UART_READ_CHECK_ERR;
    // }
    //----------------------
    ESP_LOGI(TAG, "command:%d\n", command);
    ESP_LOG_BUFFER_HEXDUMP(TAG, payload, data_len, ESP_LOG_INFO);
    if (command == ECB_UART_COMMAND_EVENT || command == ECB_UART_COMMAND_KEYPRESS)
    {
        ecb_uart_send_msg(ECB_UART_COMMAND_ACK, 0, NULL);
        if (command == ECB_UART_COMMAND_EVENT)
        {
        }
        else if (command == ECB_UART_COMMAND_KEYPRESS)
        {
        }
        else
        {
        }
        recv_data_to_cloud(command, payload, data_len);
    }
    else if (command == ECB_UART_COMMAND_GETACK)
    {
        ecb_resend_list_del_by_id(seq_id);
    }
    else if (command == ECB_UART_COMMAND_ACK || command == ECB_UART_COMMAND_NAK)
    {
        ecb_resend_list_del_by_id(seq_id);
    }
    else
    {
        ecb_uart_send_nak(ECB_NAK_COMMAND_UNKNOWN);
    }

    return ECB_UART_READ_VALID;
}

void uart_read_parse(unsigned char *in, int *in_len)
{
    int start = 0;
    ecb_uart_read_status_t status;
    for (;;)
    {
        ESP_LOGI(TAG, "start:%d,in len:%d", start, *in_len);
        status = uart_data_parse(&in[start], *in_len, &start);
        *in_len -= start;

        if (status == ECB_UART_READ_VALID || status == ECB_UART_READ_CHECK_ERR || status == ECB_UART_READ_TAILER_ERR)
        {
            if (*in_len <= 0)
            {
                break;
            }
        }
        else if (status == ECB_UART_READ_LEN_SMALL)
        {
            break;
        }
        else if (status == ECB_UART_READ_NO_HEADER)
        {
            *in_len = 0;
            break;
        }
        else
        {
        }
    }
    ESP_LOGI(TAG, "last start:%d,in len:%d", start, *in_len);
    if (*in_len <= 0)
    {
        *in_len = 0;
    }
    else if (start > 0)
    {
        memmove(in, &in[start], *in_len);
    }
}
