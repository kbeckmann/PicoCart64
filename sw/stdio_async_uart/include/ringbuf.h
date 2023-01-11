/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#include <stdint.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

#define RINGBUF_CREATE(_name, _size, _type)                                   \
    struct {                                                                  \
        _type    buf[(_size)];                                                \
        uint32_t head;                                                        \
        uint32_t tail;                                                        \
    } _name

#define ringbuf_add(_ringbuf, _value) do {                                    \
    _ringbuf.buf[_ringbuf.head++] = (_value);                                 \
    if (_ringbuf.head == ARRAY_SIZE(_ringbuf.buf)) {                          \
        _ringbuf.head = 0;                                                    \
    }                                                                         \
} while(0)

#define ringbuf_add_buf(_ringbuf, _buf, _len) do {                            \
    int32_t bytes_to_end = sizeof(_ringbuf.buf) - _ringbuf.head;              \
    int32_t copy_len = (_len > bytes_to_end) ? bytes_to_end : _len;           \
    memcpy(&_ringbuf.buf[_ringbuf.head], (_buf), copy_len);                   \
    _ringbuf.head += copy_len;                                                \
    copy_len = ((_len) - copy_len) > sizeof(_ringbuf.buf) ?                   \
                sizeof(_ringbuf.buf) : (_len) - copy_len;                     \
    if (copy_len > 0) {                                                       \
        memcpy(_ringbuf.buf, &(_buf)[bytes_to_end], copy_len);                \
        _ringbuf.head = copy_len;                                             \
    }                                                                         \
    if (_ringbuf.head == ARRAY_SIZE(_ringbuf.buf)) {                          \
        _ringbuf.head = 0;                                                    \
    }                                                                         \
} while(0)

#define ringbuf_get_tail_ptr(_ringbuf)                                        \
    &_ringbuf.buf[_ringbuf.tail]

#define ringbuf_print(_ringbuf)                                               \
    while (_ringbuf.tail != _ringbuf.head) {                                  \
        printf("%08lX\n", _ringbuf.buf[_ringbuf.tail]);                       \
        _ringbuf.tail++;                                                      \
        if (_ringbuf.tail == ARRAY_SIZE(_ringbuf.buf)) {                      \
            _ringbuf.tail = 0;                                                \
        }                                                                     \
    }
