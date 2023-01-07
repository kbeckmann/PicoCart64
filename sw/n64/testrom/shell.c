/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <libdragon.h>
#include <usb.h>

#include "git_info.h"
#include "shell.h"

// picocart64_shared
#include "pc64_regs.h"
#include "pc64_rand.h"
#include "n64_defs.h"
#include "pc64_utils.h"

#include "rom_defs.h"

/*
TODO
Make the more files above/below indicator a sprite or something besides '...'
Figure out why it feels laggy? The example code feels the same way, not sure it runs at full speed in the emulator
Load entries from an SD card
Load selected rom into memory and start

Discussion about what the menu feature set::
**Must haves**
1. Support for over 1k files per dir. 
2. Fast browsing skip to next set of alpha ordered letter. 
3. Title scrolling for long names. 
4. Select save type, select cic type or auto detect. 
5. Title images. 

**Should have**
1. Animation for the selector. 
2. Splash screen.

**Could have**
1. Title video. 
2. Cheat support 
3. Inplace byteswap. 
4. Save game revisions.

*/

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 240
#define COLOR_TRANSPARENCY_ENABLED 0

 // TODO: It is likely a directory may contain thousands of files
 // Modify the ls function to only buffer a portion of files (up to some MAX)
 #define FILE_ENTRIES_BUFFER_SIZE 256
 #define FILE_NAME_MAX_LENGTH (MAX_FILENAME_LEN+1)

/*
 * Need some kind of lookup table or function that can find a thumbnail from a rom name, including the correct region version of the boxart
 * SD Card should contain
 * firmware folder
 *      thumbnails
 *          system(e.g. N64)
 *              thumbnail.png (Maybe use the serial number?) -- Rename the thumbnails by cross referencing the titles to the serial from the dat file
 *          
 */
typedef struct {
    /* Rom name can be extracted from the rom header at address 0x20 
     * Probably better to use that to match thumbnails and display the title
     */
    char filename[MAX_FILENAME_LEN+1];
    char title[64];
    int filesize;
    sprite_t* boxart;
    // region?
    // release year?
    // eeprom type/size?
    // byte order?
    // ipl checksums?
    // crc hash?
} rom_file_info_t;

rom_file_info_t** g_romFileInfoBuffer;

bool IS_EMULATOR = 0;
bool LOAD_BOX_ART = 0;

char** g_fileEntries; // Buffer for file entries
int* g_fileSizes;
// sprite_t** g_thumbnail_cache;
int g_currentPage = 0; // variable for file list pagination

bool thumbnail_loaded = false;
static sprite_t* current_thumbnail;
char* temp_serial;
char* temp_spritename;

//
// Thumbnails need to be cached for snappier menu feel
// #if BUILD_FOR_EMULATOR == 1
// /**/
char* g_thumbnail_table[] = {
    // "goldeneye.sprite", 
    // "NGEJ.sprite",
    // "doom.sprite", 
    // "mario_tennis.sprite", 
    // "mario64.sprite"
    "NGEE.sprite"
};

// int g_fileSizes[] = {
//     12582912,
//     8388608,
//     16777216,
//     8388608  
// };

char* g_fileInfo[] = {
    "Goldeneye 007",
    "DOOM 64",
    "Mario Tennis",
    "Super Mario 64"  
};
// /**/
// #else
// // TODO implement a more generic implementation of the file info stuff
// #endif

int NUM_ENTRIES = 0;
bool g_sendingSelectedRom = false;
int g_lastSelection = 0;
char g_infoPanelTextBuf[256];
bool g_isLoading = false;
bool g_isRenderingMenu = false;

/* Layout */
#define INFO_PANEL_WIDTH (192 + (MARGIN_PADDING * 2)) // NEEDS PARENS!!! Seems the compiler doesn't evaluate the define before using it for other defines
#define FILE_PANEL_WIDTH (SCREEN_WIDTH - INFO_PANEL_WIDTH)
#define MAX_BOXART_HEIGHT (192)

#define ROW_HEIGHT (12)
#define MARGIN_PADDING (10)
#define ROW_SELECTION_WIDTH (FILE_PANEL_WIDTH)
#define MENU_BAR_HEIGHT (15)
#define LIST_TOP_PADDING (MENU_BAR_HEIGHT + ROW_HEIGHT)
#define BOTTOM_BAR_HEIGHT (32)
#define BOTTOM_BAR_Y (SCREEN_HEIGHT - BOTTOM_BAR_HEIGHT)

