#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mqueue.h"

void QueueInit(Queue *pq)
{
    if (pq == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return;
    }
    pq->head = pq->tail = NULL;
    pq->size = 0;
}

void QueuePush(Queue *pq, QDataType x)
{
    if (pq == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return;
    }
    QNode *newnode = (QNode *)malloc(sizeof(QNode));
    if (newnode == NULL)
    {
        printf("malloc fail\n");
        return;
    }
    newnode->data = x;
    newnode->next = NULL;
    if (pq->tail == NULL)
    {
        pq->head = pq->tail = newnode;
    }
    else
    {
        pq->tail->next = newnode;
        pq->tail = newnode;
    }
    pq->size++;
}

QDataType QueuePop(Queue *pq) // 头删
{
    if (pq == NULL || pq->head == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return 0;
    }
    if (pq->size == 0)
        return 0;
    QNode *next;
    QDataType data;

    next = pq->head->next;
    data = pq->head->data;
    free(pq->head);
    pq->head = next;
    if (pq->head == NULL)
        pq->tail = NULL;
    pq->size--;
    return data;
}

QDataType QueueFront(Queue *pq)
{
    if (pq == NULL || pq->head == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return 0;
    }

    return pq->head->data;
}

QDataType QueueBack(Queue *pq)
{
    if (pq == NULL || pq->head == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return 0;
    }

    return pq->tail->data;
}

bool QueueEmpty(Queue *pq)
{
    if (pq == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return 0;
    }
    if (pq->size == 0)
        return 1;
    else
        return 0;
}

int QueueSize(Queue *pq)
{
    if (pq == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return 0;
    }
    return pq->size;
}

void QueueDestory(Queue *pq)
{
    if (pq == NULL)
    {
        printf("%s,Queue is NULL\n", __func__);
        return;
    }

    QNode *cur = pq->head;
    while (cur)
    {
        QNode *next = cur->next;
        free(cur);
        cur = next;
    }

    pq->head = pq->tail = NULL;
}
