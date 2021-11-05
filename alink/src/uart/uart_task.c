#include <stdio.h>
#include <string.h>

#include "UartCfg.h"
#include "uart_task.h"
#include "ecb_parse.h"
#include "uart_resend.h"
#include "cloud.h"



int uart_send_data(uart_port_t uart_num, unsigned char *in, int in_len, int resend, int iscopy)
{
    int res = 0;
    if (xSemaphoreTake(xSemaphore, (TickType_t)1000 / portTICK_PERIOD_MS) == pdTRUE)
    {
        if (in == NULL || in_len <= 0)
        {
            res = 0;
            goto fail;
        }
        ESP_LOGI(TASK_TAG, "uart_num:%d uart_send_data:", uart_num);
        ESP_LOG_BUFFER_HEXDUMP(TASK_TAG, in, in_len, ESP_LOG_INFO);
        res = uart_write_bytes(uart_num, (const char *)in, in_len);
        if (resend)
        {
            resend_t *resend = (resend_t *)pvPortMalloc(sizeof(resend_t));
            resend->send_len = in_len;
            if (iscopy)
            {
                resend->send_data = (unsigned char *)pvPortMalloc(resend->send_len);
                memcpy(resend->send_data, in, resend->send_len);
            }
            else
                resend->send_data = in;

            resend->uart_num = uart_num;
            resend->resend_cnt = RESEND_CNT;
            resend->wait_end_tick = tick_count_add(xTaskGetTickCount(), RESEND_WAIT_TICK);
            ecb_resend_list_add(resend);
        }
    fail:
        xSemaphoreGive(xSemaphore);
        return res;
    }
    else
    {
        ESP_LOGE(TASK_TAG, "uart_send_data xSemaphoreTake error..................... \r\n");
        return res;
    }
}

void app_ecb_task(void *arg)
{
    esp_log_level_set(TASK_TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TASK_TAG, "app_ecb_task start");
    xSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xSemaphore);

    // ecb_uart_queue_create();
    esp_uart_init(UART_NUM_1, CONFIG_ECB_UART_BAUD_RATE, GPIO_NUM_27, GPIO_NUM_14);
    uint8_t *uart1_read_buf = (uint8_t *)malloc(UART_BUF_SIZE);
    int uart1_read_len, uart1_read_buf_index = 0;


    cloud_init();
    // ecb_uart_queue_msg_t ecb_uart_queue_msg;
    esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    while (1)
    {
        uart1_read_len = uart_read_bytes(UART_NUM_1, &uart1_read_buf[uart1_read_buf_index], UART_BUF_SIZE - uart1_read_buf_index, 20 / portTICK_RATE_MS);
        if (uart1_read_len > 0)
        {
            uart_send_data(UART_NUM_2, &uart1_read_buf[uart1_read_buf_index], uart1_read_len, 0, 0);
            uart1_read_buf_index += uart1_read_len;
            ESP_LOGW(TASK_TAG, "uart1_read len:%d", uart1_read_buf_index);
            ESP_LOG_BUFFER_HEXDUMP(TASK_TAG, uart1_read_buf, uart1_read_buf_index, ESP_LOG_WARN);
            uart_read_parse(uart1_read_buf, &uart1_read_buf_index);
            ESP_LOGW(TASK_TAG, "uart_read_parse len:%d", uart1_read_buf_index);
            ESP_LOG_BUFFER_HEXDUMP(TASK_TAG, uart1_read_buf, uart1_read_buf_index, ESP_LOG_WARN);
        }

        resend_list_each(&ECB_LIST_RESEND);
        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
    ESP_ERROR_CHECK(esp_task_wdt_delete(xTaskGetCurrentTaskHandle()));
    cloud_deinit();
    free(uart1_read_buf);
    uart_driver_delete(UART_NUM_1);
#ifdef USE_UART_NUM_2
    free(uart2_read_buf);
    uart_driver_delete(UART_NUM_2);
#endif
}