// Menu sprites
sprite_t *a_button_icon;
sprite_t* spinner;
sprite_t** animated_spinner;
/* Colors */
color_t MENU_BAR_COLOR = { .r = 0x82, .g = 0x00, .b = 0x2E, .a = 0x00 }; // 0x82002E, Berry
color_t BOTTOM_BAR_COLOR = { .r = 0x00, .g = 0x67, .b = 0xC7, .a = 0x55 }; // 0x82002E, Berry
color_t SELECTION_COLOR = { .r = 0x00, .g = 0x67, .b = 0xC7, .a = 0x00 }; // 0x0067C7, Bright Blue
color_t LOADING_BOX_COLOR = { .r = 0x00, .g = 0x67, .b = 0xC7, .a = 0x00 }; // 0x0067C7, Bright Blue
color_t WHITE_COLOR = { .r = 0xCC, .g = 0xCC, .b = 0xCC, .a = 0x00 }; // 0xCCCCCC, White

void loadRomAtSelection(int selection) {
    g_sendingSelectedRom = true;

    char* fileToLoad = malloc(sizeof(char*) * 256);
    strcpy(fileToLoad, g_fileEntries[selection]);

    // Write the file name to the cart buffer
    uint32_t len_aligned32 = (strlen(g_fileEntries[selection]) + 3) & (-4);
    data_cache_hit_writeback_invalidate(g_fileEntries[selection], len_aligned32);
    pi_write_raw(g_fileEntries[selection], PC64_BASE_ADDRESS_START, 0, len_aligned32);
	// io_write(PC64_BASE_ADDRESS_START, fileToLoad);

    uint16_t sdSelectRomFilenameLength[] = { strlen(fileToLoad) };
    // data_cache_hit_writeback_invalidate(&sdSelectRomFilenameLength, sizeof(sdSelectRomFilenameLength));
    // Send command to start the load, the cart will check the pc64 buffer for the filename 
    data_cache_hit_writeback_invalidate(sdSelectRomFilenameLength, sizeof(uint16_t));
    pi_write_raw(sdSelectRomFilenameLength, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_SELECT_ROM, sizeof(uint16_t));
    // io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_SELECT_ROM, &sdSelectRomFilenameLength);

    g_isLoading = true;

    wait_ms(1000);
}

static uint16_t pc64_sd_wait_single() {
    
    uint16_t read_buf[] = { 1 };
    data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
    pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_BUSY, sizeof(uint16_t));
    //uint32_t isBusy = io_read(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_BUSY);

    return read_buf[0];
}

static uint8_t pc64_sd_wait() {
    uint32_t timeout = 0;
    uint16_t read_buf[] = { 1 };
	uint16_t busy[] = { 1 };
    // uint32_t isBusy = 0;
    
    // Wait until the cartridge interface is ready
    do {
        // returns 1 while sd card is busy
        data_cache_hit_writeback_invalidate(read_buf, sizeof(read_buf));
        pi_read_raw(read_buf, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_BUSY, sizeof(uint16_t));
        // isBusy = io_read(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_BUSY);
        
        // Took too long, abort
        if((timeout++) > 1000000)
            return -1;
    }
    while(memcmp(busy, read_buf, sizeof(uint16_t)) == 0);
    (void) timeout; // Needed to stop unused variable warning
    
    // Success
    return 0;
}

