/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann <konrad.beckmann@gmail.com>
 * Copyright (c) 2022 Christopher Bonhage <me@christopherbonhage.com>
 *
 * Based on https://github.com/meeq/SaveTest-N64
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
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

void getAndPrintSDCardContents() {
    // send magic address + cmd so picocart will know we want to execute a command
    // read data from the buffer space using the "virtual" control "pins"
    // data can be written by picocart at certain addresses to say when something is dnoe
    // basically emulating gpio pins?
    /*
    rom/n64 -> hey I want `magic_address` + cmd, 
            ???block until specific register/memory address is set?
    pico -> okay, I can do that
    pico -> encodes requested data and sends it back to n64/rom code requesting it
    rom/n64 -> decode requested data and print on screen

    From the looks of it, you have to implement this interface
    typedef struct
{
	DSTATUS (*disk_initialize)(void);
	DSTATUS (*disk_status)(void);
	DRESULT (*disk_read)(BYTE* buff, LBA_t sector, UINT count);
	DRESULT (*disk_write)(const BYTE* buff, LBA_t sector, UINT count);
	DRESULT (*disk_ioctl)(BYTE cmd, void* buff);
} fat_disk_t;

This is implementation for 64drive is found in debug_sdfd_64drive.c
... we can probably implement the cart side of these functions like we were the 64drive and just use those definitions?

for the hardware we are using...
This also seems to imply (because libdragon has fat support built in) that the sd card on pico only needs to send raw data back/forth
Means setting usb_cart variable in libdragon/usb.c to a new one to specify pico
    */
   

   /*
    Can probably use the CI (cartridge interface) detailed here http://64drive.retroactive.be/64drive_hardware_spec.pdf as a starting point
    need:
    memory base (used outside "normal" rom memory so pico will interpret it as a special command area)
    general memory buffer (512 bytes | 4096 bits)
        - for sd card:
        status (16 bits)
        command (32 bites)
        lba (logical block address?) (32 bits)
        length (32 bits)
        result (32 bits)
   */
  /* Values copied from the linked 64drive pdf
    BASE = 0x18000000 
    BASE+0x0000 buffer
    BASE+0x0200 status, 0x1 busy, 0x0 idle
    BASE+0x0208 command
    BASE+0x0210 lba
    BASE+0x0218 length
    BASE+0x0220 result
  */
}

/*
TODO
Calculate number of entries that can fit on the screen
Make the more files above/below indicator a sprite or something besides '...'
Figure out why it feels laggy? The example code feels the same way, not sure it runs at full speed in the emulator
*/
#define SCREEN_WIDTH 512
int NUM_ENTRIES = 21;
const int ROW_HEIGHT = 12;
const int MARGIN_PADDING = 20;
const int ROW_SELECTION_WIDTH = SCREEN_WIDTH;

color_t MENU_BAR_COLOR = { .r = 0x82, .g = 0x00, .b = 0x2E, .a = 0x00 }; // 0x82002E
color_t SELECTION_COLOR = { .r = 0x00, .g = 0x67, .b = 0xC7, .a = 0x00 }; // 0x0067C7
const int MENU_BAR_HEIGHT = 15;

/*
 * Render a list of strings and show a caret on the currently selected row
 */
void render_list(display_context_t display, char *list[], int currently_selected, int first_visible, int max_on_screen) {

    /* If we aren't starting at the first row, draw an indicator to show more files above*/
    if (first_visible > 0) {
        graphics_draw_text(display, MARGIN_PADDING, MENU_BAR_HEIGHT, "...");
    }

    for (int i = 0; i < max_on_screen; i++) {
        int row = first_visible+i;
        int x = currently_selected == row ? MARGIN_PADDING : 0; // if the current item is selected move it over so we can show a selection caret
        int y = i * ROW_HEIGHT + (MENU_BAR_HEIGHT + ROW_HEIGHT); // First row must start below the menu bar, so add that plus a pad
        
        // if we run out of items to draw on the screen, don't draw anything else
        if (row > NUM_ENTRIES) {
            break;
        }

        if (currently_selected == row) {
            /* Render selection box */
            graphics_draw_box(display, 0, y-2, ROW_SELECTION_WIDTH, 10, graphics_convert_color(SELECTION_COLOR));

            /* Now render selection caret */
            graphics_draw_text(display, MARGIN_PADDING/2, y, "  >");

            /* Increase Margin Padding to account for added caret width */
            x += MARGIN_PADDING;
        } else {
            x += MARGIN_PADDING;
        }

        // Render the list item
        graphics_draw_text(display, x, y, list[row]);

        /* If this is the last row and there are more files below, draw an indicator*/
        if (row+1 < NUM_ENTRIES && i + 1 >= max_on_screen) {
            graphics_draw_text(display, MARGIN_PADDING, y + 10, "...");
        }
    }
}

void draw_header_bar(display_context_t display, int fileCount) {
    int x = 0, y = 0, width = SCREEN_WIDTH, height = MENU_BAR_HEIGHT;
    graphics_draw_box(display, x, y, width, height, graphics_convert_color(MENU_BAR_COLOR));
    
    char menuHeaderBuffer[50];
    sprintf(menuHeaderBuffer, "PicoCart64 OS b2022.07.17\t\t\t\t%d Files\0", fileCount);
    graphics_draw_text(display, MARGIN_PADDING, y+4, menuHeaderBuffer);
}

/*
 * Init display and controller input then render a list of strings, showing currently selected
 */
