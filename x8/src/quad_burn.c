#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "curl/curl.h"
#include "md5_func.h"

int (*save_quad_cb)(const char *, const char *, const char *, const char *);
void register_save_quad_cb(int (*cb)(const char *, const char *, const char *, const char *))
{
    save_quad_cb = cb;
}
int (*report_message_cb)(const char *);
void register_report_message_cb(int (*cb)(const char *))
{
    report_message_cb = cb;
}
void (*quad_burn_success_cb)();
void register_quad_burn_success_cb(void (*cb)())
{
    quad_burn_success_cb = cb;
}

#define QUAD_REQUEST_URL_FMT "http://%s:58396"
static char g_product_key[33];
static char g_mac[18];
static char quad_request_url[48];
static size_t http_quad_cb(void *ptr, size_t size, size_t nmemb, void *stream);

static int curl_http_quad(const char *path, const char *body)
{
    char buf[160];
    CURLcode res;
    struct curl_slist *headers = NULL;

    // get a curl handle
    CURL *curl = curl_easy_init();
    if (curl)
    {
        headers = curl_slist_append(headers, "Content-Type:application/json");
        sprintf(buf, "pk:%s", g_product_key);
        headers = curl_slist_append(headers, buf);
        sprintf(buf, "mac:%s", g_mac);
        headers = curl_slist_append(headers, buf);

        sprintf(buf, "%s%s%s%s", "1643738522000", g_product_key, g_mac, "mars");
        char md5_str[33] = {0};
        Compute_string_md5((unsigned char *)buf, strlen(buf), md5_str);
        sprintf(buf, "sign:%s.%s", "1643738522000", md5_str);
        headers = curl_slist_append(headers, buf);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        // set the URL with GET request
        sprintf(buf, "%s%s", quad_request_url, path);
        curl_easy_setopt(curl, CURLOPT_URL, buf);

        if (body == NULL)
        {
            // write response msg into strResponse
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_quad_cb);
            // curl_easy_setopt(curl, CURLOPT_WRITEDATA, master_meta_info);
            // curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            body = "";
        }
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_POST, 1);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        // perform the request, res will get the return code
        res = curl_easy_perform(curl);
        // check for errors
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            if (report_message_cb)
                report_message_cb("烧录服务器连接失败");
        }
        else
        {
            fprintf(stdout, "curl_easy_perform() success.\n");
        }
        // always cleanup
        curl_easy_cleanup(curl);
    }
    return 0;
}

size_t http_quad_cb(void *ptr, size_t size, size_t nmemb, void *stream)
{
    printf("http_quad_cb size:%lu,nmemb:%lu\n", size, nmemb);
    printf("http_quad_cb data:%s\n", (char *)ptr);
    // iotx_linkkit_dev_meta_info_t *master_meta_info = (iotx_linkkit_dev_meta_info_t *)stream;
    int res = 2, quad_rupleId = 0;
    cJSON *root = cJSON_Parse(ptr);
    if (root == NULL)
        return -1;
    // char *json = cJSON_PrintUnformatted(root);
    // printf("http_quad_cb json:%s\n", json);
    // free(json);

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code == NULL)
    {
        goto fail;
    }
    if (code->valueint != 0)
    {
        cJSON *message = cJSON_GetObjectItem(root, "message");
        if (message != NULL && cJSON_IsString(message))
        {
            if (report_message_cb)
                report_message_cb(message->valuestring);
        }
        goto fail;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data == NULL)
    {
        goto fail;
    }
    if (cJSON_HasObjectItem(data, "quadrupleId"))
    {
        cJSON *quadrupleId = cJSON_GetObjectItem(data, "quadrupleId");
        quad_rupleId = quadrupleId->valueint;
    }
    res = 0;
    cJSON *ProductKey, *ProductSecret, *DeviceName, *DeviceSecret;
    if (cJSON_HasObjectItem(data, "productKey"))
    {
        ProductKey = cJSON_GetObjectItem(data, "productKey");
        if (strlen(ProductKey->valuestring) > 0)
            ++res;
    }
    if (cJSON_HasObjectItem(data, "productSecret"))
    {
        ProductSecret = cJSON_GetObjectItem(data, "productSecret");
        if (strlen(ProductSecret->valuestring) > 0)
            ++res;
    }
    if (cJSON_HasObjectItem(data, "deviceName"))
    {
        DeviceName = cJSON_GetObjectItem(data, "deviceName");
        if (strlen(DeviceName->valuestring) > 0)
            ++res;
    }
    if (cJSON_HasObjectItem(data, "deviceSecret"))
    {
        DeviceSecret = cJSON_GetObjectItem(data, "deviceSecret");
        if (strlen(DeviceSecret->valuestring) > 0)
            ++res;
    }

    if (res == 4 && save_quad_cb)
    {
        res = 1;
        save_quad_cb(ProductKey->valuestring, ProductSecret->valuestring, DeviceName->valuestring, DeviceSecret->valuestring);
    }
    else
        res = 2;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "quadrupleId", quad_rupleId);
    cJSON_AddNumberToObject(resp, "state", res);
    if (res != 1)
    {
        cJSON_AddStringToObject(resp, "QuadInfo", "四元组错误，可能存在空值");
    }
    char *body = cJSON_PrintUnformatted(resp);
    printf("http_quad_cb report json:%s\n", body);

    curl_http_quad("/iot/quadruple/report", body);

    cJSON_free(body);
    cJSON_Delete(resp);
    if (res == 1)
    {
        if (quad_burn_success_cb)
            quad_burn_success_cb();
    }
fail:
    cJSON_Delete(root);
    return size * nmemb;
}

void quad_burn_init()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void quad_burn_deinit()
{
    curl_global_cleanup();
}

int quad_burn_requst(const char *product_key, const char *mac, const char *request_ip)
{
    if (product_key == NULL || mac == NULL || request_ip == NULL)
        return -1;
    strcpy(g_product_key, product_key);
    strcpy(g_mac, mac);
    sprintf(quad_request_url, QUAD_REQUEST_URL_FMT, request_ip);
    return curl_http_quad("/iot/quadruple/apply", NULL);
}