int boot_cic = 2;
// int boot_save = 0;
void bootRom(display_context_t disp, int silent) {
    if (boot_cic != 0)
    {
        // if (boot_save != 0)
        // {
        //     TCHAR cfg_file[32];

        //     //set cfg file with last loaded cart info and save-type
        //     sprintf(cfg_file, "/"ED64_FIRMWARE_PATH"/%s/LASTROM.CFG", save_path);

        //     FRESULT result;
        //     FIL file;
        //     result = f_open(&file, cfg_file, FA_WRITE | FA_CREATE_ALWAYS);

        //     if (result == FR_OK)
        //     {
        //         uint8_t cfg_data[2] = {boot_save, boot_cic};


        //         UINT bw;
        //         result = f_write (
        //             &file,          /* [IN] Pointer to the file object structure */
        //             &cfg_data, /* [IN] Pointer to the data to be written */
        //             2,         /* [IN] Number of bytes to write */
        //             &bw          /* [OUT] Pointer to the variable to return number of bytes written */
        //           );

        //         f_puts(rom_filename, &file);

        //         f_close(&file);

        //         //set the fpga cart-save type
        //         evd_setSaveType(boot_save);

        //         saveTypeFromSd(disp, rom_filename, boot_save);
        //     }
        // }

        volatile uint32_t cart, country;
        volatile uint32_t info = *(volatile uint32_t *)0xB000003C;
        cart = info >> 16;
        country = (info >> 8) & 0xFF;

        disable_interrupts();
        int bios_cic = 2; //getCicType(1);

        // if (checksum_fix_on)
        // {
        //     checksum_sdram();
        // }

        // evd_lockRegs();
        // sleep(10);

        // while (!(disp = display_lock()));
        // //blank screen to avoid glitches

        // graphics_fill_screen(disp, 0x000000FF);
        // display_show(disp);

        // f_mount(0, "", 0);                     /* Unmount the default drive */
        // free(fs);                              /* Here the work area can be discarded */
        simulate_boot(boot_cic); // boot_cic
    }
}

void waitForStart() {
    printf("Start to continue...\n");
    while (true) {
            controller_scan();
            struct controller_data keys = get_keys_pressed();
            if (keys.c[0].start) {
                wait_ms(300);
                break;
            }
    }
}

/* Assume default font size*/
static int calculate_num_rows_per_page(void) {
    // ---top of screen---
    // menu bar
    // padding + more files above indicator space
    // list of files...
    // ...
    // more file indicator space
    // ---bottom of screen---

    // Start by subtracting the menu bar height, and paddings from the total height
    int availableHeight = SCREEN_HEIGHT - MENU_BAR_HEIGHT - (ROW_HEIGHT * 2) - BOTTOM_BAR_HEIGHT;

    // Since there is currently no other dynamic portion, we can use a simple calculation to find rows that will fit in the space
    int rows = availableHeight / ROW_HEIGHT;

    return rows;
}

/*
 * Render a list of strings and show a caret on the currently selected row
 */
static void render_list(display_context_t display, int currently_selected, int first_visible, int max_on_screen)
{

	/* If we aren't starting at the first row, draw an indicator to show more files above */
	if (first_visible > 0) {
		graphics_draw_text(display, MARGIN_PADDING, MENU_BAR_HEIGHT, "...");
	}

	for (int i = 0; i < max_on_screen; i++) {
		int row = first_visible + i;
		int x = currently_selected == row ? MARGIN_PADDING : 0;	// if the current item is selected move it over so we can show a selection caret
		int y = i * ROW_HEIGHT + LIST_TOP_PADDING;	// First row must start below the menu bar, so add that plus a pad

		// if we run out of items to draw on the screen, don't draw anything else
		if (row >= NUM_ENTRIES) {
			break;
		}

		if (currently_selected == row) {
			/* Render selection box */
			graphics_draw_box(display, 0, y - 2, ROW_SELECTION_WIDTH, 10, graphics_convert_color(SELECTION_COLOR));
		} else {
			x += MARGIN_PADDING;
		}

		// Render the list item
		graphics_draw_text(display, x, y, g_fileEntries[row]);

		/* If this is the last row and there are more files below, draw an indicator */
		if (row + 1 < NUM_ENTRIES && i + 1 >= max_on_screen) {
			graphics_draw_text(display, MARGIN_PADDING, y + 10, "...");
		}
	}

    int x0 = FILE_PANEL_WIDTH-1, y0 = MENU_BAR_HEIGHT, x1 = FILE_PANEL_WIDTH-1, y1 = SCREEN_HEIGHT;
    graphics_draw_line(display, x0, y0, x1, y1, graphics_convert_color(SELECTION_COLOR));
    graphics_draw_line(display, x0+1, y0, x1+1, y1, graphics_convert_color(MENU_BAR_COLOR));
}

