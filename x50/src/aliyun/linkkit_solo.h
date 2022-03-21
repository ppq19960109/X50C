#ifndef __LINKKIT_SOLO_H_
#define __LINKKIT_SOLO_H_

#define EXAMPLE_MASTER_DEVID (0)
#define EXAMPLE_YIELD_TIMEOUT_MS (200)

#define EXAMPLE_TRACE(...)                                      \
    do                                                          \
    {                                                           \
        HAL_Printf("\033[1;32;40m%s.%d: ", __func__, __LINE__); \
        HAL_Printf(__VA_ARGS__);                                \
        HAL_Printf("\033[0m\r\n");                              \
    } while (0)

#define PRODUCT_KEY "ProductKey"
#define PRODUCT_SECRET "ProductSecret"
#define DEVICE_NAME "DeviceName"
#define DEVICE_SECRET "DeviceSecret"

void linkkit_user_post_property(const char *property_payload);
void linkkit_user_post_event(char *event_id, char *event_payload);

void register_property_set_event_cb(int (*cb)(const int, const char *, const int));
void register_property_report_all_cb(int (*cb)());
void register_service_request_event_cb(int (*cb)(const int, const char *, const int, const char *, const int, char **, int *));
void register_connected_cb(void (*cb)(int));
void register_dynreg_device_secret_cb(int (*cb)(const char *));
void register_link_timestamp_cb(void (*cb)(const char *));
void register_fota_event_handler_cb(void (*cb)(char*));

int linkkit_init(const char *product_key, const char *product_secret, const char *device_name, const char *device_secret, const char *version);
void linkkit_close(void);

int get_linkkit_connected_state();
void get_linkkit_quad(char *product_key, char *product_secret, char *device_name, char *device_secret);
#endif
