#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>
#include <n64sys.h>

// #include "utils.h"

#include "rom_defs.h"
// #include "sram.h"

// #define STBI_HEADER_FILE_ONLY
// #include "stb_image.c"

// extern short int gCheats;               /* 0 = off, 1 = select, 2 = all */
// extern short int force_tv;
// extern short int boot_country;

static u8 __attribute__((aligned(16))) dmaBuf[128*1024];
static volatile struct _PI_regs_s * const _PI_regs = (struct _PI_regs_s *)0xa4600000;

int is_valid_rom(unsigned char *buffer) {
    /* Test if rom is a native .z64 image with header 0x80371240. [ABCD] */
    if((buffer[0]==0x80)&&(buffer[1]==0x37)&&(buffer[2]==0x12)&&(buffer[3]==0x40))
        return 0;
    /* Test if rom is a byteswapped .v64 image with header 0x37804012. [BADC] */
    else if((buffer[0]==0x37)&&(buffer[1]==0x80)&&(buffer[2]==0x40)&&(buffer[3]==0x12))
        return 1;
    /* Test if rom is a wordswapped .n64 image with header  0x40123780. [DCBA] */
    else if((buffer[0]==0x40)&&(buffer[1]==0x12)&&(buffer[2]==0x37)&&(buffer[3]==0x80))
        return 2;
    else
        return 0;
}

// void swap_header(unsigned char* header, int loadlength) {
//     unsigned char temp;
//     int i;

//     /* Btyeswap if .v64 image. */
//     if( header[0]==0x37) {
//         for (i = 0; i < loadlength; i+=2) {
//             temp= header[i];
//             header[i]= header[i+1];
//             header[i+1]=temp;
//             }
//         }
//     /* Wordswap if .n64 image. */
//     else if( header[0]==0x40) {
//         for (i = 0; i < loadlength; i+=4) {
//             temp= header[i];
//             header[i]= header[i+3];
//             header[i+3]=temp;
//             temp= header[i+1];
//             header[i+1]= header[i+2];
//             header[i+2]=temp;
//         }
//     }
// }

// u8 getCicType(u8 bios_cic) {
//     u8 cic_buff[2048];
//     volatile u8 cic_chip;
//     volatile u32 val;
//     if (bios_cic) {
//         evd_setCfgBit(ED_CFG_SDRAM_ON, 0);
//         sleep(10);
//         val = *(u32 *) 0xB0000170;
//         dma_read_s(cic_buff, 0xB0000040, 1024);
//         cic_chip = get_cic(cic_buff);
//         evd_setCfgBit(ED_CFG_SDRAM_ON, 1);
//         sleep(10);
//     }
//     else {
//         val = *(u32 *) 0xB0000170;
//         dma_read_s(cic_buff, 0xB0000040, 1024);
//         cic_chip = get_cic(cic_buff);
//     }

//     return cic_chip;
// }



// int get_cic(unsigned char *buffer) {
//     unsigned int crc;
//     // figure out the CIC
//     crc = CRC_Calculate(0, buffer, 1000);
//     switch(crc) {
//     case 0x303faac9:
//     case 0xf0da3d50:
//         return 1;
//     case 0xf3106101:
//         return 2;
//     case 0xe7cd9d51:
//         return 3;
//     case 0x7ae65c9:
//         return 5;
//     case 0x86015f8f:
//         return 6;
//     }

//     return 2;
// }

// int get_cic_save(char *cartid, int *cic, int *save) {
//     // variables
//     int NUM_CARTS = 137;
//     int i;

