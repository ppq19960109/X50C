#ifndef _COMMONFUNC_H_
#define _COMMONFUNC_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "cJSON.h"
    void cjson_to_str(cJSON *in, char *out);
    int split_str(char *str, const char *delim, void (*cb)(const char *, unsigned int));
    long getFileSize(const char *path);
    int operateFile(int action, const char *path, char *buf, int len);
    void readFileList(const char *path, int (*readFileFunc)(const char *));
    void *get_dev_profile(const char *path, const char *devId, const char *modelId, void *(*func)(const char *, const char *));
    long *stol(const char *str, int base, long *out);
    double *stod(const char *str, double *out);
    int findStrIndex(const char *key, char *const *array, const int arrayLen);
    int findStrnIndex(const char *key, const int n, char *const *array, const int arrayLen);
    char getByteForJson(cJSON *json, const char *src, char *dst);
    int getNumForJson(cJSON *json, const char *src, int *dst);
    char *getStrForJson(cJSON *json, const char *src, char *dst);

#ifdef __cplusplus
}
#endif
#endif