static void render_info_panel(display_context_t display, int currently_selected) {
    /*
    Discussion about things to display:
    It would be great to also see the config. Eeprom type/size. The byte order of the rom, ipl checksums, crc hash,
    */

   // Only update the text string if we have changed selection
    if (currently_selected != g_lastSelection) {
        memset(g_infoPanelTextBuf, 0, 256);
        memset(temp_serial, 0, 5);

        // TODO load more information from the rom header, Game name, crc, serial, country
        read_rom_header_serial_number(temp_serial, g_fileEntries[currently_selected]);

        // TODO if part of the string is longer than the number of characters that can fit in in the info panel width, split or clip it
        sprintf(g_infoPanelTextBuf, "%s\n%s\nSize: ?M\nCountry\nReleased ?\n%d", g_fileEntries[currently_selected], temp_serial, currently_selected);

        // TODO fix the below code, loading thumbnails is causing a crash.
        // Free the last thumbnail
        // if (thumbnail_loaded) {
        //     free(current_thumbnail);
        //     thumbnail_loaded = false;
        // }

        // Try to load a thumbnail, if this isn't a rom, don't load box art
        // if(load_boxart_for_rom(g_fileEntries[currently_selected]) != 0) {
        //     LOAD_BOX_ART = false;
        // } else {
        //     LOAD_BOX_ART = true;
        // }

        g_lastSelection = currently_selected;
    }

    int x = FILE_PANEL_WIDTH + MARGIN_PADDING, y = MENU_BAR_HEIGHT + MARGIN_PADDING;
    if (LOAD_BOX_ART && current_thumbnail != NULL) {
        int offset = ((INFO_PANEL_WIDTH / 2) + (current_thumbnail->width / 2));
        graphics_draw_sprite(display, SCREEN_WIDTH - offset, y, current_thumbnail);
        y += current_thumbnail->height + MARGIN_PADDING;
    } else {
        y += 100;
    }
    
    // Display the currently selected rom info
    graphics_draw_text(display, x, y, g_infoPanelTextBuf);

    /* Draw bottom menu bar for the info panel */
    graphics_draw_box(display, x - MARGIN_PADDING, BOTTOM_BAR_Y, INFO_PANEL_WIDTH, BOTTOM_BAR_HEIGHT, graphics_convert_color(MENU_BAR_COLOR));
}

static void draw_header_bar(display_context_t display, const char* headerText) {
    int x = 0, y = 0, width = SCREEN_WIDTH, height = MENU_BAR_HEIGHT;
    graphics_draw_box(display, x, y, width, height, graphics_convert_color(MENU_BAR_COLOR));
    graphics_draw_text(display, MARGIN_PADDING, y+6, headerText);
}

static void draw_bottom_bar(display_context_t display) {
    // NOTE: Transparency only works if color mode is set to 32bit
    #if COLOR_TRANSPARENCY_ENABLED == 1
    graphics_draw_box_trans(display, 0, BOTTOM_BAR_Y, SCREEN_WIDTH - INFO_PANEL_WIDTH, BOTTOM_BAR_HEIGHT, graphics_convert_color(BOTTOM_BAR_COLOR));
    #else
    graphics_draw_box(display, 0, BOTTOM_BAR_Y, SCREEN_WIDTH - INFO_PANEL_WIDTH, BOTTOM_BAR_HEIGHT, graphics_convert_color(BOTTOM_BAR_COLOR));
    #endif
    
    //graphics_draw_sprite_trans(display, MARGIN_PADDING, BOTTOM_BAR_Y, a_button_icon);
    graphics_draw_text(display, MARGIN_PADDING + 32, BOTTOM_BAR_Y + BOTTOM_BAR_HEIGHT/2 - 4, "Load ROM");
}

// Janky lol
char* loadingText[] = { "Loading", "Loading.", "Loading..", "Loading..." };
int loadingTextIndex = 0;
const int loadingBoxWidth = 128;
const int loadingBoxHeight = 32;
int counter = 0;
void animate_progress_spinner(display_context_t display) {
    // Border
    graphics_draw_box(display, 
        SCREEN_WIDTH / 2 - (loadingBoxWidth+4) / 2, 
        SCREEN_HEIGHT / 2 - (loadingBoxHeight+4) / 2, 
        loadingBoxWidth+4, 
        loadingBoxHeight+4, 
        graphics_convert_color(WHITE_COLOR)
    );

    // Actual box
    graphics_draw_box(display, 
        SCREEN_WIDTH / 2 - loadingBoxWidth / 2, 
        SCREEN_HEIGHT / 2 - loadingBoxHeight / 2, 
        loadingBoxWidth, 
        loadingBoxHeight, 
        graphics_convert_color(LOADING_BOX_COLOR)
    );

    // Text
    graphics_draw_text(display, (SCREEN_WIDTH / 2) - (MARGIN_PADDING * 4), SCREEN_HEIGHT / 2, loadingText[loadingTextIndex]);    

    if (counter > 60) {
        counter = 0;
        loadingTextIndex++;
        if (loadingTextIndex >= 4) {
            loadingTextIndex = 0;
        }
    }
    counter++;
}