//     //data arrays
//     /*
//     char *names[] = {
//         "gnuboy64lite", "FRAMTestRom", "SRAMTestRom", "Worms Armageddon",
//         "Super Smash Bros.", "Banjo-Tooie", "Blast Corps", "Bomberman Hero",
//         "Body Harvest", "Banjo-Kazooie", "Bomberman 64",
//         "Bomberman 64: Second Attack", "Command & Conquer", "Chopper Attack",
//         "NBA Courtside 2 featuring Kobe Bryant", "Penny Racers",
//         "Chameleon Twist", "Cruis'n USA", "Cruis'n World",
//         "Legend of Zelda: Majora's Mask, The", "Donkey Kong 64",
//         "Donkey Kong 64", "Donald Duck: Goin' Quackers",
//         "Loony Toons: Duck Dodgers", "Diddy Kong Racing", "PGA European Tour",
//         "Star Wars Episode 1 Racer", "AeroFighters Assault", "Bass Hunter 64",
//         "Conker's Bad Fur Day", "F-1 World Grand Prix", "Star Fox 64",
//         "F-Zero X", "GT64 Championship Edition", "GoldenEye 007", "Glover",
//         "Bomberman 64", "Indy Racing 2000",
//         "Indiana Jones and the Infernal Machine", "Jet Force Gemini",
//         "Jet Force Gemini", "Earthworm Jim 3D", "Snowboard Kids 2",
//         "Kirby 64: The Crystal Shards", "Fighters Destiny",
//         "Major League Baseball featuring Ken Griffey Jr.",
//         "Killer Instinct Gold", "Ken Griffey Jr's Slugfest", "Mario Kart 64",
//         "Mario Party", "Lode Runner 3D", "Megaman 64", "Mario Tennis",
//         "Mario Golf", "Mission: Impossible", "Mickey's Speedway USA",
//         "Monopoly", "Paper Mario", "Multi-Racing Championship",
//         "Big Mountain 2000", "Mario Party 3", "Mario Party 2", "Excitebike 64",
//         "Dr. Mario 64", "Star Wars Episode 1: Battle for Naboo",
//         "Kobe Bryant in NBA Courtside", "Excitebike 64",
//         "Ogre Battle 64: Person of Lordly Caliber", "Pokémon Stadium 2",
//         "Pokémon Stadium 2", "Perfect Dark", "Pokémon Snap",
//         "Hey you, Pikachu!", "Pokémon Snap", "Pokémon Puzzle League",
//         "Pokémon Stadium", "Pokémon Stadium", "Pilotwings 64",
//         "Top Gear Overdrive", "Resident Evil 2", "New Tetris, The",
//         "Star Wars: Rogue Squadron", "Ridge Racer 64",
//         "Star Soldier: Vanishing Earth", "AeroFighters Assault",
//         "Starshot Space Circus", "Super Mario 64", "Starcraft 64",
//         "Rocket: Robot on Wheels", "Space Station Silicon Valley",
//         "Star Wars: Shadows of the Empire", "Tigger's Honey Hunt",
//         "1080º Snowboarding", "Tom & Jerry in Fists of Furry",
//         "Mischief Makers", "All-Star Tennis '99", "Tetrisphere",
//         "V-Rally Edition '99", "V-Rally Edition '99", "WCW/NWO Revenge",
//         "WWF: No Mercy", "Waialae Country Club: True Golf Classics",
//         "Wave Race 64", "Worms Armageddon", "WWF: Wrestlemania 2000",
//         "Cruis'n Exotica", "Yoshi's Story", "Harvest Moon 64",
//         "Legend of Zelda: Ocarina of Time, The",
//         "Legend of Zelda: Majora's Mask, The", "Airboarder 64",
//         "Bakuretsu Muteki Bangaioh", "Choro-Q 64 II", "Custom Robo",
//         "Custom Robo V2", "Densha de Go! 64", "Doraemon: Mittsu no Seireiseki",
//         "Dezaemon 3D", "Transformers Beast Wars",
//         "Transformers Beast Wars Metals", "64 Trump Collection", "Bass Rush",
//         "ECW Hardcore Revolution", "40 Winks", "Aero Gauge",
//         "Aidyn Chronicles The First Mage", "Derby Stallion 64",
//         "Doraemon 2 - Hikari no Shinden", "Doraemon 3 - Nobi Dai No Machi SOS",
//         "F-1 World Grand Prix II", "Fushigi no Dungeon - Furai no Shiren 2",
//         "Heiwa Pachinko World 64", "Neon Genesis Evangelion",
//         "Racing Simulation", "Tsumi to Batsu", "Sonic Wings Assault",
//         "Virtual Pro Wrestling", "Virtual Pro Wrestling 2", "Wild Choppers"
//     };
//     */
//     char *cartIDs[] = {
//         "DZ", "B6", "ZY", "ZZ", "AD", "AL", "B7", "BC", "BD", "BH", "BK", "BM",
//         "BV", "CC", "CH", "CK", "CR", "CT", "CU", "CW", "DL", "DO", "DP", "DQ",
//         "DU", "DY", "EA", "EP", "ER", "FH", "FU", "FW", "FX", "FZ", "GC", "GE",
//         "GV", "HA", "IC", "IJ", "JD", "JF", "JM", "K2", "K4", "KA", "KG", "KI",
//         "KJ", "KT", "LB", "LR", "M6", "M8", "MF", "MI", "ML", "MO", "MQ", "MR",
//         "MU", "MV", "MW", "MX", "N6", "NA", "NB", "NX", "OB", "P2", "P3", "PD",
//         "PF", "PG", "PH", "PN", "PO", "PS", "PW", "RC", "RE", "RI", "RS", "RZ",
//         "S6", "SA", "SC", "SM", "SQ", "SU", "SV", "SW", "T9", "TE", "TJ", "TM",
//         "TN", "TP", "VL", "VY", "W2", "W4", "WL", "WR", "WU", "WX", "XO", "YS",
//         "YW", "ZL", "ZS", "AB", "BN", "CG", "CX", "CZ", "D6", "DR", "DZ", "OH",
//         "TB", "TC", "VB", "WI", "4W", "AG", "AY", "DA", "D2", "3D", "F2", "SI",
//         "HP", "EV", "MG", "GU", "SA", "VP", "A2", "WC"
//     };

//     /*
//     int saveTypes[] = {
//         5, 1, 6, 5, 5, 5, 5, 5, 5, 4, 5, 4, 5, 5, 5, 6, 4, 6, 6, 5, 5, 5, 5, 6,
//         5, 5, 6, 5, 5, 1, 5, 5, 5, 5, 5, 5, 4, 4, 5, 5, 5, 5, 1, 5, 4, 5, 5, 5,
//         4, 6, 1, 5, 5, 5, 4, 5, 5, 6, 5, 6, 5, 5, 6, 6, 1, 4, 4, 6, 4, 5, 4, 4,
//         4, 4, 5, 5, 1, 1, 5, 6, 5, 5, 5, 5, 4, 5, 5, 5, 4, 1, 5, 5, 5, 5, 5, 5,
//         1, 4, 5, 5, 5, 1, 5, 6, 1, 1, 4, 5, 5, 5, 5, 6, 1, 5, 1, 5, 5, 5, 1, 1,
//         5, 5, 1, 1, 6, 6, 6, 4, 5, 6, 5, 5, 5, 1, 1, 5
//     };
//     */
//     // Banjo-Tooie B7 -> set to sram 'cause crk converts ek16->sram
//     int saveTypes[] = {
//         2, 1, 5, 1, 3, 1, 1, 3, 3, 3, 3, 3, 3, 5, 3, 5, 3, 3, 3, 4, 5, 4, 4, 3,
//         3, 3, 3, 4, 3, 3, 4, 3, 3, 1, 3, 3, 3, 3, 3, 3, 5, 5, 3, 3, 3, 3, 1, 3,
//         5, 3, 3, 3, 5, 4, 1, 3, 3, 3, 5, 3, 3, 4, 3, 4, 3, 3, 4, 4, 1, 5, 5, 4,
//         5, 3, 5, 5, 5, 5, 3, 3, 1, 1, 3, 4, 3, 3, 3, 3, 5, 3, 3, 3, 5, 1, 3, 3,
//         3, 3, 3, 3, 1, 5, 3, 3, 3, 1, 3, 4, 1, 1, 5, 3, 3, 3, 3, 4, 1, 3, 1, 3,
//         3, 3, 1, 1, 3, 3, 1, 1, 4, 4, 4, 5, 3, 4, 3, 3, 3, 1, 1, 3
//     };

