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
#include "shell.h"

// picocart64_shared
#include "pc64_regs.h"
#include "pc64_rand.h"
#include "n64_defs.h"
#include "pc64_utils.h"

static uint8_t pc64_sd_wait() {
    uint32_t timeout = 0;
    uint32_t read_buf[] = { 1 };
	uint32_t busy[] = { 1 };
    
    // Wait until the cartridge interface is ready
    do {
        // returns 1 while sd card is busy
        //pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_BUSY, sizeof(uint32_t))
		data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
        pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_BUSY, sizeof(uint32_t));
        
        // Took too long, abort
        if((timeout++) > 10000000) {
			printf("SD_WAIT timed out. read_buf: %ld\n", read_buf[0]);
			return -1;
		}
    }
    while(memcmp(busy, read_buf, sizeof(uint32_t)) == 0);
    (void) timeout; // Needed to stop unused variable warning

    // Success
    return 0;
}

// void test_sector_count_send(uint32_t sectorCount) {
// 	//pc_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_READ_NUM_SECTORS, 1);
// 	uint32_t buf2[] = { sectorCount };
// 	data_cache_hit_writeback_invalidate(buf2, sizeof(buf2));
// 	pi_write_raw(buf2, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_NUM_SECTORS, sizeof(buf2));

// 	//uint32_t buf[] = { value };
// 	// data_cache_hit_writeback_invalidate(buf, sizeof(buf));
// 	// pi_write_raw(buf, base, offset, sizeof(buf));
// }

uint32_t SECTOR_READ_SIZE = 32;
void test_tx_buffer_read(uint8_t *buff, uint64_t sector, uint32_t dmaReadSize) {

	uint64_t current_sector = sector;
	uint16_t part0[] = { (current_sector & 0xFFFF000000000000LL) >> 48 };
	uint16_t part1[] = { (current_sector & 0x0000FFFF00000000LL) >> 32 };
	uint16_t part2[] = { (current_sector & 0x00000000FFFF0000LL) >> 16 };
	uint16_t part3[] = { current_sector &  0x000000000000FFFFLL }; 
	
	// send sector
	data_cache_hit_writeback_invalidate(part0, sizeof(part0));
	pi_write_raw(part0, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR0, sizeof(part0));
	
	data_cache_hit_writeback_invalidate(part1, sizeof(part1));
	pi_write_raw(part1, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR1, sizeof(part1));
	
	data_cache_hit_writeback_invalidate(part2, sizeof(part2));
	pi_write_raw(part2, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR2, sizeof(part2));
	
	data_cache_hit_writeback_invalidate(part3, sizeof(part3));
	pi_write_raw(part3, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR3, sizeof(part3));

	uint32_t buf2[] = { 1 };
	data_cache_hit_writeback_invalidate(buf2, sizeof(buf2));
	pi_write_raw(buf2, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_NUM_SECTORS, sizeof(buf2));

	uint32_t buf3[] = { 1 };
	data_cache_hit_writeback_invalidate(buf3, sizeof(buf3));
	pi_write_raw(buf3, PC64_CIBASE_ADDRESS_START, PC64_COMMAND_SD_READ, sizeof(buf3));

	// wait for the sd card to finish
	if(pc64_sd_wait() == 0) {
		printf("\nDMA size of(%ld)\n", dmaReadSize);
		data_cache_hit_writeback_invalidate(buff, dmaReadSize/2);
		pi_read_raw(buff, PC64_BASE_ADDRESS_START, 0, dmaReadSize/2);
		
		// // dump the contents
		for (int k = 0; k < dmaReadSize; k++) {
			printf("%x ", buff[k]);
		}

		printf("\nRead with dma?\n");
		data_cache_hit_writeback_invalidate(buff, dmaReadSize);
		dma_read_raw_async(buff, PC64_BASE_ADDRESS_START, dmaReadSize);
		// dump the contents
		for (int k = 0; k < dmaReadSize; k++) {
			printf("%x ", buff[k]);
		}

		// data_cache_hit_writeback_invalidate(buff, dmaReadSize/2);
    	// dma_read_raw_async(buff, PC64_BASE_ADDRESS_START, dmaReadSize/2);
    	// dma_wait();

		// for (int k = 0; k < dmaReadSize; k++) {
		// 	printf("%x ", buff[k]);
		// }

		printf("\nread of sector %lld finished\n", current_sector);
	} else {
		printf("\nwait timeout\n");
	}
}

