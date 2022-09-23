#include "main.h"
#include "link_solo.h"

static pthread_mutex_t mutex;
static CURL *curl = NULL;
static size_t curl_http_request_cb(void *ptr, size_t size, size_t nmemb, void *stream)
{
    printf("%s size:%lu,nmemb:%lu ptr:%s\n", __func__, size, nmemb, (char *)ptr);

    // cJSON *root = cJSON_Parse(ptr);
    // if (root == NULL)
    //     return -1;
    // char *json = cJSON_PrintUnformatted(root);
    // printf("%s json:%s\n", __func__, json);
    // free(json);
    return size * nmemb;
}
int curl_http_post(const char *path, const char *body)
{
    if (get_link_connected_state() == 0)
    {
        dzlog_warn("%s,link disconnect", __func__);
        return -1;
    }
    CURLcode res;
    struct curl_slist *headers = NULL;
    pthread_mutex_lock(&mutex);
    if (curl)
    {
        headers = curl_slist_append(headers, "Content-Type:application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_URL, path);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);
        curl_easy_setopt(curl, CURLOPT_POST, 1);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_http_request_cb);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        // perform the request, res will get the return code
        res = curl_easy_perform(curl);
        // check for errors
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            fprintf(stdout, "curl_easy_perform() success.\n");
        }
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

static char http_report_hex_buf[2560];
static int http_report_hex_len = 0;
static pthread_mutex_t mutex_log;
int curl_http_request_init()
{
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex_log, NULL);
    curl = curl_easy_init();
    return 0;
}
void curl_http_request_deinit()
{
    curl_easy_cleanup(curl);
    pthread_mutex_destroy(&mutex_log);
    pthread_mutex_destroy(&mutex);
}
void curl_http_request_reset()
{
    curl_easy_reset(curl);
}

static int hex_to_str(const unsigned char *buff, int buff_len, char *out, int out_len)
{
    time_t now_time = time(NULL);
    struct tm *now_tm = localtime(&now_time);
    strftime(out, 21, "%Y-%m-%d %H:%M:%S,", now_tm);
    int len = strlen(out);
    // printf("hex_to_str time:%s,%d,%s\n", ctime(&now_time), len, out);

    int str_len = buff_len * 3 + 1 + len;
    if (str_len >= out_len)
        return -1;

    for (int i = 0; i < buff_len; ++i)
    {
        len += sprintf(out + len, " %02x", buff[i]);
    }
    out[str_len - 1] = '\n';
    return str_len;
}
void http_report_hex(char *title, const unsigned char *buff, int buff_len)
{
    pthread_mutex_lock(&mutex_log);
    int len;
    strncpy(http_report_hex_buf + http_report_hex_len, title, 16);
    http_report_hex_len += strlen(title);
    len = hex_to_str(buff, buff_len, http_report_hex_buf + http_report_hex_len, sizeof(http_report_hex_buf) - http_report_hex_len);
    if (len > 0)
        http_report_hex_len += len;

    if (http_report_hex_len >= 2048 || len < 0)
    {
        printf("http_report_hex:%.*s\n", http_report_hex_len, http_report_hex_buf);
        http_report_hex_buf[http_report_hex_len - 1] = 0;
        // curl_http_post("", http_report_hex_buf);
        http_report_hex_len = 0;
    }
    pthread_mutex_unlock(&mutex_log);
}