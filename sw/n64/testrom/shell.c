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
#define COLOR_TRANSPARENCY_ENABLED 1

 // TODO: It is likely a directory may contain thousands of files
 // Modify the ls function to only buffer a portion of files (up to some MAX)
 #define FILE_ENTRIES_BUFFER_SIZE 256
 #define FILE_NAME_MAX_LENGTH (MAX_FILENAME_LEN+1)
 #define MAX_DIRECTORY_TRAVERSAL_DEPTH 10

 #define RENDER_CONSOLE_ONLY 0

/*
 * Need some kind of lookup table or function that can find a thumbnail from a rom name, including the correct region version of the boxart
 * SD Card should contain
 * firmware folder
 *      thumbnails
 *          system(e.g. N64)
 *              thumbnail.png (Maybe use the serial number?) -- Rename the thumbnails by cross referencing the titles to the serial from the dat file
 *          
 */
// typedef struct {
//     /* Rom name can be extracted from the rom header at address 0x20 
//      * Probably better to use that to match thumbnails and display the title
//      */
//     char filename[MAX_FILENAME_LEN+1];
//     char title[64];
//     int filesize;
//     sprite_t* boxart;
//     // region?
//     // release year?
//     // eeprom type/size?
//     // byte order?
//     // ipl checksums?
//     // crc hash?
// } rom_file_info_t;
// rom_file_info_t** g_romFileInfoBuffer;

// Files and such
typedef enum {
    TYPE_FILE = 0,   /*(0) Object is a file */
    TYPE_ROM,        /*(1) Is a valid rom file */
    TYPE_DIRECTORY   /*(2) Is a directory */
} FILE_INFO_TYPE;

typedef struct {
    // DIR* directory; // pointer to parent directory
    char filename[256];
    uint64_t filesize;
    FILE_INFO_TYPE type; // Type
} file_info_t;

// To keep place when traversing the directories
// TODO FINISH implementing
typedef struct {
    int currentlySelectedListItem;
    int firstVisibleItemInList;
} menu_page_info_t;

char** g_directory_breadcumb;
int g_current_directory_breadcrumb_index = -1;
file_info_t** g_current_dir_entries;
int g_current_max_files_loaded = 0;
int g_currentPage = 0; // variable for file list pagination
bool g_isFirstDirectoryLoad = true;

// Misc global variables
bool IS_EMULATOR = 0;
bool LOAD_BOX_ART = 0;
bool thumbnail_loaded = false;
int NUM_ENTRIES = 0;
bool g_sendingSelectedRom = false;
int g_lastSelection = -1;
bool g_isLoading = false;
bool g_isRenderingMenu = false;
int g_current_selected_list_item = 0;
int g_first_visible_list_item = 0;

// Global Buffers
static sprite_t* current_thumbnail;
char* temp_serial;
char* temp_spritename;
char g_infoPanelTextBuf[300];
// sprite_t** g_thumbnail_cache;

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
    sprintf(fileToLoad, "%s", g_current_dir_entries[selection]->filename);

    // Write the file name to the cart buffer
    uint32_t len_aligned32 = (strlen(fileToLoad) + 3) & (-4);
    data_cache_hit_writeback_invalidate(fileToLoad, len_aligned32);
    pi_write_raw(fileToLoad, PC64_BASE_ADDRESS_START, 0, len_aligned32);

    uint32_t sdSelectRomFilenameLength = strlen(fileToLoad);
    io_write(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_SELECT_ROM, sdSelectRomFilenameLength);

    g_isLoading = true;

    wait_ms(100);
}

static uint16_t pc64_sd_wait_single() {
    uint32_t isBusy = io_read(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_BUSY);
    return isBusy;
}

