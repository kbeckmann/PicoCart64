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

#include "git_info.h"
#include "shell.h"

// picocart64_shared
#include "pc64_regs.h"
#include "pc64_rand.h"
#include "n64_defs.h"
#include "pc64_utils.h"

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
 #define FILE_NAME_MAX_LENGTH 256
char** g_fileEntries; // Buffer for file entries
sprite_t** g_thumbnail_cache;
int g_currentPage = 0; // variable for file list pagination

// Some arrays for testing layout while using the emulator
// Thumbnails need to be cached for snappier menu feel
#if BUILD_FOR_EMULATOR 1
/**/
char* g_thumbnail_table[] = {
    "goldeneye.sprite", 
    "doom.sprite", 
    "mario_tennis.sprite", 
    "mario64.sprite"
};

int g_fileSizes[] = {
    12582912,
    8388608,
    16777216,
    8388608  
};

char* g_fileInfo[] = {
    "Goldeneye 007",
    "DOOM 64",
    "Mario Tennis",
    "Super Mario 64"  
};
/**/
#else
// TODO implement a more generic implementation of the file info stuff
#endif

int NUM_ENTRIES = 21;
bool g_sendingSelectedRom = false;

/* Layout */
#define INFO_PANEL_WIDTH (192 + (MARGIN_PADDING * 2)) // NEEDS PARENS!!! Seems the compiler doesn't evaluate the define before using it for other defines
#define FILE_PANEL_WIDTH (SCREEN_WIDTH - INFO_PANEL_WIDTH)

#define ROW_HEIGHT (12)
#define MARGIN_PADDING (10)
#define ROW_SELECTION_WIDTH (FILE_PANEL_WIDTH)
#define MENU_BAR_HEIGHT (15)
#define LIST_TOP_PADDING (MENU_BAR_HEIGHT + ROW_HEIGHT)
#define BOTTOM_BAR_HEIGHT (32)
#define BOTTOM_BAR_Y (SCREEN_HEIGHT - BOTTOM_BAR_HEIGHT)

// Menu sprites
sprite_t *a_button_icon;

/* Colors */
color_t MENU_BAR_COLOR = { .r = 0x82, .g = 0x00, .b = 0x2E, .a = 0x00 }; // 0x82002E, Berry
color_t BOTTOM_BAR_COLOR = { .r = 0x00, .g = 0x67, .b = 0xC7, .a = 0x55 }; // 0x82002E, Berry
color_t SELECTION_COLOR = { .r = 0x00, .g = 0x67, .b = 0xC7, .a = 0x00 }; // 0x0067C7, Bright Blue

void loadRomAtSelection(int selection) {
    g_sendingSelectedRom = true;

    char fileToLoad[256];
    strcpy(fileToLoad, g_fileEntries[selection]);
    int strIndex = 0;
    uint8_t valueToSend;
    do {
        valueToSend = fileToLoad[strIndex++];
        data_cache_hit_writeback_invalidate(&valueToSend, sizeof(valueToSend));
	    pi_write_raw(&valueToSend, PC64_CIBASE_ADDRESS_START, PC64_REGISTER_SD_SELECT_ROM, sizeof(valueToSend));
    } while (valueToSend != NULL);

    g_sendingSelectedRom = false;
}

