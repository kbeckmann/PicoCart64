// Copyright (c) 2023 Konrad Beckmann
// SPDX-License-Identifier: MIT
// Based on https://github.com/DragonMinded/libdragon/blob/trunk/examples/vtest/vtest.c

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

/* hardware definitions */
// Pad buttons
#define A_BUTTON(a)     ((a) & 0x8000)
#define B_BUTTON(a)     ((a) & 0x4000)
#define Z_BUTTON(a)     ((a) & 0x2000)
#define START_BUTTON(a) ((a) & 0x1000)

// D-Pad
#define DU_BUTTON(a)    ((a) & 0x0800)
#define DD_BUTTON(a)    ((a) & 0x0400)
#define DL_BUTTON(a)    ((a) & 0x0200)
#define DR_BUTTON(a)    ((a) & 0x0100)

// Triggers
#define TL_BUTTON(a)    ((a) & 0x0020)
#define TR_BUTTON(a)    ((a) & 0x0010)

// Yellow C buttons
#define CU_BUTTON(a)    ((a) & 0x0008)
#define CD_BUTTON(a)    ((a) & 0x0004)
#define CL_BUTTON(a)    ((a) & 0x0002)
#define CR_BUTTON(a)    ((a) & 0x0001)

#define PAD_DEADZONE     5
#define PAD_ACCELERATION 10
#define PAD_CHECK_TIME   40

unsigned short gButtons = 0;
struct controller_data gKeys;

volatile int gTicks;			/* incremented every vblank */

/* input - do getButtons() first, then getAnalogX() and/or getAnalogY() */
unsigned short getButtons(int pad)
{
	// Read current controller status
	controller_scan();
	gKeys = get_keys_pressed();
	return (unsigned short)(gKeys.c[0].data >> 16);
}

unsigned char getAnalogX(int pad)
{
	return (unsigned char)gKeys.c[pad].x;
}

unsigned char getAnalogY(int pad)
{
	return (unsigned char)gKeys.c[pad].y;
}

display_context_t lockVideo(int wait)
{
	display_context_t dc;

	if (wait)
		while (!(dc = display_lock())) ;
	else
		dc = display_lock();
	return dc;
}

void unlockVideo(display_context_t dc)
{
	if (dc)
		display_show(dc);
}

/* text functions */
void drawText(display_context_t dc, char *msg, int x, int y)
{
	if (dc)
		graphics_draw_text(dc, x, y, msg);
}

void printText(display_context_t dc, char *msg, int x, int y)
{
	if (dc)
		graphics_draw_text(dc, x * 8, y * 8, msg);
}

#define REGISTER_BASE       0xA4400000

/**
 * @brief Update the framebuffer pointer in the VI
 *
 * @param[in] dram_val
 *            The new framebuffer to use for display.  Should be aligned and uncached.
 */
static void __write_dram_register(void const *const dram_val)
{
	uint32_t *reg_base = (uint32_t *) REGISTER_BASE;

	reg_base[1] = (uint32_t) dram_val;
	MEMORY_BARRIER();
}

/* vblank callback */
void vblCallback(void)
{
	gTicks++;
	// volatile void *fpga_source = (void *) (0x10000000 + 32 * 1024 * 1024);
	// __write_dram_register(fpga_source);
}

void delay(int cnt)
{
	int then = gTicks + cnt;
	while (then > gTicks) ;
}

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef volatile uint8_t vu8;
typedef volatile uint16_t vu16;
typedef volatile uint32_t vu32;
typedef volatile uint64_t vu64;

typedef volatile int8_t vs8;
typedef volatile int16_t vs16;
typedef volatile int32_t vs32;
typedef volatile int64_t vs64;

typedef float f32;
typedef double f64;

/* initialize console hardware */
void init_n64(void)
{
	/* enable interrupts (on the CPU) */
	// init_interrupts();

	/* Initialize peripherals */
	display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
	set_VI_interrupt(1, 590);

	register_VI_handler(vblCallback);

	controller_init();

	// PI registers
#define PI_BASE_REG   0x04600000
#define PI_STATUS_REG (PI_BASE_REG+0x10)
#define	PI_STATUS_ERROR		0x04
#define	PI_STATUS_IO_BUSY	0x02
#define	PI_STATUS_DMA_BUSY	0x01

#define PI_BSD_DOM1_LAT_REG	(PI_BASE_REG+0x14)
#define PI_BSD_DOM1_PWD_REG	(PI_BASE_REG+0x18)
#define PI_BSD_DOM1_PGS_REG	(PI_BASE_REG+0x1C)
#define PI_BSD_DOM1_RLS_REG	(PI_BASE_REG+0x20)
#define PI_BSD_DOM2_LAT_REG	(PI_BASE_REG+0x24)
#define PI_BSD_DOM2_PWD_REG	(PI_BASE_REG+0x28)
#define PI_BSD_DOM2_PGS_REG	(PI_BASE_REG+0x2C)
#define PI_BSD_DOM2_RLS_REG	(PI_BASE_REG+0x30)

	// MIPS addresses
#define KSEG0 0x80000000
#define KSEG1 0xA0000000

	// Memory translation stuff
#define	PHYS_TO_K1(x)       ((u32)(x)|KSEG1)
#define	IO_WRITE(addr,data) (*(vu32 *)PHYS_TO_K1(addr)=(u32)(data))
#define	IO_READ(addr)       (*(vu32 *)PHYS_TO_K1(addr))

	// Initialize the PI
	IO_WRITE(PI_STATUS_REG, 3);

#if 0
	// Slowish
	IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x05);
	IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x0C);
	IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x0D);
	IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x02);
