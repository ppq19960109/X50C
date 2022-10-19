#ifndef _COOK_ASSIST_H_
#define _COOK_ASSIST_H_
#ifdef __cplusplus
extern "C"
{
#endif
#include "cook_wrapper.h"
#include "cJSON.h"
    void cook_assist_init();
    void cook_assist_deinit();
    void cook_assist_report_all(cJSON *root);

    void set_stove_status(unsigned char status, enum INPUT_DIR input_dir);

    void cook_assist_set_smartSmoke(const char status);
    int cook_assist_link_recv(const char *key, cJSON *value);
#ifdef __cplusplus
}
#endif
#endif
