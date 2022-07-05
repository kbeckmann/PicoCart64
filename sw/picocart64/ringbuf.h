#pragma once

#include <stdint.h>

// ARRAY_SIZE
#include "utils.h"

#define RINGBUF_CREATE(_name, _size) \
    struct {                                                \
        uint32_t buf[(_size)];                              \
        uint32_t head;                                      \
        uint32_t tail;                                      \
    } _name

#define ringbuf_add(_ringbuf, _value)                       \
    do {                                                    \
        _ringbuf.buf[_ringbuf.head++] = (_value);           \
        if (_ringbuf.head == ARRAY_SIZE(_ringbuf.buf)) {    \
            _ringbuf.head = 0;                              \
        }                                                   \
    } while(0)

#define ringbuf_print(_ringbuf)                             \
    while (_ringbuf.tail != _ringbuf.head) {                \
        printf("%08lX\n", _ringbuf.buf[_ringbuf.tail]);     \
        _ringbuf.tail++;                                    \
        if (_ringbuf.tail == ARRAY_SIZE(_ringbuf.buf)) {    \
            _ringbuf.tail = 0;                              \
        }                                                   \
    }