void update_spinner( int ovfl ) {
    if (g_isLoading) {
        loadingTextIndex++;
        if (loadingTextIndex >= 4) {
            loadingTextIndex = 0;
        }
    }
}

int ls(const char *dir) {

    // If we couldn't init dfs (on an emulator?) just add some dummy data
    if (IS_EMULATOR) {
        int num_entries = 0;
        g_fileEntries[num_entries++] = "GoldenEye 007 (U) [!].z64";
        g_fileEntries[num_entries++] = "Doom 64 (USA) (Rev 1).z64";
        g_fileEntries[num_entries++] = "Mario Tennis (USA).z64";
        g_fileEntries[num_entries++] = "Super Mario 64 (U) [!].z64";
        return num_entries;
    }

    FRESULT fr; /* Return value */
    char const *p_dir = dir;

    DIR dj;      /* Directory object */
    FILINFO fno; /* File information */
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    fr = f_findfirst(&dj, &fno, p_dir, "*");
    
    if (FR_OK != fr) {
        printf("f_findfirst error: (%d)\n", fr);
        waitForStart();
        return 0;
    }
	int num_entries = 0;
    while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
        /* Create a string that includes the file name, the file size and the
         attributes string. */
        const char *pcWritableFile = "writable file",
                   *pcReadOnlyFile = "read only file",
                   *pcDirectory = "directory";
        const char *pcAttrib;
        /* Point pcAttrib to a string that describes the file. */
        if (fno.fattrib & AM_DIR) {
            pcAttrib = pcDirectory;
        } else if (fno.fattrib & AM_RDO) {
            pcAttrib = pcReadOnlyFile;
        } else {
            pcAttrib = pcWritableFile;
        }
        /* Create a string that includes the file name, the file size and the
         attributes string. */

        printf("%s [%s] [size=%llu]\n", fno.fname, pcAttrib, fno.fsize);
        if (fno.fname[0] != '.') {
            g_fileEntries[num_entries] = malloc(sizeof(char*) * FILE_NAME_MAX_LENGTH);
            g_fileSizes[num_entries] = fno.fsize / 1024 / 1024;
		    sprintf(g_fileEntries[num_entries++], "%s\n", fno.fname);
            
        } else {
            printf("Skipping file\n");
        }

        fr = f_findnext(&dj, &fno); /* Search for next item */
    }
    f_closedir(&dj);

    printf("num_entries %d\n", num_entries);

	return num_entries;
}
// #endif

/*
 * Init display and controller input then render a list of strings, showing currently selected
 */