void waitForStart() {
    printf("Start to continue...\n");
    while (true) {
            controller_scan();
            struct controller_data keys = get_keys_pressed();
            if (keys.c[0].start) {
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
static void render_list(display_context_t display, char **list, int currently_selected, int first_visible, int max_on_screen)
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
		if (row > NUM_ENTRIES) {
			break;
		}

		if (currently_selected == row) {
			/* Render selection box */
			graphics_draw_box(display, 0, y - 2, ROW_SELECTION_WIDTH, 10, graphics_convert_color(SELECTION_COLOR));

			/* Now render selection caret */
			// graphics_draw_text(display, MARGIN_PADDING / 2, y, "  >");

			/* Increase Margin Padding to account for added caret width */
			// x += MARGIN_PADDING;
		} else {
			x += MARGIN_PADDING;
		}

		// Render the list item
		graphics_draw_text(display, x, y, list[row]);

		/* If this is the last row and there are more files below, draw an indicator */
		if (row + 1 < NUM_ENTRIES && i + 1 >= max_on_screen) {
			graphics_draw_text(display, MARGIN_PADDING, y + 10, "...");
		}
	}

    int x0 = FILE_PANEL_WIDTH-1, y0 = MENU_BAR_HEIGHT, x1 = FILE_PANEL_WIDTH-1, y1 = SCREEN_HEIGHT;
    graphics_draw_line(display, x0, y0, x1, y1, graphics_convert_color(SELECTION_COLOR));
    graphics_draw_line(display, x0+1, y0, x1+1, y1, graphics_convert_color(BOTTOM_BAR_COLOR));
}

static void render_info_panel(display_context_t display, int currently_selected) {
    /*
    Discussion about things to display:
    It would be great to also see the config. Eeprom type/size. The byte order of the rom, ipl checksums, crc hash,
    */
    int x = FILE_PANEL_WIDTH + MARGIN_PADDING, y = MENU_BAR_HEIGHT + MARGIN_PADDING;
    
    graphics_draw_sprite(display, x, y, g_thumbnail_cache[currently_selected]);

    // Something might be wrong with this... Probably should use a global buffer and avoid creating new objects every time
    //y += 192
    // char buf[100];
    // sprintf(buf, "%s\nSize: %dM", g_fileInfo[currently_selected], g_fileSizes[currently_selected] / 1024 / 1024);
    // graphics_draw_text(display, x, 192, buf);
    
    // Display the currently selected rom info
    graphics_draw_text(display, x, 100, "Goldeneye 007\nSize: 12M\nUSA\nReleased 1997");

    /* Draw bottom menu bar for the info panel */
    graphics_draw_box(display, x - MARGIN_PADDING, BOTTOM_BAR_Y, INFO_PANEL_WIDTH, BOTTOM_BAR_HEIGHT, graphics_convert_color(MENU_BAR_COLOR));
}

static void draw_header_bar(display_context_t display, int fileCount) {
    int x = 0, y = 0, width = SCREEN_WIDTH, height = MENU_BAR_HEIGHT;
    graphics_draw_box(display, x, y, width, height, graphics_convert_color(MENU_BAR_COLOR));
    
    char menuHeaderBuffer[100];
    sprintf(menuHeaderBuffer, "PicoCart64 OS (git rev %08x)\t\t\t\t%d Files", GIT_REV, fileCount);
    graphics_draw_text(display, MARGIN_PADDING, y+6, menuHeaderBuffer);
}

static void draw_bottom_bar(display_context_t display) {
    // NOTE: Transparency only works if color mode is set to 32bit
    #if COLOR_TRANSPARENCY_ENABLED == 1
    graphics_draw_box_trans(display, 0, BOTTOM_BAR_Y, SCREEN_WIDTH - INFO_PANEL_WIDTH, BOTTOM_BAR_HEIGHT, graphics_convert_color(BOTTOM_BAR_COLOR));
    #else
    graphics_draw_box(display, 0, BOTTOM_BAR_Y, SCREEN_WIDTH - INFO_PANEL_WIDTH, BOTTOM_BAR_HEIGHT, graphics_convert_color(BOTTOM_BAR_COLOR));
    #endif
    
    graphics_draw_sprite_trans(display, MARGIN_PADDING, BOTTOM_BAR_Y, a_button_icon);
    graphics_draw_text(display, MARGIN_PADDING + 32, BOTTOM_BAR_Y + BOTTOM_BAR_HEIGHT/2 - 4, "Load ROM");
}

#if BUILD_FOR_EMULATOR == 1

int ls(const char *dir) {

    int num_entries = 0;
    g_fileEntries[num_entries++] = "GoldenEye 007 (U) [!].z64";
    g_fileEntries[num_entries++] = "Doom 64 (USA) (Rev 1).z64";
    g_fileEntries[num_entries++] = "Mario Tennis (USA).z64";
    g_fileEntries[num_entries++] = "Super Mario 64 (U) [!].z64";

    return num_entries;
}

#else
int ls(const char *dir) {
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

        if (fno.fname)

        printf("%s [%s] [size=%llu]\n", fno.fname, pcAttrib, fno.fsize);
        g_fileEntries[num_entries] = malloc(sizeof(char*) * FILE_NAME_MAX_LENGTH);
		sprintf(g_fileEntries[num_entries++], "%s [size=%llu]\n", fno.fname, fno.fsize);

        fr = f_findnext(&dj, &fno); /* Search for next item */
    }
    f_closedir(&dj);

    printf("num_entries %d\n", num_entries);

	return num_entries;
}
#endif

/*
 * Init display and controller input then render a list of strings, showing currently selected
 */