#elif 0
	// SPEEED
	IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x05);
	IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x03);
	IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x0F);
	IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x02);
#else
	// Fast SRAM access (should match SDK values)
	IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
	IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x0C);
	IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x07);
	IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x03);
#endif
}

/**
 * @brief Grab the texture buffer given a display context
 *
 * @param[in] x
 *            The display context returned from #display_lock
 *
 * @return A pointer to the drawing surface for that display context.
 */
#define __get_buffer( x ) __safe_buffer[(x)-1]
extern void *__safe_buffer[3];

/**
 * @brief Align a memory address to 64 byte offset
 * 
 * @param[in] x
 *            Unaligned memory address
 *
 * @return An aligned address guaranteed to be >= the unaligned address
 */
#define ALIGN_64BYTE(x)     ((void *)(((uint32_t)(x)+63) & 0xFFFFFFC0))

/**
 * @brief Register definition for the PI interface
 * @ingroup lowlevel
 */
typedef struct PI_regs_s {
	/** @brief Uncached address in RAM where data should be found */
	volatile void *ram_address;
	/** @brief Address of data on peripheral */
	uint32_t pi_address;
	/** @brief How much data to read from RAM into the peripheral */
	uint32_t read_length;
	/** @brief How much data to write to RAM from the peripheral */
	uint32_t write_length;
	/** @brief Status of the PI, including DMA busy */
	uint32_t status;
} PI_regs_t;

static volatile struct PI_regs_s *const PI_regs = (struct PI_regs_s *)0xa4600000;

/**
 * @brief Read from a peripheral
 *
 * This function should be used when reading from the cartridge.
 *
 * @param[out] ram_address
 *             Pointer to a buffer to place read data
 * @param[in]  pi_address
 *             Memory address of the peripheral to read from
 * @param[in]  len
 *             Length in bytes to read into ram_address
 */
void dma_read_any(void *ram_address, unsigned long read_address, unsigned long len)
{
	assert(len > 0);

	disable_interrupts();

	while (dma_busy()) ;
	MEMORY_BARRIER();
	PI_regs->ram_address = ram_address;
	MEMORY_BARRIER();
	PI_regs->pi_address = read_address;
	MEMORY_BARRIER();
	PI_regs->write_length = len - 1;
	MEMORY_BARRIER();
	while (dma_busy()) ;

	enable_interrupts();
}

/* main code entry point */
int main(void)
{
	display_context_t _dc;
	char temp[128];
	int res = 0;
	unsigned short buttons, previous = 0;

	uint32_t *buf_alloc = malloc(320 * 240 * 2);
	uint32_t *buf = ALIGN_64BYTE(buf_alloc);

	init_n64();

	int frames = 0;
	while (1) {
		frames++;

		_dc = lockVideo(1);

#define CACHED_ADDR(x)    ((void *)(((uint32_t)(x)) & (~0xA0000000)))

		// uint32_t *dest = (uint32_t *) CACHED_ADDR(__get_buffer(_dc));
		void *dest = (void *)_dc->buffer;

		uint32_t t0 = TICKS_READ();

		// SRAM streaming
		volatile void *sram_source = (void *)(0x08000000);

#if 0
		dma_read_any(dest, sram_source, 320 * 240 * 2);
#else
		for (int y = 0; y < 240; y++) {
			uint32_t offset_write = y * 320 * 2;
			uint32_t offset_read = y * 320 * 2;
			dma_read_any(dest + offset_write, sram_source + offset_read, 320 * 2);
		}
#endif

		uint32_t t1 = TICKS_READ();

		// color = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF);

		uint32_t t_delta = t1 - t0;
		uint32_t t_delta_ms = t_delta / (TICKS_PER_SECOND / 1000);

		sprintf(temp, "delta=%d (%d ms)", t_delta, t_delta_ms);
		printText(_dc, temp, 5, 3);

		unlockVideo(_dc);

		buttons = getButtons(0);

		previous = buttons;
	}

	return 0;
}
