#ifndef _UDS_PROTLCOL_H_
#define _UDS_PROTLCOL_H_

int uds_protocol_init(void);
void uds_protocol_deinit(void);
int send_to_uds(unsigned char *data, unsigned int len);
#endif