static void show_list(void) {    

    // Fetch the root contents
    g_fileEntries = malloc(sizeof(char*) * FILE_ENTRIES_BUFFER_SIZE); // alloc the buffer
    // TODO alloc any other buffers here as well
	NUM_ENTRIES = ls("/");

    // display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    display_init(RESOLUTION_512x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_320x240, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_512x240, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_512x480, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE); //Jumpy and janky, do not use

    char debugTextBuffer[100];
	int currently_selected = 0;
	int first_visible = 0;
	int max_on_screen = calculate_num_rows_per_page();
	while (1) {
		static display_context_t display = 0;

		/* Grab a render buffer */
		while (!(display = display_lock())) ;

		/* Clear the screen */
		graphics_fill_screen(display, 0);

		/* Draw top header bar */
		draw_header_bar(display, NUM_ENTRIES);

		/* Render the list of file */
		render_list(display, g_fileEntries, currently_selected, first_visible, max_on_screen);

        /* Render info about the currently selected rom including box art */
        render_info_panel(display, currently_selected);

        /* A little debug text at the bottom of the screen */
        //snprintf(debugTextBuffer, 100, "currently_selected=%d, first_visible=%d, max_per_page=%d", currently_selected, first_visible, max_on_screen);
        //graphics_draw_text(display, 5, 230, debugTextBuffer);

        draw_bottom_bar(display);

        /* Force the backbuffer flip */
        display_show(display);

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
        } else if (keys.c[0].right) {
            // page forward
        } else if (keys.c[0].left) {
            // page backward
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

int filesize( FILE *pFile )
{
    fseek( pFile, 0, SEEK_END );
    int lSize = ftell( pFile );
    rewind( pFile );

    return lSize;
}

sprite_t *read_sprite( const char * const spritename )
{
    //FILE *fp = fopen(spritename, "r");
    int fp = dfs_open(spritename);

    if( fp )
    {
        sprite_t *sp = malloc( dfs_size( fp ) );
        // fread( sp, 1, filesize( fp ), fp );
        // fclose( fp );
        dfs_read(sp, 1, dfs_size(fp), fp);
        dfs_close(fp);

        printf("height: %d, width: %d\n", sp->height, sp->width);

        return sp;
    }
    else
    {
        printf("Missing FP[%d] for file %s\n", fp, spritename);
        return 0;
    }
}

static void init_sprites(void) {
    // int count = 0;
    // populate_dir(&count);
    printf("init sprites\n");
    // int fp = dfs_open("/a-button-icon.png");
    // a_button_icon = malloc( dfs_size( fp ) );
    // dfs_read( a_button_icon, 1, dfs_size( fp ), fp );
    // dfs_close( fp );

    a_button_icon = read_sprite("a-button-icon.sprite");

    //api_write_raw_button_icon = read_sprite( "rom://a_button_icon.sprite" );

    g_thumbnail_cache = malloc(sizeof(sprite_t*) * 4); // alloc the buffer

    #if BUILD_FOR_EMULATOR == 1
    int thumbnailCount = sizeof(g_thumbnail_table) / sizeof(*g_thumbnail_table);
    for(int i = 0; i < thumbnailCount; i++) {
        // fp = dfs_open(g_thumbnail_table[i]);
        // printf("%s opened? %d\n", g_thumbnail_table[i], fp);

        // sprite_t* sprite = malloc( dfs_size( fp ) );
        // dfs_read(sprite, 1, dfs_size(fp), fp);
        // g_thumbnail_cache[i] = sprite;

        // dfs_close( fp );

        printf("Loading thumbnail: %s\n", g_thumbnail_table[i]);
        g_thumbnail_cache[i] = read_sprite(g_thumbnail_table[i]);
        // printf("height: %d, width: %d\n", g_thumbnail_cache[i]->height, g_thumbnail_cache[i]->width);
    }
    #endif

    printf("done!\n");
}

void start_shell(void) {
    /* Init the screen, controller, and filesystem */
    // display_init(RESOLUTION_512x240, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    // display_init(RESOLUTION_512x480, DEPTH_32_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE); // jitters in cen64
    controller_init();
    
    #if BUILD_FOR_EMULATOR == 1
    bool debuggingOutputEnabled = debug_init_isviewer();

    if (!debuggingOutputEnabled) {
        printf("Unable to enable isviewer support.\n");
    }

    int ret = dfs_init(DFS_DEFAULT_LOCATION);
    if (ret != DFS_ESUCCESS) {
        printf("Unable to init filesystem. ret: %d\n", ret);
		printf("git rev %08x\n", GIT_REV);

        waitForStart();
    }

    /* Load sprites for shell from the filesystem */
    init_sprites();

    /* Starts the shell by rendering the list of files from the SD card*/
    show_list();

    #else

    int ret = dfs_init(DFS_DEFAULT_LOCATION);
    if (ret != DFS_ESUCCESS) {
        printf("Unable to init filesystem. ret: %d\n", ret);
		printf("git rev %08x\n", GIT_REV);
    } else {
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
                        break;
                    } else if (keys.c[0].B) {
                        retrying = false;
                        break;
                    }
                }
            } else {
                printf("\nSD Init... SUCCESS!!\n");
                retrying = false;
            }
        }

        printf("Press START to load The Shell.\n");
        while (true) {
            controller_scan();
            struct controller_data keys = get_keys_pressed();
            if (keys.c[0].start) {
                break;
            }
        }
        console_clear();

        waitForStart();

        /* Load sprites for shell from the filesystem */
        init_sprites();

        /* Starts the shell by rendering the list of files from the SD card*/
        show_list();
    }
    #endif
}