static void show_list(void) {    

    // Fetch the root contents
    g_fileEntries = malloc(sizeof(char*) * FILE_ENTRIES_BUFFER_SIZE); // alloc the buffer
    g_fileSizes = malloc(sizeof(int) * FILE_ENTRIES_BUFFER_SIZE);

    // Fetch the root directory contents
	NUM_ENTRIES = ls("/");

    char* menuHeaderText = malloc(sizeof(char) * 128);
    sprintf(menuHeaderText, "DREAMDrive OS (git rev %08x)\t\t\t\t%d Files", GIT_REV, NUM_ENTRIES);

    // display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_320x240, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    display_init(RESOLUTION_512x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_512x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_512x480, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE); //Jumpy and janky, do not use

    // char debugTextBuffer[100];
    g_isRenderingMenu = true;
	int currently_selected = 0;
	int first_visible = 0;
	int max_on_screen = calculate_num_rows_per_page();

    timer_init();
    bool createLoadingTimer = false;

	while (1) {
		static display_context_t display = 0;
        
		/* Grab a render buffer */
		while (!(display = display_lock())) ;

		/* Clear the screen */
		graphics_fill_screen(display, 0);

		/* Draw top header bar */
		draw_header_bar(display, (const char*) menuHeaderText);

		/* Render the list of file */
		render_list(display, currently_selected, first_visible, max_on_screen);

        /* Render info about the currently selected rom including box art */
        render_info_panel(display, currently_selected);

        /* A little debug text at the bottom of the screen */
        //snprintf(debugTextBuffer, 100, "currently_selected=%d, first_visible=%d, max_per_page=%d", currently_selected, first_visible, max_on_screen);
        //graphics_draw_text(display, 5, 230, debugTextBuffer);

        draw_bottom_bar(display);

        if (g_isLoading) {
            animate_progress_spinner(display);
        }

        if (g_sendingSelectedRom) {
            // check the busy register
            uint16_t sdBusy = pc64_sd_wait_single();
            if (sdBusy == 0) {
                graphics_draw_box(display, SCREEN_WIDTH - 8, 3, 5, 5, graphics_convert_color(WHITE_COLOR));
                g_isLoading = false;
                g_sendingSelectedRom = false;
                wait_ms(300);
                // start boot
                bootRom(display, 1);
            } else {
                graphics_draw_box(display, SCREEN_WIDTH - 8, 3, 5, 5, graphics_convert_color(SELECTION_COLOR));
            }
            char b[] = "                ";
            // sprintf(b, "busy: %d", sdBusy);
            // graphics_draw_text(display, 30, 130, b);
        }

        /* Force the backbuffer flip */
        display_show(display);

        // If we are loading, then don't allow anymore input
        if (g_isLoading) {
            continue;
        }

		/* Grab controller input and move the selection up or down */
		controller_scan();
		struct controller_data keys = get_keys_down();
		int mag = 0;
		if (keys.c[0].down) {
			mag = 1;
		} else if (keys.c[0].up) {
			mag = -1;
		} else if (keys.c[0].A) {
            // Load the selected from
            loadRomAtSelection(currently_selected);
            createLoadingTimer = true;
            if (createLoadingTimer) {
                createLoadingTimer = false;
                //new_timer(TIMER_TICKS(1000000 / 10), TF_CONTINUOUS, update_spinner);
            }
        } else if (keys.c[0].right) {
            // page forward
            //mag = max_on_screen; // TODO something more sophisticated than this, because we need to handle partial
        } else if (keys.c[0].left) {
            // page backward
            //mag = -max_on_screen; // TODO something more sophisticated than this, because we need to handle partial
        }

		if ((mag > 0 && currently_selected + mag < NUM_ENTRIES) || (mag < 0 && currently_selected > 0)) {
			currently_selected += mag;
        }

        // If we have moved the cursor to an entry not yet visible on screen, move first_visible as well
        if ((mag > 0 && currently_selected >= (first_visible + max_on_screen)) || (mag < 0 && currently_selected < first_visible && currently_selected >= 0)) {
            first_visible += mag;
        }
    }
}

typedef struct
{
    uint32_t type;
    char filename[MAX_FILENAME_LEN+1];
} direntry_t;

char dir[512] = "rom://";

direntry_t *populate_dir(int *count)
{
    /* Grab a slot */
    direntry_t *list = malloc(sizeof(direntry_t));
    *count = 1;

    /* Grab first */
    dir_t buf;
    int ret = dir_findfirst(dir, &buf);
    

    if( ret != 0 ) 
    {
        /* Free stuff */
        free(list);
        *count = 0;

        /* Dir was bad! */
        return 0;
    }

    /* Copy in loop */
    while( ret == 0 )
    {
        list[(*count)-1].type = buf.d_type;
        strcpy(list[(*count)-1].filename, buf.d_name);

        printf("%s\n", list[(*count)-1].filename);

        /* Grab next */
        ret = dir_findnext(dir,&buf);

        if( ret == 0 )
        {
            (*count)++;
            list = realloc(list, sizeof(direntry_t) * (*count));
        }
    }

    // if(*count > 0)
    // {
    //     /* Should sort! */
    //     qsort(list, *count, sizeof(direntry_t), compare);
    // }

    return list;
}

long filesize( FILE *pFile )
{
    fseek( pFile, 0, SEEK_END );
    long lSize = ftell( pFile );
    rewind( pFile );
    return lSize;
}

