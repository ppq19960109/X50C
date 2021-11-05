#ifndef _CLOUD_H_
#define _CLOUD_H_


#define PROFILE_PATH "."
#define PROFILE_NAME "X50BCZ"
int cloud_init(void);
void cloud_deinit();
void recv_data_to_cloud(const unsigned char cmd,const unsigned char *value, const int value_len);

#endif