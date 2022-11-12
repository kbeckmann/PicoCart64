// #include <stdio.h>
// //
// #include "f_util.h"
// #include "ff.h"
// #include "pico/stdlib.h"
// #include "rtc.h"
// //
// #include "hw_config.h"

// #include "psram_inline.h"

// char buf[1024 / 2 / 2 / 2 / 2];

// void write_random_file(const char *filename, int32_t size)
// {
// 	// See FatFs - Generic FAT Filesystem Module, "Application Interface",
// 	// http://elm-chan.org/fsw/ff/00index_e.html
// 	sd_card_t *pSD = sd_get_by_num(0);
// 	FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
// 	if (FR_OK != fr) {
// 		panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
// 	}

// 	FIL fil;

// 	printf("\n\n---- write /%s -----\n", filename);

// 	fr = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
// 	if (FR_OK != fr && FR_EXIST != fr) {
// 		panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
// 	}

// 	uint32_t *buf32 = (uint32_t *) buf;
// 	int len = 0;
// 	int total = 0;
// 	uint64_t t0 = to_us_since_boot(get_absolute_time());
// 	uint32_t ctr = 0;
// 	do {
// 		for (int i = 0; i < sizeof(buf) / 4; i++) {
// 			// buf32[i] = pc64_rand32();
// 			buf32[i] = ctr++;
// 		}
// 		fr = f_write(&fil, buf, sizeof(buf), &len);
// 		total += len;
// 		size -= len;
// 	} while (size > 0);
// 	uint64_t t1 = to_us_since_boot(get_absolute_time());
// 	uint32_t delta = (t1 - t0) / 1000;
// 	uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

// 	printf("Wrote %d bytes in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

// 	fr = f_close(&fil);
// 	if (FR_OK != fr) {
// 		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
// 	}
// 	printf("---- write file done -----\n\n\n");
// }

// void load_rom(const char *filename)
// {
// 	// Set output enable (OE) to normal mode on all QSPI IO pins except SS
// 	qspi_enable();

// 	// See FatFs - Generic FAT Filesystem Module, "Application Interface",
// 	// http://elm-chan.org/fsw/ff/00index_e.html
// 	sd_card_t *pSD = sd_get_by_num(0);
// 	FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
// 	if (FR_OK != fr) {
// 		panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
// 	}

// 	FIL fil;

// 	printf("\n\n---- read /%s -----\n", filename);

// 	fr = f_open(&fil, filename, FA_OPEN_EXISTING | FA_READ);
// 	if (FR_OK != fr && FR_EXIST != fr) {
// 		panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
// 	}

// 	FILINFO filinfo;
// 	fr = f_stat(filename, &filinfo);
// 	printf("%s [size=%llu]\n", filinfo.fname, filinfo.fsize);

// 	int len = 0;
// 	int total = 0;
// 	uint64_t t0 = to_us_since_boot(get_absolute_time());
// 	do {
// 		fr = f_read(&fil, buf, sizeof(buf), &len);
// 		qspi_write(total, buf, len);
// 		// qspi_read(total, buf, len);
// 		total += len;

// 		// uint32_t *buf32 = (uint32_t *) buf;
// 		// printf("read: %d, len: %d, buf[0]: %08X\n\n", fr, len, buf32[0]);

// 		// static iter;
// 		// iter++;
// 		// if (iter > 16) {
// 		//  break;
// 		// }
// 	} while (len > 0);
// 	uint64_t t1 = to_us_since_boot(get_absolute_time());
// 	uint32_t delta = (t1 - t0) / 1000;
// 	uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

// 	printf("Read %d bytes and programmed PSRAM in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

// 	fr = f_close(&fil);
// 	if (FR_OK != fr) {
// 		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
// 	}
// 	printf("---- read file done -----\n\n\n");

// 	printf("---- Verify file with PSRAM -----\n\n\n");

// 	qspi_enter_cmd_xip();

// 	fr = f_open(&fil, filename, FA_OPEN_EXISTING | FA_READ);
// 	if (FR_OK != fr && FR_EXIST != fr) {
// 		panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
// 	}

// 	fr = f_stat(filename, &filinfo);
// 	printf("%s [size=%llu]\n", filinfo.fname, filinfo.fsize);

// 	// {
// 	//  uint32_t *ptr = (uint32_t *) 0x10000000;

// 	//  for (int i = 0; i < 8 * 1024 / 4 * 2; i++) {
// 	//      psram_set_cs(1);
// 	//      uint32_t word = ptr[i];
// 	//      psram_set_cs(0);
// 	//      // printf("%08X ", word);
// 	//      if (word != i) {
// 	//          printf("ERR: %08X != %08X\n", word, i);
// 	//      }
// 	//  }

// 	//  printf("----- halt ----\n");

// 	//  while (1) {

// 	//  }
// 	// }

// 	len = 0;
// 	total = 0;
// 	t0 = to_us_since_boot(get_absolute_time());
// 	do {
// 		fr = f_read(&fil, buf, sizeof(buf), &len);

// 		volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
// 		uint32_t *buf32 = (uint32_t *) buf;
// 		for (int i = 0; i < len / 4; i++) {
// 			uint32_t address_32 = total / 4 + i;
// 			uint32_t address = address_32 * 4;
// 			psram_set_cs(psram_addr_to_chip(address));
// 			uint32_t word = ptr[address_32];
// 			uint32_t facit = buf32[i];
// 			psram_set_cs(0);
// 			if (word != facit) {
// 				printf("diff @%08X + %d: %08X %08X\n", total, i * 4, word, facit);
// 			}
// 		}
// 		total += len;
// 		// printf("@%08X [len=%d]\n", total, len);

// 		// if (total > 128) {
// 		//  printf("Halting\n");
// 		//  while (1) {
// 		//  }
// 		// }
// 	} while (len > 0);
// 	t1 = to_us_since_boot(get_absolute_time());
// 	delta = (t1 - t0) / 1000;
// 	kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

// 	printf("Verified %d bytes with PSRAM in %d ms (%d kB/s)\n\n\n", total, delta, kBps);

// 	//////////////////////////////
// 	t0 = to_us_since_boot(get_absolute_time());
// 	len = 64;
// 	uint32_t totalRead = 0;
// 	do {
// 		volatile uint32_t *ptr = (volatile uint32_t *)0x10000000;
// 		for (int i = 0; i < len / 4; i++) {
// 			uint32_t address_32 = totalRead / 4 + i;
// 			uint32_t address = address_32 * 4;
// 			psram_set_cs(psram_addr_to_chip(address));
// 			uint32_t word = ptr[address_32];
// 			// uint32_t facit = buf32[i];
// 			psram_set_cs(0);
// 		}
// 		totalRead += len;
// 	} while (totalRead <= total);
// 	t1 = to_us_since_boot(get_absolute_time());
// 	delta = (t1 - t0) / 1000;
// 	kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));

// 	printf("Reread %d bytes from PSRAM in %d ms (%d kB/s)\n\n\n", totalRead, delta, kBps);
// 	//////////////////////////////

// 	fr = f_close(&fil);
// 	if (FR_OK != fr) {
// 		printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
// 	}

// 	printf("---- Verify file with PSRAM Done -----\n\n\n");

// 	f_unmount(pSD->pcName);

// 	// Release the qspi control
// 	qspi_disable();
// }
