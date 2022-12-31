//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#ifndef _ROM_DEFS_H
#define	_ROM_DEFS_H

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned long
#define u64 unsigned long long

#define vu8 volatile unsigned char
#define vu16 volatile unsigned short
#define vu32 volatile unsigned long
#define vu64 volatile unsigned long long

#define s8 signed char
#define s16 short
#define s32 long
#define s64 long long

typedef volatile uint64_t sim_vu64;
typedef volatile uint64_t sim_vu64;
typedef unsigned int sim_u32;
typedef uint64_t sim_u64;

/*
 *
0000h              (1 byte): initial PI_BSB_DOM1_LAT_REG value (0x80)
0001h              (1 byte): initial PI_BSB_DOM1_PGS_REG value (0x37)
0002h              (1 byte): initial PI_BSB_DOM1_PWD_REG value (0x12)
0003h              (1 byte): initial PI_BSB_DOM1_PGS_REG value (0x40)
0004h - 0007h     (1 dword): ClockRate
0008h - 000Bh     (1 dword): Program Counter (PC)
000Ch - 000Fh     (1 dword): Release
0010h - 0013h     (1 dword): CRC1
0014h - 0017h     (1 dword): CRC2
0018h - 001Fh    (2 dwords): Unknown (0x0000000000000000)
0020h - 0033h    (20 bytes): Image name
                             Padded with 0x00 or spaces (0x20)
0034h - 0037h     (1 dword): Unknown (0x00000000)
0038h - 003Bh     (1 dword): Manufacturer ID
                             0x0000004E = Nintendo ('N')
003Ch - 003Dh      (1 word): Cartridge ID
003Eh - 003Fh      (1 word): Country code
                             0x4400 = Germany ('D')
                             0x4500 = USA ('E')
                             0x4A00 = Japan ('J')
                             0x5000 = Europe ('P')
                             0x5500 = Australia ('U')
0040h - 0FFFh (1008 dwords): Boot code
*/

#define DP_BASE_REG		0x04100000
#define VI_BASE_REG		0x04400000
#define PI_BASE_REG		0x04600000
#define PIF_RAM_START		0x1FC007C0


/*
 * PI status register has 3 bits active when read from (PI_STATUS_REG - read)
 *	Bit 0: DMA busy - set when DMA is in progress
 *	Bit 1: IO busy  - set when IO is in progress
 *	Bit 2: Error    - set when CPU issues IO request while DMA is busy
 */

#define PI_STATUS_REG		(PI_BASE_REG+0x10)

/* PI DRAM address (R/W): starting RDRAM address */
#define PI_DRAM_ADDR_REG	(PI_BASE_REG+0x00)	/* DRAM address */

/* PI pbus (cartridge) address (R/W): starting AD16 address */
#define PI_CART_ADDR_REG	(PI_BASE_REG+0x04)

/* PI read length (R/W): read data length */
#define PI_RD_LEN_REG		(PI_BASE_REG+0x08)

/* PI write length (R/W): write data length */
#define PI_WR_LEN_REG		(PI_BASE_REG+0x0C)

/*
 * PI status (R): [0] DMA busy, [1] IO busy, [2], error
 *           (W): [0] reset controller (and abort current op), [1] clear intr
 */

#define PI_BSD_DOM1_LAT_REG	(PI_BASE_REG+0x14)

/* PI dom1 pulse width (R/W): [7:0] domain 1 device R/W strobe pulse width */
#define PI_BSD_DOM1_PWD_REG	(PI_BASE_REG+0x18)

/* PI dom1 page size (R/W): [3:0] domain 1 device page size */
#define PI_BSD_DOM1_PGS_REG	(PI_BASE_REG+0x1C)    /*   page size */

/* PI dom1 release (R/W): [1:0] domain 1 device R/W release duration */
#define PI_BSD_DOM1_RLS_REG	(PI_BASE_REG+0x20)
/* PI dom2 latency (R/W): [7:0] domain 2 device latency */
#define PI_BSD_DOM2_LAT_REG	(PI_BASE_REG+0x24)    /* Domain 2 latency */

/* PI dom2 pulse width (R/W): [7:0] domain 2 device R/W strobe pulse width */
#define PI_BSD_DOM2_PWD_REG	(PI_BASE_REG+0x28)    /*   pulse width */