//     //bt cic to 2 pos6 was 5
//     int cicTypes[] = {
//         2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 6, 5, 5, 5, 2,
//         2, 3, 2, 2, 2, 2, 5, 2, 1, 6, 2, 2, 2, 2, 2, 2, 5, 5, 2, 2, 3, 2, 3, 2,
//         3, 2, 2, 2, 2, 2, 2, 2, 5, 2, 3, 2, 2, 2, 2, 3, 2, 2, 3, 3, 2, 3, 3, 5,
//         3, 2, 3, 2, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2,
//         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 6, 2, 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2,
//         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
//     };

//     // search for cartid
//     for (i=0; i<NUM_CARTS; i++)
//         if (strcmp(cartid, cartIDs[i]) == 0)
//             break;

//     if (i == NUM_CARTS) {
//         // cart not in list
//         *cic = 2;
//         *save = 0;
//         return 0; // not found
//     }

//     // cart found
//     *cic = cicTypes[i];
//     *save = saveTypes[i];

//     return 1; // found
// }


// const char* saveTypeToExtension(int type, int etype) {
//     static char* str = "SAV";

//     if(etype==0){
//         switch(type) {
//             case 0:  str = "OFF"; break;
//             case 1:  str = "SRM"; break;
//             case 2:  str = "128"; break;
//             case 3:  str = "E4K";  break;
//             case 4:  str = "E16"; break;
//             case 5:  str = "FLA"; break;
//             default: str = "SAV";
//         }
//     }
//     else {
//         switch(type) {
//             case 0:  str = "OFF"; break;
//             case 1:  str = "SRA"; break;
//             case 2:  str = "SRA"; break;
//             case 3:  str = "EEP";  break;
//             case 4:  str = "EEP"; break;
//             case 5:  str = "FLA"; break;
//             default: str = "SAV";
//         }
//     }

//     return str;
// }

// int saveTypeToSize(int type) {
//     switch(type) {
//         case 0: return 0; 					break;
//         case 1: return SAVE_SIZE_SRAM; 		break;
//         case 2: return SAVE_SIZE_SRAM128;	break;
//         case 3: return SAVE_SIZE_EEP4k;		break;
//         case 4: return SAVE_SIZE_EEP16k; 	break;
//         case 5: return SAVE_SIZE_FLASH; 	break;
//         default: return 0;
//     }
// }

// /*
// #define SAVE_TYPE_OFF 0
// #define SAVE_TYPE_SRAM 1
// #define SAVE_TYPE_SRAM128 2
// #define SAVE_TYPE_EEP4k 3
// #define SAVE_TYPE_EEP16k 4
// #define SAVE_TYPE_FLASH 5
// */


// //switch to the correct dump function
// int getSaveFromCart(int stype, uint8_t *buffer) {
//     int ret=0;

//     switch(stype) {
//         case 0: return 0;
//         case 1: ret = getSRAM32(buffer);	break;
//         case 2: ret = getSRAM128(buffer);   break;
//         case 3: ret = getEeprom4k(buffer);  break;
//         case 4: ret = getEeprom16k(buffer); break;
//         case 5: ret = getFlashRAM(buffer);  break;
//         default: return 0;
//     }

//     return ret;
// }


// //switch to the correct upload function
// int pushSaveToCart(int stype, uint8_t *buffer){
//     int ret=0;

//     switch(stype) {
//         case 0: return 0;
//         case 1: ret = setSRAM32(buffer);	break;
//         case 2: ret = setSRAM128(buffer);   break;
//         case 3: ret = setEeprom4k(buffer);  break;
//         case 4: ret = setEeprom16k(buffer); break;
//         case 5: ret = setFlashRAM(buffer);  break;
//         default: return 0;
//     }

//     return ret;
// }


// int getSRAM( uint8_t *buffer, int size){
//     while (dma_busy()) ;

//     IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x05);
//     IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x0C);
//     IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x0D);
//     IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x02);

//     while (dma_busy()) ;

//     PI_Init();

//     sleep(1000);

//     while (dma_busy()) ;

//     PI_DMAFromSRAM(buffer, 0, size) ;

//     while (dma_busy()) ;

//     IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
//     IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x12);
//     IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x07);
//     IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x03);

//     return 1;
// }


// int getSRAM32(  uint8_t *buffer) {
//     return getSRAM(buffer, SAVE_SIZE_SRAM);
// }

// int getSRAM128( uint8_t *buffer) {
//     return getSRAM(buffer, SAVE_SIZE_SRAM128);
// }


// //working hurray :D
// int getEeprom4k(  uint8_t *buffer) {
//     if(eeprom_present()){
//         int blocks=SAVE_SIZE_EEP4k/8;
//         for( int b = 0; b < blocks; b++ ) {
//             eeprom_read( b, &buffer[b * 8] );
//         }
//         return 1;
//     }

//     return 0;
// }

// int getEeprom16k(  uint8_t *buffer){
//     int blocks=SAVE_SIZE_EEP16k/8;
//     for( int b = 0; b < blocks; b++ ) {
//         eeprom_read( b, &buffer[b * 8] );
//     }

//     return 1;
// }

// int getFlashRAM( uint8_t *buffer){
//     evd_setSaveType(SAVE_TYPE_SRAM128); //2
//     sleep(10);