sprite_t *read_sprite( const char * const spritename )
{
    printf("reading sprite %s ", spritename);
    int fp = dfs_open(spritename);

    if( fp )
    {
        sprite_t *sp = malloc( dfs_size( fp ) );
        int ret = dfs_read(sp, 1, dfs_size(fp), fp);
        dfs_close(fp);

        if (ret == DFS_ESUCCESS && sp) {
            printf("height: %d, width: %d\n", sp->height, sp->width);
        } else {
            printf("...Error loading sprite: %d\n", ret);
        }

        return sp;
    }
    else
    {
        printf("Missing FP[%d] for file %s\n", fp, spritename);
        return 0;
    }
}

sprite_t* load_sprite_from_sd(const char* const spritename) {
    if (!g_isRenderingMenu) {
    printf("loading sprite from sd : %s\n", spritename);
    }
    FILE *fp = fopen(spritename, "r");
    if (fp) {
        const long size = filesize(fp);
        sprite_t *sp = malloc(size);
        fread(sp, 1, size, fp);
        fclose(fp);

        if (!g_isRenderingMenu) {
        printf("size: %ld, width: %d, height: %d\n", size, sp->width, sp->height);
        }

        return sp;
    } else {
        if (!g_isRenderingMenu) {
        printf("Missing fp for %s from SD.\n", spritename);
        }
        return 0;
    }
}

/*
 *
 * Read a rom's header and find its serial number
 * 
 * Example usage:
    char* serialNumber[5]; // make sure to include an extra for the null character
    int success = read_rom_header_serial_number(serialNumber, "rom://goldeneye.z64");
    if (success == 0) {
        success!
    } else if (success == -1) {
        not a rom file
    }
 *
 */
int read_rom_header_serial_number(char* buf, const char* const filename) {
    FILE* fp = fopen(filename, "r");

    if (!fp) {
        fclose(fp);
        if (!g_isRenderingMenu) {
        printf("Unable to open file %s\n", filename);
        }
        return -1;
    }

    // TODO check if a valid rom (maybe using the janky method of checking for 80 37 12 40 bytes)

    // Doing a filesize on rom files keep returning -1 size... wth?
    // const long size = filesize(fp);
    // if (size < 1024) {
    //     printf("failed to read rom %s\nSize: %ld\n", filename, size);
    //     return -1;
    // }

    int r = fseek(fp, 0x3B, SEEK_SET);
    if (r != 0) {
        fclose(fp);
        if (!g_isRenderingMenu) {
        printf("Unable to seek rom file.\n");
        }
        return -1;
    }

    int numRead = fread(buf, 1, 4, fp);
    if (numRead == 0) {
        fclose(fp);
        if (!g_isRenderingMenu) {
        printf("Unable to read rom file header serial number.\n");
        }
        return -1;
    }

    fclose(fp);

    buf[4] = '\0';
    
    return 0;
}

// Loads box art into cache and returns the index it was added to, -1 if it couldn't be added
int load_boxart_for_rom(char* filename) {
    memset(temp_serial, 0, 5);
    int ret = read_rom_header_serial_number(temp_serial, filename);

    if (ret != 0) {
        thumbnail_loaded = false;
        return - 1;
    }

    if(!g_isRenderingMenu) {
    printf("Loading sprite...\n");
    }
    
    memset(temp_spritename, 0, 256);
    sprintf(temp_spritename, "sd:/N64/sprites/%s.sprite", temp_serial);

    current_thumbnail = load_sprite_from_sd(temp_spritename);
    thumbnail_loaded = true;

    return 0;
}