void test_tx_buffer_read16(uint16_t *buff, uint64_t sector, uint32_t dmaReadSize) {

	uint64_t current_sector = sector;
	uint16_t part0[] = { (current_sector & 0xFFFF000000000000LL) >> 48 };
	uint16_t part1[] = { (current_sector & 0x0000FFFF00000000LL) >> 32 };
	uint16_t part2[] = { (current_sector & 0x00000000FFFF0000LL) >> 16 };
	uint16_t part3[] = { current_sector &  0x000000000000FFFFLL }; 
	
	// send sector
	data_cache_hit_writeback_invalidate(part0, sizeof(part0));
	pi_write_raw(part0, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR0, sizeof(part0));
	
	data_cache_hit_writeback_invalidate(part1, sizeof(part1));
	pi_write_raw(part1, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR1, sizeof(part1));
	
	data_cache_hit_writeback_invalidate(part2, sizeof(part2));
	pi_write_raw(part2, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR2, sizeof(part2));
	
	data_cache_hit_writeback_invalidate(part3, sizeof(part3));
	pi_write_raw(part3, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_SECTOR3, sizeof(part3));

	uint32_t buf2[] = { 1 };
	data_cache_hit_writeback_invalidate(buf2, sizeof(buf2));
	pi_write_raw(buf2, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_READ_NUM_SECTORS, sizeof(buf2));

	uint32_t buf3[] = { 1 };
	data_cache_hit_writeback_invalidate(buf3, sizeof(buf3));
	pi_write_raw(buf3, PC64_CIBASE_ADDRESS_START, PC64_COMMAND_SD_READ, sizeof(buf3));

	// wait for the sd card to finish
	if(pc64_sd_wait() == 0) {
		printf("\nDMA size of(%ld) buff16\n", dmaReadSize);
		data_cache_hit_writeback_invalidate(buff, dmaReadSize);
		pi_read_raw(buff, PC64_BASE_ADDRESS_START, 0, dmaReadSize);

		for (int k = 0; k < dmaReadSize; k++) {
			if (k % 28 == 0 && k != 0) {
				printf("\n");
			}
			printf("%x ", buff[k]);
		}
		printf("\n");

		// printf("\nRead with dma\n");
		// data_cache_hit_writeback_invalidate(buff, dmaReadSize * 4);
		// dma_read_raw_async(buff, PC64_BASE_ADDRESS_START, dmaReadSize*4);
		// // dump the contents
		// for (int k = 0; k < dmaReadSize; k++) {
		// 	printf("%x ", buff[k]);
		// }
	} else {
		printf("\nwait timeout\n");
	}
}