//     int s = getSRAM(buffer, SAVE_SIZE_SRAM128);
//     data_cache_hit_writeback_invalidate(buffer,SAVE_SIZE_SRAM128);

//     sleep(10);
//     evd_setSaveType(SAVE_TYPE_FLASH); //5

//     return 1;
// }


// /*
// sram upload
// */
// int setSRAM(  uint8_t *buffer,int size){
//      //half working
//     PI_DMAWait();
//     //Timing
//     PI_Init_SRAM();

//     //Readmode
//     PI_Init();

//     data_cache_hit_writeback_invalidate(buffer,size);
//      while (dma_busy());
//     PI_DMAToSRAM(buffer, 0, size);
//     data_cache_hit_writeback_invalidate(buffer,size);

//     //Wait
//     PI_DMAWait();
//     //Restore evd Timing
//     setSDTiming();

//     return 1;
// }

// int setSRAM32( uint8_t *buffer){
//     return setSRAM(buffer, SAVE_SIZE_SRAM);
// }

// int setSRAM128( uint8_t *buffer){
//     return setSRAM(buffer, SAVE_SIZE_SRAM128);
// }


// //working hurray :D
// int setEeprom4k( uint8_t *buffer){
//     if(eeprom_present()){
//         int blocks=SAVE_SIZE_EEP4k/8;
//         for( int b = 0; b < blocks; b++ ) {
//             eeprom_write( b, &buffer[b * 8] );
//         }
//         return 1;
//     }

//     return 0;
// }

// int setEeprom16k(uint8_t *buffer){
//     int blocks=SAVE_SIZE_EEP16k/8;
//     for( int b = 0; b < blocks; b++ ) {
//         eeprom_write( b, &buffer[b * 8] );
//     }

//     return 1;
// }


// //isn't working nor finished
// int setFlashRAM(uint8_t *buffer){
//     evd_setSaveType(SAVE_TYPE_SRAM128); //2
//     sleep(10);

//     int s = setSRAM(buffer, SAVE_SIZE_SRAM128);

//     evd_setSaveType(SAVE_TYPE_FLASH); //5

//     return 1;
// }

// void setSDTiming(void){
// /*
// PI_BSD_DOM1_LAT_REG (0x04600014) write word 0x000000FF
// PI_BSD_DOM1_PWD_REG (0x04600018) write word 0x000000FF
// PI_BSD_DOM1_PGS_REG (0x0460001C) write word 0x0000000F
// PI_BSD_DOM1_RLS_REG (0x04600020) write word 0x00000003
// *
// PI_BSD_DOM1_LAT_REG (0x04600014) write word 0x00000040
// PI_BSD_DOM1_PWD_REG (0x04600018) write word 0x00803712
// PI_BSD_DOM1_PGS_REG (0x0460001C) write word 0x00008037
// PI_BSD_DOM1_RLS_REG (0x04600020) write word 0x00000803
// */
//     // PI_DMAWait();
//     IO_WRITE(PI_BSD_DOM1_LAT_REG, 0x40);
//     IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x12);
//     IO_WRITE(PI_BSD_DOM1_PGS_REG, 0x07);
//     IO_WRITE(PI_BSD_DOM1_RLS_REG, 0x03);

//     IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
//     IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x12);
//     IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x07);
//     IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x03);
// }


// void _data_cache_invalidate_all(void) {
//     asm(
//         "li $8,0x80000000;"
//         "li $9,0x80000000;"
//         "addu $9,$9,0x1FF0;"
//         "cacheloop:;"
//         "cache 1,0($8);"
//         "cache 1,16($8);"
//         "cache 1,32($8);"
//         "cache 1,48($8);"
//         "cache 1,64($8);"
//         "cache 1,80($8);"
//         "cache 1,96($8);"
//         "addu $8,$8,112;"
//         "bne $8,$9,cacheloop;"
//         "cache 1,0($8);"
//     : // no outputs
//     : // no inputs
//     : "$8", "$9" // trashed registers
//     );
// }

// void restoreTiming(void) {
//     //n64 timing restore :>
//     IO_WRITE(PI_BSD_DOM1_LAT_REG, 0x40);
//     IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x12);
//     IO_WRITE(PI_BSD_DOM1_PGS_REG, 0x07);
//     IO_WRITE(PI_BSD_DOM1_RLS_REG, 0x03);

//     IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
//     IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x12);
//     IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x07);
//     IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x03);
// }

// /*
//  * Load an image from the rom filesystem, returning a pointer to the
//  * sprite that hold the image.
//  */
// sprite_t *loadImageDFS(char *fname) {
//     int size, x, y, n, fd;
//     u8 *tbuf;
//     u8 *ibuf;
//     sprite_t *sbuf;

//     fd = dfs_open(fname);
//     if (fd < 0)
//         return 0;                       // couldn't open image

//     size = dfs_size(fd);
//     tbuf = malloc(size);
//     if (!tbuf) {
//         dfs_close(fd);
//         return 0;                       // out of memory
//     }

//     dfs_read(tbuf, 1, size, fd);
//     dfs_close(fd);

//     ibuf = stbi_load_from_memory(tbuf, size, &x, &y, &n, 4);
//     free(tbuf);
//     if (!ibuf)
//         return 0;                       // couldn't decode image

//     sbuf = (sprite_t*)malloc(sizeof(sprite_t) + x * y * 2);
//     if (!sbuf) {
//         stbi_image_free(ibuf);
//         return 0;                       // out of memory
//     }
//     sbuf->width = x;
//     sbuf->height = y;
//     sbuf->bitdepth = 2;
//     sbuf->format = 0;
//     sbuf->hslices = x / 32;
//     sbuf->vslices = y / 16;

//     color_t *src = (color_t*)ibuf;
//     u16 *dst = (u16*)((u32)sbuf + sizeof(sprite_t));

