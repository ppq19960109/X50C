#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ring_buffer.h"
#include "mlog.h"
/**
 1. 初始化ring_buffer
 2. malloc开辟传入的buff_size大小的空间存放buffer
 3. read_offset write_offset valid_size均置为０
*/
void ring_buffer_init(ring_buffer_t *ring_buffer, int buff_size, int element_size)
{
    ring_buffer->buffer = malloc(buff_size * element_size);
    memset(ring_buffer->buffer, 0, buff_size * element_size);
    ring_buffer->element_size = element_size;
    ring_buffer->read_offset = 0;
    ring_buffer->write_offset = 0;
    ring_buffer->valid_size = 0;
    ring_buffer->total_size = buff_size;
}

/**
 *释放ring_buffer
 */
void ring_buffer_deinit(ring_buffer_t *ring_buffer)
{
    if (ring_buffer->buffer != NULL)
    {
        free(ring_buffer->buffer);
    }
    // memset(ring_buffer, 0, sizeof(ring_buffer_t));
}

/**
 * buff:需要写入的数据的地址
 * size:需要写入的数据的大小
 */
int ring_buffer_write(ring_buffer_t *ring_buffer, void *buff, int size)
{
    if (size == 0)
        return 0;
    int write_offset = ring_buffer->write_offset;
    int total_size = ring_buffer->total_size;
    int element_size = ring_buffer->element_size;
    int first_write_size = 0;

    if (size + write_offset <= total_size) // ring_buffer->buffer的后段未写入的空间不小于size
    {
        memcpy(ring_buffer->buffer + write_offset * element_size, buff, size * element_size);
    }
    else // ring_buffer->buffer的后段未写入的空间小于size,这时候需要先在后面写入一部分，然后返回头部，从前面接着写入
    {
        first_write_size = total_size - write_offset;
        memcpy(ring_buffer->buffer + write_offset * element_size, buff, first_write_size * element_size);
        memcpy(ring_buffer->buffer, buff + first_write_size * element_size, (size - first_write_size) * element_size);
    }
    ring_buffer->write_offset += size;
    ring_buffer->write_offset %= total_size;

    if (ring_buffer->valid_size + size > total_size) // ring_buffer->buffer未使用的总大小比需要写入的size小
    {
        printf("ring buffer is to short to save buff\n");
        printf("total_size:%d valid_size:%d size:%d\n", ring_buffer->total_size, ring_buffer->valid_size, size);
        ring_buffer->valid_size = total_size;
        ring_buffer->read_offset = ring_buffer->write_offset;
    }
    else
    {
        ring_buffer->valid_size += size;
    }
    return size;
}

int ring_buffer_push(ring_buffer_t *ring_buffer, void *buff)
{
    return ring_buffer_write(ring_buffer, buff, 1);
}

int ring_buffer_peek(ring_buffer_t *ring_buffer, void *buff, int size)
{
    if (ring_buffer->valid_size == 0)
        return 0;
    int read_offset = ring_buffer->read_offset;
    int total_size = ring_buffer->total_size;
    int element_size = ring_buffer->element_size;
    int first_read_size = 0;

    if (size > ring_buffer->valid_size)
    {
        printf("valid size < read size\n");
        printf("valid size:%d read size:%d\n", ring_buffer->valid_size, size);
        size = ring_buffer->valid_size;
    }

    if (total_size - read_offset >= size)
    {
        memcpy(buff, ring_buffer->buffer + read_offset * element_size, size * element_size);
    }
    else
    {
        first_read_size = total_size - read_offset;
        memcpy(buff, ring_buffer->buffer + read_offset * element_size, first_read_size * element_size);
        memcpy(buff + first_read_size * element_size, ring_buffer->buffer, (size - first_read_size) * element_size);
    }

    return size;
}

int ring_buffer_read(ring_buffer_t *ring_buffer, void *buff, int size)
{
    if (ring_buffer->valid_size == 0)
        return 0;
    int total_size = ring_buffer->total_size;
    size = ring_buffer_peek(ring_buffer, buff, size);

    ring_buffer->read_offset += size;
    ring_buffer->read_offset %= total_size;
    ring_buffer->valid_size -= size;
    return size;
}

int ring_buffer_is_full(ring_buffer_t *ring_buffer)
{
    return ring_buffer->valid_size == ring_buffer->total_size;
}

int ring_buffer_valid_size(ring_buffer_t *ring_buffer)
{
    return ring_buffer->valid_size;
}

void ring_buffer_clear(ring_buffer_t *ring_buffer)
{
    ring_buffer->read_offset = 0;
    ring_buffer->write_offset = 0;
    ring_buffer->valid_size = 0;
}