static uint8_t pc64_sd_wait() {
    uint32_t timeout = 0;
    uint32_t isBusy __attribute__((aligned(8)));
	isBusy = 1;

    // Wait until the cartridge interface is ready
    do {
        // returns 1 while sd card is busy
		isBusy = io_read(PC64_CIBASE_ADDRESS_START + PC64_REGISTER_SD_BUSY);
        
        // Took too long, abort
        if((timeout++) > 10000000) {
			fprintf(stdout, "SD_WAIT timed out. isBusy: %ld\n", isBusy);
			return -1;
		}
    }
    while(isBusy != 0);
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

void silentWaitForStart() {
    printf("...\n");
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
		graphics_draw_text(display, x, y, g_current_dir_entries[row]->filename);

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
        memset(g_infoPanelTextBuf, 0, 300);
        memset(temp_serial, 0, 5);

        // TODO fix the below code, loading thumbnails is causing a crash.
        // Free the last thumbnail
        if (thumbnail_loaded) {
            free(current_thumbnail);
            thumbnail_loaded = false;
        }

        // Try to load a thumbnail, if this isn't a rom, don't load box art
        if(load_boxart_for_rom(g_current_dir_entries[currently_selected]->filename) != 0) {
            LOAD_BOX_ART = false;
        } else {
            LOAD_BOX_ART = true;
        }

        // TODO if part of the string is longer than the number of characters that can fit in in the info panel width, split or clip it
        sprintf(g_infoPanelTextBuf, "%s\n%s\nSize: ?M\nCountry xxx\nReleased xxxx", g_current_dir_entries[currently_selected]->filename, temp_serial);
        if (!g_isRenderingMenu) {
            printf("rom: %s, serial: %s\n",g_current_dir_entries[currently_selected]->filename, temp_serial);
        }

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
    
    // graphics_draw_sprite_trans(display, MARGIN_PADDING, BOTTOM_BAR_Y, a_button_icon);
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

// Go up a directory to this directories parent folder
void go_up() {
    if (g_current_directory_breadcrumb_index == 0) {
        if (!g_isRenderingMenu) {
            printf("Already at root!\n");
        }
        return;
    }

    if (!g_isRenderingMenu) {
        printf("Go up\n");
    }
    g_current_directory_breadcrumb_index--;
    
    // If we are going to root, skip the path building
    if (g_current_directory_breadcrumb_index == 0) {
        #if RENDER_CONSOLE_ONLY == 1
        printf("going to root %d...\n", g_current_directory_breadcrumb_index);
        #endif
        cd("/", true);

    } else {
        #if RENDER_CONSOLE_ONLY == 1
        printf("constructing path %d...\n", g_current_directory_breadcrumb_index);
        #endif
        // Max path length = max_filename size * max_number_of_directories + slashes for each directory
        char* path = malloc(sizeof(char) * FILE_NAME_MAX_LENGTH * MAX_DIRECTORY_TRAVERSAL_DEPTH + (MAX_DIRECTORY_TRAVERSAL_DEPTH));
        int offset = 0;
        for (int i = 1; i < g_current_directory_breadcrumb_index; i++) {
            sprintf(path + offset, "/%s", g_directory_breadcumb[i]);
            offset = strlen(path)-1; // don't include the terminating character
        }

        // Change directories to parent
        cd(path, true);
    }
}

void cd(const char* dir, bool isPop) {
    #if RENDER_CONSOLE_ONLY == 1
    printf("cd %s, isPop:%d\n", dir, isPop);
    #endif

    if (g_isFirstDirectoryLoad) {
        // First load
        // Setup the breadcrumb array
        g_directory_breadcumb = malloc(sizeof(char*) * MAX_DIRECTORY_TRAVERSAL_DEPTH);

        // Allocate memory for a number of file entries
        g_current_dir_entries = malloc(sizeof(file_info_t*) * FILE_ENTRIES_BUFFER_SIZE);
        g_isFirstDirectoryLoad = false;
    }

    // Only want to set directory name entries when we are going somewhere "new"
    // ie not back
    if (!isPop) {
        // Set the directory
        g_directory_breadcumb[g_current_directory_breadcrumb_index] = malloc(sizeof(char*) * FILE_NAME_MAX_LENGTH);
        sprintf(g_directory_breadcumb[g_current_directory_breadcrumb_index++], "%s", dir);
    }

    // reset some state variables
    g_lastSelection = -1;
    g_current_selected_list_item = 0;
    g_first_visible_list_item = 0;
    
    // Populate the file list
    NUM_ENTRIES = ls(dir);
}

int ls(const char *dir) {

    #if RENDER_CONSOLE_ONLY == 1
    printf("breadcrumb2=%d\n", g_current_directory_breadcrumb_index);
    #endif

    char const *p_dir = dir;
    FRESULT fr; /* Return value */
    DIR dj;      /* Directory object */
    FILINFO fno; /* File information */

    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    fr = f_findfirst(&dj, &fno, p_dir, "*");
    
    if (FR_OK != fr) {
        printf("f_findfirst error: (%d)\n", fr);
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
        FILE_INFO_TYPE fileType = TYPE_FILE;
        /* Point pcAttrib to a string that describes the file. */
        if (fno.fattrib & AM_DIR) {
            pcAttrib = pcDirectory;
            fileType = TYPE_DIRECTORY;
        } else if (fno.fattrib & AM_RDO) {
            pcAttrib = pcReadOnlyFile;
        } else {
            pcAttrib = pcWritableFile;
        }
        
        if (fno.fname[0] != '.') {
            // Create the file entry if needed
            file_info_t* entry;
            if (num_entries >= g_current_max_files_loaded) {
                entry = malloc(sizeof(file_info_t));
                g_current_max_files_loaded++;
            } else {
                entry = g_current_dir_entries[num_entries];
            }

            // Populate the file entry
            entry->filesize = fno.fsize;
            entry->type = fileType;
		    sprintf(entry->filename, "%s", fno.fname);

            // TODO, can we check if the file is a rom here?

            /* Create a string that includes the file name, the file size and the
            attributes string. */
            if (!g_isRenderingMenu) {
                printf("%s [%d] [size=%llu]\n", entry->filename, entry->type, entry->filesize);
            }

            // Set the entry
            g_current_dir_entries[num_entries++] = entry;
            
        } else {
            // if (!g_isRenderingMenu) {
            //     printf("Skipping file\n");
            // }
        }

        fr = f_findnext(&dj, &fno); /* Search for next item */
    }
    f_closedir(&dj);

    if (!g_isRenderingMenu) {
        printf("num_entries %d\n", num_entries);
    }

	return num_entries;
}
// #endif

/*
 * Init display and controller input then render a list of strings, showing currently selected
 */
static void show_list(void) {    

    // Fetch the root contents
    cd("/", false); // cd into the root

    char* menuHeaderText = malloc(sizeof(char) * 128);
    sprintf(menuHeaderText, "DREAMDrive OS (git rev %08x)\t\t\t\t%d Files", GIT_REV, NUM_ENTRIES);

    // display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_320x240, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    
    #if RENDER_CONSOLE_ONLY == 0
    display_init(RESOLUTION_512x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    g_isRenderingMenu = true;
    #endif

    // display_init(RESOLUTION_512x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_512x480, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE); //Jumpy and janky, do not use

    // char debugTextBuffer[100];
	int max_on_screen = calculate_num_rows_per_page();

    timer_init();
    bool createLoadingTimer = false;

	while (1) {
		static display_context_t display = 0;
        
        #if RENDER_CONSOLE_ONLY == 0
		/* Grab a render buffer */
		while (!(display = display_lock())) ;
        

		/* Clear the screen */
		graphics_fill_screen(display, 0);

		/* Draw top header bar */
		draw_header_bar(display, (const char*) menuHeaderText);

		/* Render the list of file */
		render_list(display, g_current_selected_list_item, g_first_visible_list_item, max_on_screen);

        /* Render info about the currently selected rom including box art */
        render_info_panel(display, g_current_selected_list_item);

        /* A little debug text at the bottom of the screen */
        //snprintf(debugTextBuffer, 100, "g_current_selected_list_item=%d, g_first_visible_list_item=%d, max_per_page=%d", g_current_selected_list_item, g_first_visible_list_item, max_on_screen);
        //graphics_draw_text(display, 5, 230, debugTextBuffer);

        draw_bottom_bar(display);

        if (g_isLoading) {
            animate_progress_spinner(display);
        }
        #endif

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
            // char b[] = "                ";
            // sprintf(b, "busy: %d", sdBusy);
            // graphics_draw_text(display, 30, 130, b);
        }

        #if RENDER_CONSOLE_ONLY == 1
        console_render(); // use if not rendering with display, also disable g_isRenderingMenu
        #else
        /* Force the backbuffer flip */
        display_show(display);
        #endif

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
            switch (g_current_dir_entries[g_current_selected_list_item]->type) {
                case TYPE_FILE:
                    break;
                case TYPE_ROM:
                    // Load the selected from
                    loadRomAtSelection(g_current_selected_list_item);
                    break;
                case TYPE_DIRECTORY:
                    cd(g_current_dir_entries[g_current_selected_list_item]->filename, false);
                    break;
            }
            
            // createLoadingTimer = true;
            // if (createLoadingTimer) {
            //     createLoadingTimer = false;
                // this seems to bind up rom loading, try again later
                // new_timer(TIMER_TICKS(1000000 / 10), TF_CONTINUOUS, update_spinner);
            // }
        } else if (keys.c[0].B) {
            go_up();

        } else if (keys.c[0].right) {
            // page forward
            //mag = max_on_screen; // TODO something more sophisticated than this, because we need to handle partial
        } else if (keys.c[0].left) {
            // page backward
            //mag = -max_on_screen; // TODO something more sophisticated than this, because we need to handle partial
        }

		if ((mag > 0 && g_current_selected_list_item + mag < NUM_ENTRIES) || (mag < 0 && g_current_selected_list_item > 0)) {
			g_current_selected_list_item += mag;
        }

        // If we have moved the cursor to an entry not yet visible on screen, move first_visible as well
        if ((mag > 0 && g_current_selected_list_item >= (g_first_visible_list_item + max_on_screen)) || (mag < 0 && g_current_selected_list_item < g_first_visible_list_item && g_current_selected_list_item >= 0)) {
            g_first_visible_list_item += mag;
        }

        #if RENDER_CONSOLE_ONLY == 1
        if (mag != 0) {
            printf("->%s\n", g_current_dir_entries[g_current_selected_list_item]->filename); // print current selection
        }
        #endif
        
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
        int totalRead = 0;
        sprite_t *sp = malloc(size);
        fread(sp, 1, size, fp);
        fclose(fp);

        if (!g_isRenderingMenu) {
        printf("bytesRead: %d, size: %ld, width: %d, height: %d\n", totalRead, size, sp->width, sp->height);
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
    // printf("Loading header info for: %s\n", filename);
    FILE* fp = fopen(filename, "r");

    if (!fp) {
        if (!g_isRenderingMenu) {
        printf("Unable to open file %s\n", filename);
        }
        fclose(fp);
        return -1;
    }

    // This is really slow and locks up the UI, so only check and print if using console
    if (!g_isRenderingMenu) {
        const long size = filesize(fp);
        if (size < 64) {
            if(!g_isRenderingMenu) {
                printf("failed to read rom %s\nSize: %ld\n", filename, size);
            }
        }
        printf("Size: %ld\n", size);
    }

    uint8_t* b = malloc(64);    
    uint numRead = fread(b, 1, 64, fp);
    if (numRead == 0) {
        if (!g_isRenderingMenu) {
            printf("Unable to read rom file header serial number.\n");
        }
        fclose(fp);
        return -1;
    }
    fclose(fp);

    // janky method of checking for 80 37 12 40 bytes
    if (b[0] != 0x80 && b[1] != 0x37 && b[2] != 0x12 && b[3] != 0x40) {
        if (!g_isRenderingMenu) {
            printf("Not a rom file.\n");
        }
        return -1;
    }

    buf[0] = b[0x3B];
    buf[1] = b[0x3C];
    buf[2] = b[0x3D];
    buf[3] = b[0x3E];
    buf[4] = '\0';

    free(b);
    
    return 0;
}

// Loads box art into cache and returns the index it was added to, -1 if it couldn't be added
int load_boxart_for_rom(char* filename) {
    memset(temp_serial, 0, 8);
    char* prefixedFilename = malloc(256);
    sprintf(prefixedFilename, "sd:/%s", filename);
    int ret = read_rom_header_serial_number(temp_serial, prefixedFilename);
    free(prefixedFilename);

    if (ret != 0) {
        thumbnail_loaded = false;
        return - 1;
    }

    if(!g_isRenderingMenu) {
        printf("Loading sprite...\n");
    }
    
    memset(temp_spritename, 0, 256);
    // sprintf(temp_spritename, "sd:/N64/sprites/%s.sprite", temp_serial);
    sprintf(temp_spritename, "sd:/N64/boxart_sprites_32/%s.sprite", temp_serial);

    current_thumbnail = load_sprite_from_sd(temp_spritename);
    thumbnail_loaded = true;

    return 0;
}

static void init_sprites(void) {
    printf("init sprites\n");

    // a_button_icon = read_sprite("a-button-icon.sprite");

    if (IS_EMULATOR) {
        // g_thumbnail_cache = malloc(sizeof(sprite_t*) * 4); // alloc the buffer
        // LOAD_BOX_ART = true;
        // int thumbnailCount = 1; //sizeof(g_thumbnail_table) / sizeof(*g_thumbnail_table);
        // for(int i = 0; i < thumbnailCount; i++) {
        //     // fp = dfs_open(g_thumbnail_table[i]);
        //     // printf("%s opened? %d\n", g_thumbnail_table[i], fp);

        //     // sprite_t* sprite = malloc( dfs_size( fp ) );
        //     // dfs_read(sprite, 1, dfs_size(fp), fp);
        //     // g_thumbnail_cache[i] = sprite;

        //     // dfs_close( fp );

        //     printf("Loading thumbnail: %s\n", g_thumbnail_table[i]);
        //     // g_thumbnail_cache[i] = read_sprite(g_thumbnail_table[i]);
        //     // printf("height: %d, width: %d\n", g_thumbnail_cache[i]->height, g_thumbnail_cache[i]->width);
        // }
    } else {
        LOAD_BOX_ART = true;
        // printf("Loading box art\n");
        // int success = load_boxart_for_rom("GoldenEye 007 (U) [!].z64");
        // silentWaitForStart();
        // printf("box art load success: %d. thumbnail_loaded:%d\n", success, thumbnail_loaded);
    }

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

    if (IS_EMULATOR) {
        printf("Running in an emulator?\n");
    } else {
        printf("Running on real hardware?\n");
    }

    // printf("PC64_CIBASE_ADDRESS_START: %08x\n", PC64_CIBASE_ADDRESS_START);
    pc64_debug_print();

    eeprom_type_t eeprom = eeprom_present();
    uint8_t* eepromBuf = malloc(8);
    printf("EEPROM type %d\n", (int)eeprom);
    eeprom_read(7, eepromBuf);
    for(int i = 0; i < 8; i++) {
        printf("%02x ", eepromBuf[i]);
    }
    
    silentWaitForStart();

    // Alloc global holding variables
    temp_serial = malloc(sizeof(char) * 8);
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
    }

    init_sprites();
    show_list();
}
