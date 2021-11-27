#ifndef _UDS_RECV_SEND_H_
#define _UDS_RECV_SEND_H_

#define TYPE "Type"
#define TYPE_SET "SET"
#define TYPE_GET "GET"
#define TYPE_GETALL "GETALL"
#define TYPE_EVENT "EVENT"

#define DATA "Data"

void uds_recv_send_init();
int send_event_uds(cJSON *send);
#endif