int main(void)
{
	uint32_t *facit_buf32 = (uint32_t *) facit_buf;
	uint32_t *read_buf32 = (uint32_t *) read_buf;
	uint16_t *read_buf16 = (uint16_t *) read_buf;

	configure_sram();

	display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
	console_init();
	console_set_render_mode(RENDER_AUTOMATIC);
	controller_init();
	// debug_init_isviewer();

	printf("PicoCart64 Test ROM (git rev %08X)\n\n", GIT_REV);

	///////////////////////////////////////////////////////////////////////////

	// Verify PicoCart64 Magic
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf32, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_MAGIC, sizeof(uint32_t));
	if (read_buf32[0] == PC64_MAGIC) {
		printf("[ OK ] MAGIC = 0x%08lX.\n", read_buf32[0]);
	} else {
		printf("[FAIL] MAGIC = 0x%08lX.\n", read_buf32[0]);
	}

	///////////////////////////////////////////////////////////////////////////

	// Print Hello from the N64
	strcpy(write_buf, "Hello from the N64!\r\n");
	pc64_uart_write((const uint8_t *)write_buf, strlen(write_buf));
	printf("[ -- ] Wrote hello over UART.\n");

	///////////////////////////////////////////////////////////////////////////

	// Initialize a random sequence
	pc64_rand_seed(0);
	data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
	for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
		facit_buf32[i] = pc64_rand32();
	}

	// Read back 1Mbit of SRAM
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf, CART_DOM2_ADDR2_START, 0, sizeof(read_buf));

	// Compare SRAM with the facit
	if (memcmp(facit_buf, read_buf, sizeof(read_buf)) != 0) {
		printf("[FAIL] SRAM was not backed up properly.\n");

		for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
			if (facit_buf32[i] != read_buf[i]) {
				printf("       Error @%d: facit %08lX != sram %08lX\n", i, facit_buf32[i], read_buf32[i]);
				break;
			}
		}
	} else {
		printf("[ OK ] SRAM was backed up properly.\n");
	}

	///////////////////////////////////////////////////////////////////////////

	// Write 1Mbit of SRAM
	data_cache_hit_writeback_invalidate(facit_buf, sizeof(facit_buf));
	pi_write_raw(facit_buf, CART_DOM2_ADDR2_START, 0, sizeof(facit_buf));

	printf("[ -- ] Wrote test pattern to SRAM.\n");

	// Read it back and verify
	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	pi_read_raw(read_buf, CART_DOM2_ADDR2_START, 0, sizeof(read_buf));

	// Compare SRAM with the facit
	if (memcmp(facit_buf, read_buf, sizeof(read_buf)) != 0) {
		printf("[FAIL] SRAM (volatile) did not verify correctly.\n");

		for (int i = 0; i < sizeof(facit_buf) / sizeof(uint32_t); i++) {
			if (facit_buf32[i] != read_buf[i]) {
				printf("       Error @%d: facit %08lX != sram %08lX\n", i, facit_buf32[i], read_buf32[i]);
				break;
			}
		}
	} else {
		printf("[ OK ] (volatile) SRAM verified correctly.\n");
	}

	///////////////////////////////////////////////////////////////////////////

	// Stress test: Read pseudo-random numbers from PicoCart64 RAND address space
	// Reset random seed
	pi_write_u32(0, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_RAND_SEED);
	pc64_rand_seed(0);

	// Compare buffer with RNG
	// printf("[ -- ] RNG Test: ");
	// bool rng_ok = true;
	// for (int j = 0; j < 64 && rng_ok; j++) {
	// 	// Read back 1Mbit of RAND values
	// 	data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
	// 	pi_read_raw(read_buf, PC64_RAND_ADDRESS_START, 0, sizeof(read_buf));
	// 	printf(".");
	// 	fflush(stdout);

	// 	for (int i = 0; i < sizeof(read_buf) / sizeof(uint16_t); i++) {
	// 		uint16_t value = pc64_rand16();

	// 		if (value != read_buf16[i]) {
	// 			printf("\n       Error @%d: ours %04X != theirs %04X", i, value, read_buf16[i]);
	// 			rng_ok = false;
	// 			break;
	// 		}
	// 	}
	// }

	// if (rng_ok) {
	// 	printf("\n[ OK ] Random stress test verified correctly.\n");
	// } else {
	// 	printf("\n[FAIL] Random stress test failed.\n");
	// }

	///////////////////////////////////////////////////////////////////////////

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
		printf("       PicoCart64 might stall now and require a power cycle.\n");
	}

	console_clear();	
    
	// uint32_t w_buf[] = { 1 };
	// pc_pi_write_raw(w_buf, PC64_BASE_ADDRESS_START, PC64_COMMAND_SD_READ, sizeof(buf2));
	printf("Test SD Read Buffer\n");	
	// for(uint64_t i = 0; i < 16; i++) {

	uint64_t sectorToRead = 0;
	// uint8_t __attribute__((aligned(8))) buff[1024];
	uint16_t __attribute__((aligned(8))) buff16[256];

	// Try to populate the buffer with sd card data
	// c = 0x00;
	// test_tx_buffer_read(buff, sectorToRead, 32);
	// for(int i = 0; i < 1024; i++) {
	// 	buff[i] = c;
	// 	if (i % 8 == 1) {
	// 		c++;
	// 	}
	// }

	// while (true) {
    //     controller_scan();
    //     struct controller_data keys = get_keys_pressed();
    //     if (keys.c[0].start) {
    //         printf("Press START for next test\n");
    //         break;
    //     }
    // }
	// console_clear();

	for(int i = 0; i < 100; i++) {
		if (i == 0) {
			printf("S");
		} else if (i > 0 &&  i < 511) {
			printf("%d", i % 10);
		} else {
			printf("E");
		}
		buff16[i] = i;
	}
	printf("\n");

	while (true) {
        controller_scan();
        struct controller_data keys = get_keys_pressed();
        if (keys.c[0].start) {
            printf("Press START for 1st test\n");
			console_clear();
            break;
        }
    }

	test_tx_buffer_read16(buff16, sectorToRead, 64);
	while (true) {
        controller_scan();
        struct controller_data keys = get_keys_pressed();
        if (keys.c[0].start) {
            printf("Press START for next test\n");
            break;
        }
    }
	console_clear();

	test_tx_buffer_read16(buff16, sectorToRead, 128);
	while (true) {
        controller_scan();
        struct controller_data keys = get_keys_pressed();
        if (keys.c[0].start) {
            printf("Press START for next test\n");
            break;
        }
    }
	console_clear();

	
	test_tx_buffer_read16(buff16, sectorToRead, 256);
	while (true) {
        controller_scan();
        struct controller_data keys = get_keys_pressed();
        if (keys.c[0].start) {
            printf("Press START for next test\n");
            break;
        }
    }
	console_clear();

	// test_tx_buffer_read16(buff16, sectorToRead, 64);
	// while (true) {
    //     controller_scan();
    //     struct controller_data keys = get_keys_pressed();
    //     if (keys.c[0].start) {
    //         printf("Press START for next test\n");
    //         break;
    //     }
    // }
	// console_clear();
	

	// c = 0x00;
	// test_tx_buffer_read(buff, sectorToRead, 64);
	// for(int i = 0; i < 1024; i++) {
	// 	buff[i] = c;
	// 	if (i % 8 == 1) {
	// 		c++;
	// 	}
	// }

	// while (true) {
    //     controller_scan();
    //     struct controller_data keys = get_keys_pressed();
    //     if (keys.c[0].start) {
    //         printf("Press START for next test\n");
    //         break;
    //     }
    // }
	// console_clear();

	// c = 0x00;
	// test_tx_buffer_read(buff, sectorToRead, 128);
	// for(int i = 0; i < 1024; i++) {
	// 	buff[i] = c;
	// 	if (i % 8 == 1) {
	// 		c++;
	// 	}
	// }

	// while (true) {
    //     controller_scan();
    //     struct controller_data keys = get_keys_pressed();
    //     if (keys.c[0].start) {
    //         printf("Press START for next test\n");
    //         break;
    //     }
    // }
	// console_clear();
	
	// test_tx_buffer_read(buff, sectorToRead, 256);
	// for(int i = 0; i < 1024; i++) {
	// 	buff[i] = (char)i;
	// }

	// while (true) {
    //     controller_scan();
    //     struct controller_data keys = get_keys_pressed();
    //     if (keys.c[0].start) {
    //         printf("Press START for next test\n");
    //         break;
    //     }
    // }
	// console_clear();

	// test_tx_buffer_read(buff, sectorToRead, 512);
	// for(int i = 0; i < 1024; i++) {
	// 	buff[i] = (char)i;
	// }

	while (true) {
        controller_scan();
        struct controller_data keys = get_keys_pressed();
        if (keys.c[0].start) {
            printf("Press START to finsh test\n");
            break;
        }
    }
	
	console_render();
    
    /* Start the shell if the user presses start */
    printf("\n\nPress START to continue to the shell...\n");
    
    
    start_shell();
}
