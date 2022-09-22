#ifndef _CURL_HTTP_REQUEST_H_
#define _CURL_HTTP_REQUEST_H_
#ifdef __cplusplus
extern "C"
{
#endif

    int curl_http_post(const char *path, const char *body);
    int curl_http_request_init();
    void curl_http_request_deinit();
    void curl_http_request_reset();
    void http_report_hex(char *title,const unsigned char *buff, int buff_len);

#ifdef __cplusplus
}
#endif
#endif
