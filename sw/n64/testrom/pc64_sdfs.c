// /*

// This code needs to live in libdragon and will also need to modify the debug.c file to include definitions for picocart64
// to use functions in here


// */
// /**
//  * SPDX-License-Identifier: BSD-2-Clause
//  *
//  * Copyright (c) 2022 Kaili Hill
//  * 
//  */

// #include <libdragon.h>
// #include "pc64_sdfs.h"

// // PicoCart64 Address space

// // [READ/WRITE]: Scratch memory used for various functions
// #ifndef PC64_BASE_ADDRESS_START
// #define PC64_BASE_ADDRESS_START     0x81000000
// #define PC64_BASE_ADDRESS_LENGTH    0x00001000
// #define PC64_BASE_ADDRESS_END       (PC64_BASE_ADDRESS_START + PC64_BASE_ADDRESS_LENGTH - 1)

// // [READ]: Returns pseudo-random values.
// //         Address does not matter.
// //         Each returned 16-bit word generates a new random value.
// //         PC64_REGISTER_RESET_RAND resets the random seed.
// #define PC64_RAND_ADDRESS_START     0x82000000
// #define PC64_RAND_ADDRESS_LENGTH    0x01000000
// #define PC64_RAND_ADDRESS_END       (PC64_RAND_ADDRESS_START + PC64_RAND_ADDRESS_LENGTH - 1)

// // [READ/WRITE]: Command address space. See register definitions below for details.
// #define PC64_CIBASE_ADDRESS_START   0x83000000
// #define PC64_CIBASE_ADDRESS_LENGTH  0x00001000
// #define PC64_CIBASE_ADDRESS_END     (PC64_CIBASE_ADDRESS_START + PC64_CIBASE_ADDRESS_LENGTH - 1)

// // [Read]: Returns PC64_MAGIC
// #define PC64_REGISTER_MAGIC         0x00000000
// #define PC64_MAGIC                  0xDEAD6400

// // [WRITE]: Write number of bytes to print from TX buffer
// #define PC64_REGISTER_UART_TX       0x00000004

// // [WRITE]: Set the random seed to a 32-bit value
// #define PC64_REGISTER_RAND_SEED     0x00000008

// /* *** SD CARD *** */
// // [READ]: Signals pico to start data read from SD Card
// #define PC64_COMMAND_SD_READ       0x00000012

// // [READ]: Load selected rom into memory and boot, 
// #define PC64_COMMAND_SD_ROM_SELECT 0x00000013

// // [READ] 1 while sd card is busy, 0 once the CI is free
// #define PC64_REGISTER_SD_BUSY PC64_COMMAND_SD_ROM_SELECT + 0x1

// // [WRITE] Sector to read from SD Card, 8 bytes
// #define PC64_REGISTER_SD_READ_SECTOR PC64_REGISTER_SD_BUSY + 0x1

// // [WRITE] number of sectors to read from the sd card, 8 bytes
// #define PC64_REGISTER_SD_READ_NUM_SECTORS PC64_REGISTER_SD_READ_SECTOR + 0x8

// // [WRITE] write the selected file name that should be loaded into memory
// // 255 bytes
// #define PC64_REGISTER_SD_SELECT_ROM PC64_REGISTER_SD_READ_NUM_SECTORS + 0x8
// #endif

// /* // Should be able to just use the write_io command, hopefully won't need these special functions
// // typedef struct PI_regs_s {
// // 	volatile void *ram_address;
// // 	uint32_t pi_address;
// // 	uint32_t read_length;
// // 	uint32_t write_length;
// // } PI_regs_t;
// // static volatile PI_regs_t *const PI_regs = (PI_regs_t *) 0xA4600000;

// // void pi_read_raw(void *dest, uint32_t base, uint32_t offset, uint32_t len)
// // {
// // 	assert(dest != NULL);
// // 	verify_memory_range(base, offset, len);

// // 	disable_interrupts();
// // 	dma_wait();

// // 	MEMORY_BARRIER();
// // 	PI_regs->ram_address = UncachedAddr(dest);
// // 	MEMORY_BARRIER();
// // 	PI_regs->pi_address = offset | base;
// // 	MEMORY_BARRIER();
// // 	PI_regs->write_length = len - 1;
// // 	MEMORY_BARRIER();

// // 	enable_interrupts();
// // 	dma_wait();
// // }

// // void pi_write_raw(const void *src, uint32_t base, uint32_t offset, uint32_t len)
// // {
// // 	assert(src != NULL);
// // 	verify_memory_range(base, offset, len);

// // 	disable_interrupts();
// // 	dma_wait();