//     for (int j=0; j<y; j++)
//         for (int i=0; i<x; i++)
//             dst[i + j*x] = graphics_convert_color(src[i + j*x]) & 0x0000FFFF;

//     /* Invalidate data associated with sprite in cache */
//     data_cache_hit_writeback_invalidate( sbuf->data, sbuf->width * sbuf->height * sbuf->bitdepth );

//     stbi_image_free(ibuf);
//     return sbuf;
// }


// sprite_t *loadImage32(u8 *png, int size) {
//     int x, y, n, fd;
//     u8 *tbuf;
//     u32 *ibuf;
//     sprite_t *sbuf;

//     tbuf = malloc(size);
//     memcpy(tbuf,png,size);

//     ibuf = (u32*)stbi_load_from_memory(tbuf, size, &x, &y, &n, 4);
//     free(tbuf);
//     if (!ibuf)
//         return 0;                       // couldn't decode image

//     sbuf = (sprite_t*)malloc(sizeof(sprite_t) + x * y * 4);
//     if (!sbuf) {
//         stbi_image_free(ibuf);
//         return 0;                       // out of memory
//     }

//     sbuf->width = x;
//     sbuf->height = y;
//     sbuf->bitdepth = 4;
//     sbuf->format = 0;
//     sbuf->hslices = x / 32;
//     sbuf->vslices = y / 32;

//    // color_t *src = (color_t*)ibuf;
//     u32 *dst = (u32*)((u32)sbuf + sizeof(sprite_t));

//     for (int j=0; j<y; j++)
//         for (int i=0; i<x; i++)
//             dst[i + j*x] = ibuf[i + j*x];

//     /* Invalidate data associated with sprite in cache */
//     data_cache_hit_writeback_invalidate( sbuf->data, sbuf->width * sbuf->height * sbuf->bitdepth );

//     stbi_image_free(ibuf);
//     return sbuf;
// }

// sprite_t *loadImage32DFS(char *fname) {
//     int size, x, y, n, fd;
//     u8 *tbuf;
//     u32 *ibuf;
//     sprite_t *sbuf;

//     fd = dfs_open(fname);
//     if (fd < 0)
//         return 0;                       // couldn't open image

//     size = dfs_size(fd);
//     tbuf = malloc(size);
//     if (!tbuf) {
//         dfs_close(fd);
//         return 0;                       // out of memory
//     }

//     dfs_read(tbuf, 1, size, fd);
//     dfs_close(fd);

//     ibuf = (u32*)stbi_load_from_memory(tbuf, size, &x, &y, &n, 4);
//     free(tbuf);
//     if (!ibuf)
//         return 0;                       // couldn't decode image

//     sbuf = (sprite_t*)malloc(sizeof(sprite_t) + x * y * 4);
//     if (!sbuf) {
//         stbi_image_free(ibuf);
//         return 0;                       // out of memory
//     }

//     sbuf->width = x;
//     sbuf->height = y;
//     sbuf->bitdepth = 4;
//     sbuf->format = 0;
//     sbuf->hslices = x / 32;
//     sbuf->vslices = y / 32;

//    // color_t *src = (color_t*)ibuf;
//     u32 *dst = (u32*)((u32)sbuf + sizeof(sprite_t));

//     for (int j=0; j<y; j++)
//         for (int i=0; i<x; i++)
//             dst[i + j*x] = ibuf[i + j*x];

//     /* Invalidate data associated with sprite in cache */
//     data_cache_hit_writeback_invalidate( sbuf->data, sbuf->width * sbuf->height * sbuf->bitdepth );

//     stbi_image_free(ibuf);
//     return sbuf;
// }

// /*
//  * Draw an image to the screen using the sprite passed.
//  */
// void drawImage(display_context_t dcon, sprite_t *sprite) {
//     int x, y = 0;

//     rdp_sync(SYNC_PIPE);
//     rdp_set_default_clipping();
//     rdp_enable_texture_copy();
//     rdp_attach_display(dcon);
//     // Draw image
//     for (int j=0; j<sprite->vslices; j++) {
//         x = 0;
//         for (int i=0; i<sprite->hslices; i++) {
//             rdp_sync(SYNC_PIPE);
//             rdp_load_texture_stride(0, 0, MIRROR_DISABLED, sprite, j*sprite->hslices + i);
//             rdp_draw_sprite(0, x, y);
//             x += 32;
//         }
//         y += 16;
//     }
//     rdp_detach_display();
// }


#define CIC_6101 1
#define CIC_6102 2
#define CIC_6103 3
#define CIC_6104 4
#define CIC_6105 5
#define CIC_6106 6

#define ROM         ((vu32 *)0xB0000000)
#define EXE_START   0xB0001000
#define EXE_SIZE    0x00100004
#define TV_TYPE     *(vu32 *)0x80000300
#define RAM_SIZE_1  *(vu32 *)0x80000318
#define RAM_SIZE_2  *(vu32 *)0x800003F0

