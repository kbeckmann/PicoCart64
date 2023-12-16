#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "ringbuf.h"  // Include the header file for the ring buffer

RINGBUF_CREATE(ringbuf, 64, uint8_t);  // Use the RINGBUF_CREATE macro to initialize the ring buffer

// int debug = 1;
int debug = 0;

// Generic hexdump function that prints the contents of a buffer with 8 bytes wide and prints the index in the beginning
static void hexdump(uint8_t *buf, size_t len) {
    for (int i = 0; i < len; i++) {
        if (i % 8 == 0) {
            printf("%04X: ", i);
        }
        printf("%c ", buf[i]);
        if (i % 8 == 7) {
            printf("\n");
        }
    }
    printf("\n");
}

// Debug function that prints the contents of the ring buffer and head and tail
static void ringbuf_dump(void) {
    if (debug) {
        printf("head: %d\n", ringbuf.head);
        printf("tail: %d\n", ringbuf.tail);
        hexdump(ringbuf.buf, sizeof(ringbuf.buf));
    }
}

void reset_ringbuf(void) {
    // memset ringbuf with 'x'
    memset(ringbuf.buf, 'x', sizeof(ringbuf.buf));
    ringbuf.head = 0;
    ringbuf.tail = 0;
}

static void test_ringbuf_basic_functionality() {
    uint8_t data[10];  // Create a data buffer that's smaller than the ring buffer
    for (int i = 0; i < sizeof(data); i++) {
        data[i] = 'a' + i;
    }

    // Add the data to the ring buffer
    ringbuf_add_buf(ringbuf, data, sizeof(data));
    ringbuf_dump();

    // Check that the data was correctly copied into the ring buffer
    for (int i = 0; i < sizeof(data); i++) {
        assert(ringbuf.buf[i] == 'a' + i);
    }

    // Check that the head and tail are at the correct positions
    assert(ringbuf.head == sizeof(data));
}


static void test_ringbuf_empty() {
    // Check that the ring buffer is initially empty
    assert(ringbuf.head == ringbuf.tail);
}

static void test_ringbuf_full() {
    uint8_t data[64];  // Create a data buffer that's the same size as the ring buffer
    memset(data, 'a', sizeof(data));

    // Fill the ring buffer
    ringbuf_add_buf(ringbuf, data, sizeof(data));

    // Verify the contents of the ring buffer
    for (int i = 0; i < ARRAY_SIZE(ringbuf.buf); i++) {
        assert(ringbuf.buf[i] == 'a');
    }
    ringbuf_dump();

    // Check that the ring buffer is now full. The head should be at the same position as the tail.
    assert(ringbuf.head == 0);
    assert(ringbuf.tail == 0);
}

static void test_ringbuf_overwrite() {
    uint8_t data[64];  // Create a data buffer that's the same size as the ring buffer
    memset(data, 'a', sizeof(data));

    // Fill the ring buffer with 'a'
    ringbuf_add_buf(ringbuf, data, sizeof(data));
    ringbuf_dump();

    // Overwrite the first 48 bytes with 'b'
    memset(data, 'b', 48);
    ringbuf_add_buf(ringbuf, data, 48);
    ringbuf_dump();

    // Overwrite the next 48 bytes with 'c'
    memset(data, 'c', 48);
    ringbuf_add_buf(ringbuf, data, 48);
    ringbuf_dump();

    // Verify the contents of the ring buffer
    for (int i = 0; i < 32; i++) {
        assert(ringbuf.buf[i] == 'c');
    }
    for (int i = 32; i < 48; i++) {
        assert(ringbuf.buf[i] == 'b');
    }

    // Check that the head and tail are at the correct positions
    assert(ringbuf.head == 32);
}

void test_ringbuf_write_to_end() {
    uint8_t data[63];  // Create a data buffer that's one less than the size of the ring buffer
    memset(data, 'a', sizeof(data));

    // Write data up to the end of the ring buffer
    ringbuf_add_buf(ringbuf, data, sizeof(data));
    ringbuf_dump();

    // Verify the contents of the ring buffer
    for (int i = 0; i < sizeof(data); i++) {
        assert(ringbuf.buf[i] == 'a');
    }

    // Check that the head and tail are at the correct positions
    assert(ringbuf.head == sizeof(data));
}

void test_ringbuf_write_around_end() {
    uint8_t data[65];  // Create a data buffer that's one more than the size of the ring buffer
    memset(data, 'b', sizeof(data));

    // Write data around the end of the ring buffer
    ringbuf_add_buf(ringbuf, data, sizeof(data));
    ringbuf_dump();

    // Verify the contents of the ring buffer
    for (int i = 0; i < ARRAY_SIZE(ringbuf.buf); i++) {
        assert(ringbuf.buf[i] == 'b');
    }

    // Check that the head and tail are at the correct positions
    assert(ringbuf.head == 1);
}

int main() {
    reset_ringbuf();
    printf("test_ringbuf_empty\n");
    test_ringbuf_empty();

    reset_ringbuf();
    printf("test_ringbuf_full\n");
    test_ringbuf_full();

    reset_ringbuf();
    printf("test_ringbuf_basic_functionality\n");
    test_ringbuf_basic_functionality();

    reset_ringbuf();
    printf("test_ringbuf_overwrite\n");
    test_ringbuf_overwrite();

    reset_ringbuf();
    printf("test_ringbuf_write_to_end\n");
    test_ringbuf_write_to_end();

    reset_ringbuf();
    printf("test_ringbuf_write_around_end\n");
    test_ringbuf_write_around_end();
    
    return 0;
}