void show_list() {
    
    display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    controller_init();

    char *entries[21] = {
        "Goldeneye 007 (USA).n64", 
        "Mario 64 (USA).n64", 
        "Star Wars Rouge Squadron (USA).n64", 
        "Star Wars Shadows of the Empire (USA).n64", 
        "The Legend of Zelda, Ocarina of Time (USA).n64", 
        "Perfect Dark (USA).n64", 
        "Mario Kart 64 (USA).n64", 
        "Killer Instinct 64 (USA).n64", 
        "Star Wars Podracer (USA).n64", 
        "007 Tomorrow Never Dies (USA).n64",
        "Donkey Kong 64 (USA).n64",
        "testrom.n64",
        /*"お母さん３(JPN).n64",*/ // I don't think it likes this entry too much
        "Harvest Moon 64 (JPN).n64",
        "Crusin' USA (USA).n64",
        "Metriod 64 (USA).n64",
        "Super Secret Menu Rom.n64",
        "Winback (USA).n64",
        "Resident Evil 2 (USA).n64",
        "Best Game You Have Never Hear Of (USA).n64",
        "testrom2.n64",
        "Final Fight 64 (JPN).n64",
         };

    int currently_selected = 0;
    int first_visible = 0;
    int max_on_screen = 10; // ?? No idea what this number should be, if we wrap strings then this would be computed when first_visible changes
    while(1) {
        static display_context_t display = 0;

        /* Grab a render buffer */
		while (!(display = display_lock()))
			;

		/* Clear the screen */
		graphics_fill_screen(display, 0);

        /* Draw top header bar */
        draw_header_bar(display, 21);

        /* Render the list of file */
        render_list(display, entries, currently_selected, first_visible, max_on_screen);

        controller_scan();
        struct controller_data keys = get_keys_down();
        int mag = 0;
        if (keys.c[0].down) {
            mag = 1;
		} else if (keys.c[0].up) {
            mag = -1;
        }

        //(sizeof(entries)/sizeof(entries[0]))
        if ((mag > 0 && currently_selected+mag < NUM_ENTRIES) || (mag < 0 && currently_selected > 0)) {
			currently_selected += mag;
        }

        // If we have moved the cursor to an entry not yet visible on screen, move first_visible as well
        if ((mag > 0 && currently_selected >= (first_visible + max_on_screen)) || (mag < 0 && currently_selected < first_visible && currently_selected >= 0)) {
            first_visible += mag;
        }

        char debugTextBuffer[50];
        sprintf(debugTextBuffer, "currently_selected=%d, first_visible=%d\0", currently_selected, first_visible);
        graphics_draw_text(display, 5, 230, debugTextBuffer);

        /* Force the backbuffer flip */
        display_show(display);
    }
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
    // uint32_t *facit_buf32 = (uint32_t *) facit_buf;
    // uint32_t *read_buf32 = (uint32_t *) read_buf;

    // display_init(
    //     RESOLUTION_320x240,
    //     DEPTH_32_BPP,
    //     2,
    //     GAMMA_NONE,
    //     ANTIALIAS_RESAMPLE
    // );
    // console_init();
    // // debug_init_isviewer();

    // printf("PicoCart64 Test ROM (git rev %08lX)\n\n", GIT_REV);

    // // Initialize a random sequence
    // rand32_reset();
    // data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
    // for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
    //     facit_buf32[i] = rand32();
    // }

    // // Read back 1Mbit of SRAM
    // data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    // pi_read_raw(read_buf, CART_DOM2_ADDR2_START, 0, sizeof(read_buf));

    // // Compare SRAM with the facit
    // if (memcmp(facit_buf, read_buf, sizeof(read_buf)) != 0) {
    //     printf("[FAIL] SRAM was not backed up properly.\n");

    //     for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
    //         if (facit_buf32[i] != read_buf[i]) {
    //             printf("       Error @%d: facit %08lX != sram %08lX\n",
    //                    i, facit_buf32[i], read_buf32[i]);
    //             break;
    //         }
    //     }
    // } else {
    //     printf("[ OK ] SRAM was backed up properly.\n");
    // }

    // // Verify PicoCart64 Magic
    // data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    // pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_MAGIC, sizeof(uint32_t));
    // if (read_buf32[0] == PC64_MAGIC) {
    //     printf("[ OK ] MAGIC = 0x%08lX.\n", read_buf32[0]);
    // } else {
    //     printf("[FAIL] MAGIC = 0x%08lX.\n", read_buf32[0]);
    // }

    // // Print Hello from the N64
    // strcpy(write_buf, "Hello from the N64!\r\n");
    // pc64_uart_write((const uint8_t *) write_buf, strlen(write_buf));
    // printf("[ -- ] Wrote hello over UART.\n");

    // // Write 1Mbit of SRAM
    // data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
    // pi_write_raw(facit_buf, CART_DOM2_ADDR2_START, 0, sizeof(facit_buf));

    // printf("[ -- ] Wrote test pattern to SRAM.\n");

    // // Read 1Mbit of 64DD IPL ROM
    // data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    // pi_read_raw(read_buf, CART_DOM1_ADDR1_START, 0, sizeof(read_buf));
    // printf("[ -- ] Read from the N64DD address space.\n");

    // // Verify the PicoCart64 Magic *again*
    // // This is done to ensure that the PicoCart64 is still alive and well,
    // // and hasn't crashed or locked itself up.
    // data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    // pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_MAGIC, sizeof(uint32_t));
    // if (read_buf32[0] == PC64_MAGIC) {
    //     printf("[ OK ] (second time) MAGIC = 0x%08lX.\n", read_buf32[0]);
    // } else {
    //     printf("[FAIL] (second time) MAGIC = 0x%08lX.\n", read_buf32[0]);
    // }

    // console_render();

    show_list();
}