// void simulate_boot(u32 cic_chip, u8 gBootCic, u32 *cheat_lists[2])
void simulate_boot(u32 cic_chip) {
    short int gCheats = 0;

    // Clear screen
    IO_WRITE(VI_V_INT, 0x3FF);
    IO_WRITE(VI_H_LIMITS, 0);
    IO_WRITE(VI_CUR_LINE, 0);

    // Reset controller and clear interrupt
    IO_WRITE(PI_STATUS_REG, 0x03);

    // Set cart latency registers with values specified in ROM
    u32 lat = ROM[0];
    IO_WRITE(PI_BSD_DOM1_LAT_REG, lat & 0xFF);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, lat >> 8);
    IO_WRITE(PI_BSD_DOM1_PGS_REG, lat >> 16);
    IO_WRITE(PI_BSD_DOM1_RLS_REG, lat >> 20);

    // Fix RAM size location (State required by CIC-NUS-6105)
    // vu32 *ram_size = (cic_chip == CIC_6105) ? &RAM_SIZE_2 : &RAM_SIZE_1;
    // *ram_size = (gBootCic == CIC_6105) ? RAM_SIZE_2 : RAM_SIZE_1;

    // if (force_tv) {
    //     /*
    //      * This magic bit-twiddling is required to retain backward compatibility
    //      * with old ini files. It converts alt64's tv mode to N64's tv mode:
    //      * alt64:
    //      *   0: Use default
    //      *   1: Force NTSC
    //      *   2: Force PAL
    //      *   3: Force M-PAL
    //      * N64:
    //      *   0: PAL
    //      *   1: NTSC
    //      *   2: M-PAL
    //      *   3: Unused
    //      */
    //     TV_TYPE = MIN((~force_tv - 1) & 3, 2);
    // }

    // if (gCheats) {
    //     // Copy patcher into a memory location where it will not be overwritten
    //     void *patcher = (void*)0x80700000; // Temporary patcher location
    //     u32 patcher_length = &&patcher_end - &&patcher_start;
    //     memcpy(patcher, &&patcher_start, patcher_length);

    //     // Copy code lists into memory, behind the patcher
    //     u32 *p = patcher + patcher_length;
    //     *(u32**)0xA06FFFFC = p; // Save temporary code list location
    //     for (int i = 0; i < 2; i++) {
    //         int j = -2;
    //         do {
    //             j += 2;
    //             patcher_length += 8;

    //             *p++ = cheat_lists[i][j];
    //             *p++ = cheat_lists[i][j + 1];
    //         } while (cheat_lists[i][j] || cheat_lists[i][j + 1]);
    //     }

    //     // Write cache to physical memory and invalidate
    //     data_cache_hit_writeback(patcher, patcher_length);
    //     inst_cache_hit_invalidate(patcher, patcher_length);
    // }

    // Start game via CIC boot code
    asm __volatile__ (
        ".set noreorder;"

        "lui    $t0, 0x8000;"

        // State required by all CICs
        "move   $s3, $zero;"            // osRomType (0: N64, 1: 64DD)
        "lw     $s4, 0x0300($t0);"      // osTvType (0: PAL, 1: NTSC, 2: MPAL)
        "move   $s5, $zero;"            // osResetType (0: Cold, 1: NMI)
        "lui    $s6, %%hi(cic_ids);"    // osCicId (See cic_ids LUT)
        "addu   $s6, $s6, %0;"
        "lbu    $s6, %%lo(cic_ids)($s6);"
        "lw     $s7, 0x0314($t0);"      // osVersion

        // Copy PIF code to RSP IMEM (State required by CIC-NUS-6105)
        "lui    $a0, 0xA400;"
        "lui    $a1, %%hi(imem_start);"
        "ori    $a2, $zero, 0x0008;"
    "1:"
        "lw     $t0, %%lo(imem_start)($a1);"
        "addiu  $a1, $a1, 4;"
        "sw     $t0, 0x1000($a0);"
        "addiu  $a2, $a2, -1;"
        "bnez   $a2, 1b;"
        "addiu  $a0, $a0, 4;"

        // Copy CIC boot code to RSP DMEM
        "lui    $t3, 0xA400;"
        "ori    $t3, $t3, 0x0040;"      // State required by CIC-NUS-6105
        "move   $a0, $t3;"
        "lui    $a1, 0xB000;"
        "ori    $a2, 0x0FC0;"
    "1:"
        "lw     $t0, 0x0040($a1);"
        "addiu  $a1, $a1, 4;"
        "sw     $t0, 0x0000($a0);"
        "addiu  $a2, $a2, -4;"
        "bnez   $a2, 1b;"
        "addiu  $a0, $a0, 4;"

        // Boot with or without cheats enabled?
        "beqz   %1, 2f;"

        // Patch CIC boot code
        "lui    $a1, %%hi(cic_patch_offsets);"
        "addu   $a1, $a1, %0;"
        "lbu    $a1, %%lo(cic_patch_offsets)($a1);"
        "addu   $a0, $t3, $a1;"
        "lui    $a1, 0x081C;"           // "j 0x80700000"
        "ori    $a2, $zero, 0x06;"
        "bne    %0, $a2, 1f;"
        "lui    $a2, 0x8188;"
        "ori    $a2, $a2, 0x764A;"
        "xor    $a1, $a1, $a2;"         // CIC-NUS-6106 encryption
    "1:"
        "sw     $a1, 0x0700($a0);"      // Patch CIC boot code with jump

        // Patch CIC boot code to disable checksum failure halt
        // Required for CIC-NUS-6105
        "ori    $a2, $zero, 0x05;"
        "beql   %0, $a2, 2f;"
        "sw     $zero, 0x06CC($a0);"

        // Go!
    "2:"
        "lui    $sp, 0xA400;"
        "ori    $ra, $sp, 0x1550;"      // State required by CIC-NUS-6105
        "jr     $t3;"
        "ori    $sp, $sp, 0x1FF0;"      // State required by CIC-NUS-6105


    // Table of all CIC IDs
    "cic_ids:"
        ".byte  0x00;"                  // Unused
        ".byte  0x3F;"                  // NUS-CIC-6101
        ".byte  0x3F;"                  // NUS-CIC-6102
        ".byte  0x78;"                  // NUS-CIC-6103
        ".byte  0x00;"                  // Unused
        ".byte  0x91;"                  // NUS-CIC-6105
        ".byte  0x85;"                  // NUS-CIC-6106
        ".byte  0x00;"                  // Unused

    "cic_patch_offsets:"
        ".byte  0x00;"                  // Unused
        ".byte  0x30;"                  // CIC-NUS-6101
        ".byte  0x2C;"                  // CIC-NUS-6102
        ".byte  0x20;"                  // CIC-NUS-6103
        ".byte  0x00;"                  // Unused
        ".byte  0x8C;"                  // CIC-NUS-6105
        ".byte  0x60;"                  // CIC-NUS-6106
        ".byte  0x00;"                  // Unused

    // These instructions are copied to RSP IMEM; we don't execute them.
    "imem_start:"
        "lui    $t5, 0xBFC0;"
    "1:"
        "lw     $t0, 0x07FC($t5);"
        "addiu  $t5, $t5, 0x07C0;"
        "andi   $t0, $t0, 0x0080;"
        "bnezl  $t0, 1b;"
        "lui    $t5, 0xBFC0;"
        "lw     $t0, 0x0024($t5);"
        "lui    $t3, 0xB000;"

        :                               // outputs
        : "r" (cic_chip),               // inputs
          "r" (gCheats)
        : "$4", "$5", "$6", "$8",       // clobber
          "$11", "$19", "$20", "$21",
          "$22", "$23", "memory"
    );

