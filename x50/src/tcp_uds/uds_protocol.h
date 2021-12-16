#ifndef _UDS_PROTLCOL_H_
#define _UDS_PROTLCOL_H_

#include "cJSON.h"

#define FRAME_HEADER (0xAA)
#define FRAME_TAIL (0xBB)

#define TYPE "Type"
#define TYPE_SET "SET"
#define TYPE_GET "GET"
#define TYPE_GETALL "GETALL"
#define TYPE_EVENT "EVENT"

#define DATA "Data"

int uds_protocol_init(void);
void uds_protocol_deinit(void);
int send_event_uds(cJSON *send);
#endif