/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann <konrad.beckmann@gmail.com>
 * Copyright (c) 2022 Christopher Bonhage <me@christopherbonhage.com>
 *
 * Based on https://github.com/meeq/SaveTest-N64
 */

#include <string.h>
#include <stdint.h>
#include <libdragon.h>

#include "git_info.h"

typedef struct PI_regs_s {
    volatile void * ram_address;
    uint32_t pi_address;
    uint32_t read_length;
    uint32_t write_length;
} PI_regs_t;
static volatile PI_regs_t * const PI_regs = (PI_regs_t *) 0xA4600000;

// Source https://n64brew.dev/wiki/Peripheral_Interface#Mapped_Domains

// N64DD control registers
#define CART_DOM2_ADDR1_START     0x05000000
#define CART_DOM2_ADDR1_END       0x05FFFFFF

// N64DD IPL ROM
#define CART_DOM1_ADDR1_START     0x06000000
#define CART_DOM1_ADDR1_END       0x07FFFFFF

// Cartridge SRAM
#define CART_DOM2_ADDR2_START     0x08000000
#define CART_DOM2_ADDR2_END       0x0FFFFFFF

// Cartridge ROM
#define CART_DOM1_ADDR2_START     0x10000000
#define CART_DOM1_ADDR2_END       0x1FBFFFFF

// Unknown
#define CART_DOM1_ADDR3_START     0x1FD00000
#define CART_DOM1_ADDR3_END       0x7FFFFFFF

// PicoCart64 Address Space
#define PC64_BASE_ADDRESS_START   0xB0000000
#define PC64_BASE_ADDRESS_END     (PC64_BASE_ADDRESS_START + 0xFFF)

#define PC64_CIBASE_ADDRESS_START 0xB8000000
#define PC64_CIBASE_ADDRESS_END   0xB8000FFF

#define PC64_REGISTER_MAGIC       0x00000000

// Write length to print from TX buffer
#define PC64_REGISTER_UART      0x00000004

#define PC64_MAGIC              0xDEAD6400


// SRAM constants
#define SRAM_256KBIT_SIZE         0x00008000
#define SRAM_768KBIT_SIZE         0x00018000
#define SRAM_1MBIT_SIZE           0x00020000
#define SRAM_BANK_SIZE            SRAM_256KBIT_SIZE
#define SRAM_256KBIT_BANKS        1
#define SRAM_768KBIT_BANKS        3
#define SRAM_1MBIT_BANKS          4


static void verify_memory_range(uint32_t base, uint32_t offset, uint32_t len)
{
    uint32_t start = base | offset;
    uint32_t end = start + len - 1;

    switch (base) {
    case CART_DOM2_ADDR1_START:
        assert(start >= CART_DOM2_ADDR1_START);
        assert(start <= CART_DOM2_ADDR1_END);
        assert(end   >= CART_DOM2_ADDR1_START);
        assert(end   <= CART_DOM2_ADDR1_END);
        break;

    case CART_DOM1_ADDR1_START:
        assert(start >= CART_DOM1_ADDR1_START);
        assert(start <= CART_DOM1_ADDR1_END);
        assert(end   >= CART_DOM1_ADDR1_START);
        assert(end   <= CART_DOM1_ADDR1_END);
        break;

    case CART_DOM2_ADDR2_START:
        assert(start >= CART_DOM2_ADDR2_START);
        assert(start <= CART_DOM2_ADDR2_END);
        assert(end   >= CART_DOM2_ADDR2_START);
        assert(end   <= CART_DOM2_ADDR2_END);
        break;

    case CART_DOM1_ADDR2_START:
        assert(start >= CART_DOM1_ADDR2_START);
        assert(start <= CART_DOM1_ADDR2_END);
        assert(end   >= CART_DOM1_ADDR2_START);
        assert(end   <= CART_DOM1_ADDR2_END);
        break;

    case CART_DOM1_ADDR3_START:
        assert(start >= CART_DOM1_ADDR3_START);
        assert(start <= CART_DOM1_ADDR3_END);
        assert(end   >= CART_DOM1_ADDR3_START);
        assert(end   <= CART_DOM1_ADDR3_END);
        break;

    case PC64_BASE_ADDRESS_START:
        assert(start >= PC64_BASE_ADDRESS_START);
        assert(start <= PC64_BASE_ADDRESS_END);
        assert(end   >= PC64_BASE_ADDRESS_START);
        assert(end   <= PC64_BASE_ADDRESS_END);
        break;

    case PC64_CIBASE_ADDRESS_START:
        assert(start >= PC64_CIBASE_ADDRESS_START);
        assert(start <= PC64_CIBASE_ADDRESS_END);
        assert(end   >= PC64_CIBASE_ADDRESS_START);
        assert(end   <= PC64_CIBASE_ADDRESS_END);
        break;

    default:
        assert(!"Unsupported base");
    }
}

