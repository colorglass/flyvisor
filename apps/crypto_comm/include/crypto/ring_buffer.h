#pragma once
#include <stdint.h>
#include <stddef.h>
#define MSG_BUFFER_SIZE 0x2000

#define OFFSET_OF(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)

typedef struct ring_buffer {
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint8_t buf[];
} ring_buffer_t;

static inline void ring_buffer_init(ring_buffer_t *rb, uint32_t size)
{
    rb->head = 0;
    rb->tail = 0;
    size_t offset = OFFSET_OF(ring_buffer_t, buf);
    rb->size = size - offset;
}

static inline int ring_buffer_is_full(ring_buffer_t *rb)
{
    return (rb->tail + 1) % rb->size == rb->head;
}

static inline int ring_buffer_is_empty(ring_buffer_t *rb)
{
    return rb->tail == rb->head;
}

static inline void ring_buffer_put(ring_buffer_t *rb, uint8_t c)
{
    rb->buf[rb->tail] = c;
    rb->tail = (rb->tail + 1) % rb->size;
}

static inline uint8_t ring_buffer_get(ring_buffer_t *rb)
{
    uint8_t c = rb->buf[rb->head];
    rb->head = (rb->head + 1) % rb->size;
    return c;
}
