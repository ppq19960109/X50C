#ifndef _QUAD_BURN_H_
#define _QUAD_BURN_H_

void register_save_quad_cb(int (*cb)(const char *, const char *, const char *, const char *));
void register_report_message_cb(int (*cb)(const char *));
void register_quad_burn_success_cb(void (*cb)());

int quad_burn_requst(const char *product_key, const char *mac, const char *request_ip);
#endif