patcher_start:
    asm __volatile__ (
        ".set noat;"
        ".set noreorder;"

    // Installs general exception handler, router, and code engine
    "patcher:"
        "lui    $t5, 0x8070;"           // Start of temporary patcher location
        "lui    $t6, 0x8000;"           // Start of cached memory
        "li     $t7, 0x007FFFFF;"       // Address mask
        "li     $t8, 0x807C5C00;"       // Permanent code engine location
//      "lw     $t9, 0xFFFC($t5);"      // Assembles incorrectly (gcc sucks)
        "addiu  $t9, $t5, -4;"          // Go to hell, gcc!
        "lw     $t9, 0x0000($t9);"      // Get temporary code lists location

    "1:"
        // Apply boot-time cheats
        "lw     $v0, 0x0000($t9);"
        "bnez   $v0, 2f;"
        "lw     $v1, 0x0004($t9);"
        "beqz   $v1, 1f;"

    "2:"
        "addiu  $t9, $t9, 0x0008;"
        "srl    $t2, $v0, 24;"
        "li     $at, 0xEE;"
        "beq    $t2, $at, 5f;"
        "li     $at, 0xF0;"
        "and    $v0, $v0, $t7;"
        "beq    $t2, $at, 4f;"
        "or     $v0, $v0, $t6;"
        "li     $at, 0xF1;"
        "beq    $t2, $at, 3f;"
        "nop;"

        // Apply FF code type
        "li     $at, 0xFFFFFFFC;"       // Mask address
        "b      1b;"
        "and    $t8, $v0, $at;"         // Update permanent code engine location

    "3:"
        // Apply F1 code type
        "b      1b;"
        "sh     $v1, 0x0000($v0);"

    "4:"
        // Apply F0 code type
        "b      1b;"
        "sb     $v1, 0x0000($v0);"

    "5:"
        // Apply EE code type
        "lui    $v0, 0x0040;"
        "sw     $v0, 0x0318($t6);"
        "b      1b;"
        "sw     $v0, 0x03F0($t6);"


    "1:"
        // Install General Exception Handler
        "srl    $at, $t8, 2;"
        "and    $v0, $at, $t7;"
        "lui    $at, 0x0800;"
        "or     $v0, $v0, $at;"         // Jump to code engine
        "sw     $v0, 0x0180($t6);"
        "sw     $zero, 0x0184($t6);"


        // Install code engine to permanent location
        "sw     $t8, 0x0188($t6);"      // Save permanent code engine location
        "la     $at, %%lo(patcher);"
        "la     $v0, %%lo(code_engine_start);"
        "la     $v1, %%lo(code_engine_end);"
        "subu   $at, $v0, $at;"         // Get patcher length
        "subu   $v1, $v1, $v0;"         // Get code engine length
        "addu   $v0, $t5, $at;"         // Get temporary code engine location
    "1:"
        "lw     $at, 0x0000($v0);"
        "addiu  $v1, $v1, -4;"
        "sw     $at, 0x0000($t8);"
        "addiu  $v0, $v0, 4;"
        "bgtz   $v1, 1b;"
        "addiu  $t8, $t8, 4;"
        "sw     $t8, 0x018C($t6);"      // Save permanent code list location


    "1:"
        // Install in-game code list
        "lw     $v0, 0x0000($t9);"
        "lw     $v1, 0x0004($t9);"
        "addiu  $t9, $t9, 8;"
        "sw     $v0, 0x0000($t8);"
        "sw     $v1, 0x0004($t8);"
        "bnez   $v0, 1b;"
        "addiu  $t8, $t8, 8;"
        "bnez   $v1, 1b;"
        "nop;"


        // Write cache to physical memory and invalidate (GEH)
        "ori    $t0, $t6, 0x0180;"
        "li     $at, 0x0010;"
    "1:"
        "cache  0x19, 0x0000($t0);"     // Data cache hit writeback
        "cache  0x10, 0x0000($t0);"     // Instruction cache hit invalidate
        "addiu  $at, $at, -4;"
        "bnez   $at, 1b;"
        "addiu  $t0, $t0, 4;"


        // Write cache to physical memory and invalidate (code engine + list)
        "lw     $t0, 0x0188($t6);"
        "subu   $at, $t8, $t0;"
    "1:"
        "cache  0x19, 0x0000($t0);"     // Data cache hit writeback
        "cache  0x10, 0x0000($t0);"     // Instruction cache hit invalidate
        "addiu  $at, $at, -4;"
        "bnez   $at, 1b;"
        "addiu  $t0, $t0, 4;"


        // Protect GEH via WatchLo/WatchHi
        "li     $t0, 0x0181;"           // Watch 0x80000180 for writes
        "mtc0   $t0, $18;"              // Cp0 WatchLo
        "nop;"
        "mtc0   $zero, $19;"            // Cp0 WatchHi


        // Start game!
        "jr     $t1;"
        "nop;"


    "code_engine_start:"
        "mfc0   $k0, $13;"              // Cp0 Cause
        "andi   $k1, $k0, 0x1000;"      // Pre-NMI
        "bnezl  $k1, 1f;"
        "mtc0   $zero, $18;"            // Cp0 WatchLo
    "1:"

        "andi   $k0, $k0, 0x7C;"
        "li     $k1, 0x5C;"             // Watchpoint
        "bne    $k0, $k1, 1f;"

        // Watch exception; manipulate register contents
        "mfc0   $k1, $14;"              // Cp0 EPC
        "lw     $k1, 0x0000($k1);"      // Load cause instruction
        "lui    $k0, 0x03E0;"
        "and    $k1, $k1, $k0;"         // Mask (base) register
        "srl    $k1, $k1, 5;"           // Shift it to the "rt" position
        "lui    $k0, 0x3740;"           // Upper half "ori $zero, $k0, 0x0120"
        "or     $k1, $k1, $k0;"
        "ori    $k1, $k1, 0x0120;"      // Lower half "ori $zero, $k0, 0x0120"
        "lui    $k0, 0x8000;"
        "lw     $k0, 0x0188($k0);"      // Load permanent code engine location
        "sw     $k1, 0x0060($k0);"      // Self-modifying code FTW!
        "cache  0x19, 0x0060($k0);"     // Data cache hit writeback
        "cache  0x10, 0x0060($k0);"     // Instruction cache hit invalidate
        "lui    $k0, 0x8000;"
        "nop;"                          // Short delay for cache sync
        "nop;"
        "nop;"
        "nop;"                          // Placeholder for self-modifying code
        "eret;"                         // Back to game

    "1:"
        // Run code engine
        "lui    $k0, 0x8000;"
        "lw     $k0, 0x0188($k0);"
        "addiu  $k0, $k0, -0x28;"
        "sd     $v1, 0x0000($k0);"
        "sd     $v0, 0x0008($k0);"
        "sd     $t9, 0x0010($k0);"
        "sd     $t8, 0x0018($k0);"
        "sd     $t7, 0x0020($k0);"

        // Handle cheats
        "lui    $t9, 0x8000;"
        "lw     $t9, 0x018C($t9);"      // Get code list location
    "1:"
        "lw     $v0, 0x0000($t9);"      // Load address
        "bnez   $v0, 2f;"
        "lw     $v1, 0x0004($t9);"      // Load value
        "beqz   $v1, 4f;"
        "nop;"

        // Address == 0 (TODO)
        "b      1b;"

    "2:"
        // Address != 0
        "addiu  $t9, $t9, 0x0008;"
        "srl    $t7, $v0, 24;"
        "sltiu  $k1, $t7, 0xD0;"        // Code type < 0xD0 ?
        "sltiu  $t8, $t7, 0xD4;"        // Code type < 0xD4 ?
        "xor    $k1, $k1, $t8;"         // $k1 = (0xD0 >= code_type < 0xD4)
        "li     $t8, 0x50;"
        "bne    $t7, $t8, 3f;"          // Code type != 0x50 ? -> 3

        // GS Patch/Repeater
        "srl    $t8, $v0, 8;"
        "andi   $t8, $t8, 0x00FF;"      // Get address count
        "andi   $t7, $v0, 0x00FF;"      // Get address increment
        "lw     $v0, 0x0000($t9);"      // Load address
        "lw     $k1, 0x0004($t9);"      // Load value
        "addiu  $t9, $t9, 0x0008;"

    "2:"
        "sh     $k1, 0x0000($v0);"      // Repeater/Patch write
        "addiu  $t8, $t8, -1;"
        "addu   $v0, $v0, $t7;"
        "bnez   $t8, 2b;"
        "addu   $k1, $k1, $v1;"
        "b      1b;"

    "3:"
        // GS RAM write or Conditional
        "lui    $t7, 0x0300;"
        "and    $t7, $v0, $t7;"         // Test for 8-bit or 16-bit code type
        "li     $t8, 0xA07FFFFF;"
        "and    $v0, $v0, $t8;"
        "lui    $t8, 0x8000;"
        "beqz   $k1, 2f;"
        "or     $v0, $v0, $t8;"         // Mask address

        // GS Conditional
        "sll    $k1, $t7, 7;"
        "beqzl  $k1, 3f;"
        "lbu    $t8, 0x0000($v0);"      // 8-bit conditional
        "lhu    $t8, 0x0000($v0);"      // 16-bit conditional
    "3:"
        "srl    $t7, $t7, 22;"
        "andi   $t7, $t7, 8;"           // Test for equal-to or not-equal-to
        "beql   $v1, $t8, 1b;"
        "add    $t9, $t9, $t7;"         // Add if equal
        "xori   $t7, $t7, 8;"
        "b      1b;"
        "add    $t9, $t9, $t7;"         // Add if not-equal

    "2:"
        // GS RAM write
        "sll    $k1, $t7, 7;"
        "beqzl  $k1, 3f;"
        "sb     $v1, 0x0000($v0);"      // Constant 8-bit write
        "sh     $v1, 0x0000($v0);"      // Constant 16-bit write
    "3:"
        "b      1b;"

    "4:"
        // Restore registers from our temporary stack, and back to the game!
        "ld     $t7, 0x0020($k0);"
        "ld     $t8, 0x0018($k0);"
        "ld     $t9, 0x0010($k0);"
        "ld     $v0, 0x0008($k0);"
        "j      0x80000120;"
        "ld     $v1, 0x0000($k0);"
    "code_engine_end:"

        :                               // outputs
        :                               // inputs
        : "memory"                      // clobber
    );
patcher_end:

    return;
}