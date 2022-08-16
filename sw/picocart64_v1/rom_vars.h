#define MAPPING_TABLE_LEN (16384 - 8)

#define COMPRESSED_ROM 1
#define COMPRESSION_SHIFT_AMOUNT 10
#define COMPRESSION_MASK 1023

extern const char picocart_header[16];
extern uint16_t rom_mapping[MAPPING_TABLE_LEN];
extern const uint16_t flash_rom_mapping[];

extern const unsigned char rom_chunks[][1024];