// // 	MEMORY_BARRIER();
// // 	PI_regs->ram_address = UncachedAddr(src);
// // 	MEMORY_BARRIER();
// // 	PI_regs->pi_address = offset | base;
// // 	MEMORY_BARRIER();
// // 	PI_regs->read_length = len - 1;
// // 	MEMORY_BARRIER();

// // 	enable_interrupts();
// // 	dma_wait();
// // }
// */

// uint8_t pc64_sd_wait() {
//     u32 timeout = 0;
//     uint32_t *read_buf32 = (uint32_t *) read_buf;
    
//     // Wait until the cartridge interface is ready
//     do {
//         // returns 1 while sd card is busy
//         pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_BUSY, sizeof(uint32_t))
        
//         // Took too long, abort
//         if((timeout++) > 10000)
//             return -1;
//     }
//     while(read_buf[0]);
//     (void) timeout; // Needed to stop unused variable warning
    
//     // Success
//     return 0;
// }

// DRESULT fat_disk_read_pc64(BYTE* buff, LBA_t sector, UINT count) {
// 	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
// 	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

// 	for (int i=0;i<count;i++) {
// 		// pc64_sd_wait();
// 		io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_READ_SECTOR, sector+i);
//         io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_READ_NUM_SECTORS, count);
// 		// pc64_sd_wait();
// 		io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_READ, PC64_COMMAND_SD_READ);

//         // wait for the sd card to finish
//         pc64_sd_wait();

//         // TODO add in error handling and associated registers
// 		// if (pc64_sd_wait() != 0)
// 		// {
// 		// 	debugf("[debug] fat_disk_read_64drive: wait timeout\n");
// 		// 	// Operation is taking too long. Probably SD was not inserted.
// 		// 	// Send a COMMAND_ABORT and SD_RESET, and return I/O error.
// 		// 	// Note that because of a 64drive firmware bug, this is not
// 		// 	// sufficient to unblock the 64drive. The USB channel will stay
// 		// 	// unresponsive. We don't currently have a workaround for this.
// 		// 	io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_ABORT);
// 		// 	usb_64drive_wait();
// 		// 	io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_SD_RESET);
// 		// 	usb_64drive_wait();
// 		// 	return FR_DISK_ERR;
// 		// }

// 		data_cache_hit_writeback_invalidate(buff, 512);
// 		dma_read(buff, PC64_BASE_ADDRESS_START, 512);
// 		buff += 512;
// 	}
// 	return RES_OK;
// }

// // static DRESULT fat_disk_write_64drive(const BYTE* buff, LBA_t sector, UINT count)
// // {
// // 	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
// // 	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");

// // 	for (int i=0;i<count;i++)
// // 	{
// // 		if (((uint32_t)buff & 7) == 0)
// // 		{
// // 			data_cache_hit_writeback(buff, 512);
// // 			dma_write(buff, D64_CIBASE_ADDRESS + D64_BUFFER, 512);
// // 		}
// // 		else
// // 		{
// // 			typedef uint32_t u_uint32_t __attribute__((aligned(1)));

// // 			uint32_t* dst = (uint32_t*)(D64_CIBASE_ADDRESS + D64_BUFFER);
// // 			u_uint32_t* src = (u_uint32_t*)buff;
// // 			for (int i = 0; i < 512/16; i++)
// // 			{
// // 				uint32_t a = *src++; uint32_t b = *src++; uint32_t c = *src++; uint32_t d = *src++;
// // 				*dst++ = a;          *dst++ = b;          *dst++ = c;          *dst++ = d;
// // 			}
// // 		}

// // 		usb_64drive_wait();
// // 		io_write(D64_CIBASE_ADDRESS + D64_REGISTER_LBA, sector+i);
// // 		usb_64drive_wait();
// // 		io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_SD_WRITE);
// // 		if (usb_64drive_wait() != 0)
// // 		{
// // 			debugf("[debug] fat_disk_write_64drive: wait timeout\n");
// // 			// Operation is taking too long. Probably SD was not inserted.
// // 			// Send a COMMAND_ABORT and SD_RESET, and return I/O error.
// // 			// Note that because of a 64drive firmware bug, this is not
// // 			// sufficient to unblock the 64drive. The USB channel will stay
// // 			// unresponsive. We don't currently have a workaround for this.
// // 			io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_ABORT);
// // 			usb_64drive_wait();
// // 			io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, D64_COMMAND_SD_RESET);
// // 			usb_64drive_wait();
// // 			return FR_DISK_ERR;
// // 		}

// // 		buff += 512;
// // 	}

// // 	return RES_OK;
// // }

// static DSTATUS fat_disk_initialize_pc64(void) { return 0; }