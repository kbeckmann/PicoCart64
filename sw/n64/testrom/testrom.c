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

    printf("PicoCart64 Test ROM\n\n");

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
        printf("[OK]   SRAM was backed up properly.\n");
    }

    // Write 1Mbit of SRAM
    data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
    pi_write_raw(facit_buf, CART_DOM2_ADDR2_START, 0, sizeof(facit_buf));

    printf("[OK]   Wrote test pattern to SRAM.\n");

    console_render();
}