static void init_sprites(void) {
    printf("init sprites\n");

    // a_button_icon = read_sprite("a-button-icon.sprite");

    if (IS_EMULATOR) {
        // g_thumbnail_cache = malloc(sizeof(sprite_t*) * 4); // alloc the buffer
        LOAD_BOX_ART = true;
        int thumbnailCount = 1; //sizeof(g_thumbnail_table) / sizeof(*g_thumbnail_table);
        for(int i = 0; i < thumbnailCount; i++) {
            // fp = dfs_open(g_thumbnail_table[i]);
            // printf("%s opened? %d\n", g_thumbnail_table[i], fp);

            // sprite_t* sprite = malloc( dfs_size( fp ) );
            // dfs_read(sprite, 1, dfs_size(fp), fp);
            // g_thumbnail_cache[i] = sprite;

            // dfs_close( fp );

            printf("Loading thumbnail: %s\n", g_thumbnail_table[i]);
            // g_thumbnail_cache[i] = read_sprite(g_thumbnail_table[i]);
            // printf("height: %d, width: %d\n", g_thumbnail_cache[i]->height, g_thumbnail_cache[i]->width);
        }
    } else {
        LOAD_BOX_ART = true;
        // load_boxart_for_rom("sd:/Doom 64 (USA) (Rev 1).z64");
        if(load_boxart_for_rom("sd:/GoldenEye 007 (U) [!].z64") != 0) {
            printf("Failed to load boxart\n");
            LOAD_BOX_ART = false;
            thumbnail_loaded = false;
        } else {
            printf("Loaded box art\n");
        }

        waitForStart();
    }

    // char buf[512];
    // FIL fil;

	// FRESULT fr = f_open(&fil, "GoldenEye 007 (U) [!].z64", FA_OPEN_EXISTING | FA_READ);

    // int MB = 1 * 1024 * 1024;
    // uint len = 0;
	// int total = 0;
    // unsigned long t0 = get_ticks_ms();//to_us_since_boot(get_absolute_time());
    // do {
    //     fr = f_read(&fil, buf, sizeof(buf), &len);
    //     total += len;
    // } while(len > 0 || total < MB);

    // uint64_t t1 = get_ticks_ms();
	// uint32_t delta = (t1 - t0) / 1000;
	// uint32_t kBps = (uint32_t) ((float)(total / 1024.0f) / (float)(delta / 1000.0f));
    
	// printf("Read %d bytes in %ld ms (%ld kB/s)\n\n\n", total, delta, kBps);
    // f_close(&fil);    
    // waitForStart();

    printf("done!\n");
}

void start_shell(void) {
    controller_init();

    if (usb_initialize()) {
        char usbcart = usb_getcart();
        printf("USB Cart %d\n", usbcart);
        switch (usbcart)
        {
        case CART_64DRIVE:
        case CART_EVERDRIVE:
        case CART_PC64:
            IS_EMULATOR = false;
            break;
        default:
            IS_EMULATOR = true;
        }
    } else {
        IS_EMULATOR = true;
    }

    // IS_EMULATOR = true;

    if (IS_EMULATOR) {
        printf("Running in an emulator?\n");
    } else {
        printf("Running on real hardware?\n");
    }

    // Alloc global holding variables
    temp_serial = malloc(sizeof(char) * 5);
    temp_spritename = malloc(sizeof(char) * 256);

    int ret = dfs_init(DFS_DEFAULT_LOCATION);
    if (ret != DFS_ESUCCESS) {
        printf("Unable to init filesystem. ret: %d\n", ret);
		printf("git rev %08x\n", GIT_REV);
    } else if (!IS_EMULATOR) {
        printf("Initing sd access");
        wait_ms( 100 );

		// Try to init the sd card
        bool retrying = true;
        while (retrying) {
            if(!debug_init_sdfs("sd:/", -1)) {
                printf("Unable to access SD Card on Picocart64. Try again(Start)? Abort(B)\n");
                while (true) {
                    controller_scan();
                    struct controller_data keys = get_keys_pressed();
                    if (keys.c[0].start) {
                        retrying = true;
                        wait_ms(300);
                        break;
                    } else if (keys.c[0].B) {
                        retrying = false;
                        wait_ms(300);
                        break;
                    }
                }
            } else {
                printf("\nSD Init... SUCCESS!!\n");
                retrying = false;
            }
        }

        console_clear();

        // waitForStart();

        /* Load sprites for shell from the filesystem */

        // printf("Loading selected rom 'DOOM 64'\n");
        // loadRomAtSelection(0);
        // printf("Start waiting for sd_busy register...\n");
        
        // // wait before trying to poll busy register just to be sure everything is set
        // // this is likely overkill 
        // wait_ms(1000); 

        // printf("Waiting for rom load...\n");
        // pc64_sd_wait();
        // pc64_sd_wait();

        // printf("busy flag released, waiting for cart to settle...\n");
        // wait_ms(5000);
        
        // g_isLoading = false;
        // g_sendingSelectedRom = false;

        // // start boot
        // // display_init(RESOLUTION_512x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
        // static display_context_t display = 0;
		// /* Grab a render buffer */
		// // while (!(display = display_lock())) ;
        // printf("Booting rom!\n");
        // bootRom(display, 1);
    
        /* Starts the shell by rendering the list of files from the SD card*/
    }

    init_sprites();
    show_list();

    // #endif
}
