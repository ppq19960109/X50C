#ifndef __LINKKIT_SOLO_H_
#define __LINKKIT_SOLO_H_
void linkkit_user_post_property(const char *property_payload);
void linkkit_user_post_event(const char *event_id, const char *event_payload);
void register_property_set_event_cb(int (*cb)(const int, const char *, const int));
void register_property_report_all_cb(int (*cb)());
void register_service_request_event_cb(int (*cb)(const int, const char *, const int, const char *, const int, char **, int *));
int linkkit_main(const char *product_key, const char *product_secret, const char *device_name, const char *device_secret);
void linkkit_close(void);
#endif