static void pi_read_raw(void * dest, uint32_t base, uint32_t offset, uint32_t len)
{
    assert(dest != NULL);
    verify_memory_range(base, offset, len);

    disable_interrupts();
    dma_wait();

    MEMORY_BARRIER();
    PI_regs->ram_address = UncachedAddr(dest);
    MEMORY_BARRIER();
    PI_regs->pi_address = offset | base;
    MEMORY_BARRIER();
    PI_regs->write_length = len - 1;
    MEMORY_BARRIER();

    enable_interrupts();
    dma_wait();
}


static void pi_write_raw(const void * src, uint32_t base, uint32_t offset, uint32_t len)
{
    assert(src != NULL);
    verify_memory_range(base, offset, len);

    disable_interrupts();
    dma_wait();

    MEMORY_BARRIER();
    PI_regs->ram_address = UncachedAddr(src);
    MEMORY_BARRIER();
    PI_regs->pi_address = offset | base;
    MEMORY_BARRIER();
    PI_regs->read_length = len - 1;
    MEMORY_BARRIER();

    enable_interrupts();
    dma_wait();
}

static void pi_write_u32(const uint32_t value, uint32_t base, uint32_t offset)
{
    uint32_t buf[] = {value};

    data_cache_hit_writeback_invalidate(buf, sizeof(buf));
    pi_write_raw(buf, base, offset, sizeof(buf));
}


static unsigned int seed;

static uint32_t rand32(void)
{
    seed = (seed * 1103515245U + 12345U) & 0x7fffffffU;
    return (int)seed;
}

static void rand32_reset(void)
{
    seed = 1;
}



static uint8_t __attribute__ ((aligned(16))) facit_buf[SRAM_1MBIT_SIZE];
static uint8_t __attribute__ ((aligned(16))) read_buf[SRAM_1MBIT_SIZE];
static char    __attribute__ ((aligned(16))) write_buf[0x1000];


static void pc64_uart_write(const uint8_t *buf, uint32_t len)
{
    // 16-bit aligned
    assert((((uint32_t) buf) & 0x1) == 0);

    uint32_t len_aligned32 = (len + 3) & (-4);

    data_cache_hit_writeback_invalidate((uint8_t *) buf, len_aligned32);
    pi_write_raw(write_buf, PC64_BASE_ADDRESS_START, 0, len_aligned32);

    pi_write_u32(len, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_UART);
}

int main(void)
{
    uint32_t *facit_buf32 = (uint32_t *) facit_buf;
    uint32_t *read_buf32 = (uint32_t *) read_buf;

    display_init(
        RESOLUTION_320x240,
        DEPTH_32_BPP,
        2,
        GAMMA_NONE,
        ANTIALIAS_RESAMPLE
    );
    console_init();
    // debug_init_isviewer();

    printf("PicoCart64 Test ROM (git rev %08lX)\n\n", GIT_REV);

    // Initialize a random sequence
    rand32_reset();
    data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
    for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
        facit_buf32[i] = rand32();
    }

    // Read back 1Mbit of SRAM
    data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    pi_read_raw(read_buf, CART_DOM2_ADDR2_START, 0, sizeof(read_buf));

    // Compare SRAM with the facit
    if (memcmp(facit_buf, read_buf, sizeof(read_buf)) != 0) {
        printf("[FAIL] SRAM was not backed up properly.\n");

        for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
            if (facit_buf32[i] != read_buf[i]) {
                printf("       Error @%d: facit %08lX != sram %08lX\n",
                       i, facit_buf32[i], read_buf32[i]);
                break;
            }
        }
    } else {
        printf("[ OK ] SRAM was backed up properly.\n");
    }

    // Verify PicoCart64 Magic
    data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_MAGIC, sizeof(uint32_t));
    if (read_buf32[0] == PC64_MAGIC) {
        printf("[ OK ] MAGIC = 0x%08lX.\n", read_buf32[0]);
    } else {
        printf("[FAIL] MAGIC = 0x%08lX.\n", read_buf32[0]);
    }

    // Print Hello from the N64
    strcpy(write_buf, "Hello from the N64!\r\n");
    pc64_uart_write((const uint8_t *) write_buf, strlen(write_buf));
    printf("[ -- ] Wrote hello over UART.\n");

    // Write 1Mbit of SRAM
    data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
    pi_write_raw(facit_buf, CART_DOM2_ADDR2_START, 0, sizeof(facit_buf));

    printf("[ -- ] Wrote test pattern to SRAM.\n");

    // Read 1Mbit of 64DD IPL ROM
    data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    pi_read_raw(read_buf, CART_DOM1_ADDR1_START, 0, sizeof(read_buf));
    printf("[ -- ] Read from the N64DD address space.\n");

    // Verify the PicoCart64 Magic *again*
    // This is done to ensure that the PicoCart64 is still alive and well,
    // and hasn't crashed or locked itself up.
    data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_MAGIC, sizeof(uint32_t));
    if (read_buf32[0] == PC64_MAGIC) {
        printf("[ OK ] (second time) MAGIC = 0x%08lX.\n", read_buf32[0]);
    } else {
        printf("[FAIL] (second time) MAGIC = 0x%08lX.\n", read_buf32[0]);
    }

    console_render();
}
