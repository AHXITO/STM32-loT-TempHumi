#include "stm32f10x.h"                  // Device header
#include "ringbuffer.h"

void RingBuffer_Init(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

uint8_t RingBuffer_Push(RingBuffer_t *rb, SensorData_t data)
{
    /* 满了就覆盖最旧数据：丢弃 tail 指向的那条，再写入新数据 */
    if (RingBuffer_IsFull(rb)) {
        rb->tail = (rb->tail + 1) % RINGBUFFER_MAX;
        rb->count--;
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RINGBUFFER_MAX;
    rb->count++;
    return 1;
}

uint8_t RingBuffer_Peek(RingBuffer_t *rb, SensorData_t *data)
{
    if (RingBuffer_IsEmpty(rb)) return 0;
    *data = rb->buffer[rb->tail];
    return 1;
}

uint8_t RingBuffer_Pop(RingBuffer_t *rb, SensorData_t *data)
{
    if (RingBuffer_IsEmpty(rb)) return 0;
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RINGBUFFER_MAX;
    rb->count--;
    return 1;
}

uint8_t RingBuffer_IsFull(RingBuffer_t *rb)
{
    return rb->count == RINGBUFFER_MAX;
}

uint8_t RingBuffer_IsEmpty(RingBuffer_t *rb)
{
    return rb->count == 0;
}
