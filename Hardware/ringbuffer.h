#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#define RINGBUFFER_MAX 20

typedef struct {
    float   temp;
    uint8_t humi;
} SensorData_t;

typedef struct {
    SensorData_t buffer[RINGBUFFER_MAX];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} RingBuffer_t;

void RingBuffer_Init(RingBuffer_t *rb);
uint8_t RingBuffer_Push(RingBuffer_t *rb, SensorData_t data);
uint8_t RingBuffer_Peek(RingBuffer_t *rb, SensorData_t *data);
uint8_t RingBuffer_Pop(RingBuffer_t *rb, SensorData_t *data);
uint8_t RingBuffer_IsFull(RingBuffer_t *rb);
uint8_t RingBuffer_IsEmpty(RingBuffer_t *rb);

#endif
