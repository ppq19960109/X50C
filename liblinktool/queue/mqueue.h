#ifndef _MQUEUE_H_
#define _MQUEUE_H_

#include <stdbool.h>

typedef void *QDataType;
typedef struct QueueNode
{
    struct QueueNode *next;
    QDataType data;
} QNode;
typedef struct
{
    QNode *tail; // 尾指针
    QNode *head; // 头指针
    int size;
} Queue;

void QueueInit(Queue *pq);
void QueuePush(Queue *pq, QDataType x);
QDataType QueuePop(Queue *pq);
QDataType QueueFront(Queue *pq);
QDataType QueueBack(Queue *pq);
bool QueueEmpty(Queue *pq);
int QueueSize(Queue *pq);
void QueueDestory(Queue *pq);
#endif