/* PI dom2 page size (R/W): [3:0] domain 2 device page size */
#define PI_BSD_DOM2_PGS_REG	(PI_BASE_REG+0x2C)    /*   page size */

/* PI dom2 release (R/W): [1:0] domain 2 device R/W release duration */
#define PI_BSD_DOM2_RLS_REG	(PI_BASE_REG+0x30)    /*   release duration */


#define	PI_DOMAIN1_REG		PI_BSD_DOM1_LAT_REG
#define	PI_DOMAIN2_REG		PI_BSD_DOM2_LAT_REG


#define	PI_STATUS_ERROR		0x04
#define	PI_STATUS_IO_BUSY	0x02
#define	PI_STATUS_DMA_BUSY	0x01

#define                      DPC_START                    (DP_BASE_REG + 0x00)
#define                      DPC_END                      (DP_BASE_REG + 0x04)
#define                      DPC_CURRENT                  (DP_BASE_REG + 0x08)
#define                      DPC_STATUS                   (DP_BASE_REG + 0x0C)
#define                      DPC_CLOCK                    (DP_BASE_REG + 0x10)
#define                      DPC_BUFBUSY                  (DP_BASE_REG + 0x14)
#define                      DPC_PIPEBUSY                 (DP_BASE_REG + 0x18)
#define                      DPC_TMEM                     (DP_BASE_REG + 0x1C)

#define	VI_CONTROL	(VI_BASE_REG + 0x00)
#define	VI_FRAMEBUFFER	(VI_BASE_REG + 0x04)
#define	VI_WIDTH	(VI_BASE_REG + 0x08)
#define	VI_V_INT	(VI_BASE_REG + 0x0C)
#define	VI_CUR_LINE	(VI_BASE_REG + 0x10)
#define	VI_TIMING	(VI_BASE_REG + 0x14)
#define	VI_V_SYNC	(VI_BASE_REG + 0x18)
#define	VI_H_SYNC	(VI_BASE_REG + 0x1C)
#define	VI_H_SYNC2	(VI_BASE_REG + 0x20)
#define	VI_H_LIMITS	(VI_BASE_REG + 0x24)
#define	VI_COLOR_BURST	(VI_BASE_REG + 0x28)
#define	VI_H_SCALE	(VI_BASE_REG + 0x2C)
#define	VI_VSCALE	(VI_BASE_REG + 0x30)

#define	PHYS_TO_K0(x)	((u32)(x)|0x80000000)	/* physical to kseg0 */
#define	K0_TO_PHYS(x)	((u32)(x)&0x1FFFFFFF)	/* kseg0 to physical */
#define	PHYS_TO_K1(x)	((u32)(x)|0xA0000000)	/* physical to kseg1 */
#define	K1_TO_PHYS(x)	((u32)(x)&0x1FFFFFFF)	/* kseg1 to physical */

#define	IO_READ(addr)		(*(volatile u32*)PHYS_TO_K1(addr))
#define	IO_WRITE(addr,data)	(*(volatile u32*)PHYS_TO_K1(addr)=(u32)(data))

#define SAVE_SIZE_SRAM 		32768
#define SAVE_SIZE_SRAM96 	131072 //TODO: or should this be 98304
#define SAVE_SIZE_EEP4k 	512
#define SAVE_SIZE_EEP16k 	2048
#define SAVE_SIZE_FLASH 	131072

#define ROM_ADDR 		0xb0000000


#define FRAM_EXECUTE_CMD		0xD2000000
#define FRAM_STATUS_MODE_CMD	0xE1000000
#define FRAM_ERASE_OFFSET_CMD	0x4B000000
#define FRAM_WRITE_OFFSET_CMD	0xA5000000
#define FRAM_ERASE_MODE_CMD		0x78000000
#define FRAM_WRITE_MODE_CMD		0xB4000000
#define FRAM_READ_MODE_CMD		0xF0000000

#define FRAM_STATUS_REG	0xA8000000
#define FRAM_COMMAND_REG 0xA8010000



//void romFill(...);
void pif_boot();

int is_valid_rom(unsigned char *buffer);
void swap_header(unsigned char* header, int loadlength);

u8 getCicType(u8 bios_cic);

void simulate_boot(u32 cic_chip);

#endif
