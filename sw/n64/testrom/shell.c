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

/*
TODO
Make the more files above/below indicator a sprite or something besides '...'
Figure out why it feels laggy? The example code feels the same way, not sure it runs at full speed in the emulator
Load entries from an SD card
Load selected rom into memory and start
*/

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 240
int NUM_ENTRIES = 21;

/* Layout */
const int ROW_HEIGHT = 12;
const int MARGIN_PADDING = 20;
const int ROW_SELECTION_WIDTH = SCREEN_WIDTH;
const int MENU_BAR_HEIGHT = 15;
const int LIST_TOP_PADDING = MENU_BAR_HEIGHT + ROW_HEIGHT;

/* Colors */
color_t MENU_BAR_COLOR = {.r = 0x82,.g = 0x00,.b = 0x2E,.a = 0x00 };	// 0x82002E, Berry
color_t SELECTION_COLOR = {.r = 0x00,.g = 0x67,.b = 0xC7,.a = 0x00 };	// 0x0067C7, Bright Blue

/* Assume default font size*/
static int calculate_num_rows_per_page(void)
{
	// ---top of screen---
	// menu bar
	// padding + more files above indicator space
	// list of files...
	// ...
	// more file indicator space
	// ---bottom of screen---

	// Start by subtracting the menu bar height, and paddings from the total height
	int availableHeight = SCREEN_HEIGHT - MENU_BAR_HEIGHT - (ROW_HEIGHT * 2);

	// Since there is currently no other dynamic portion, we can use a simple calculation to find rows that will fit in the space
	int rows = availableHeight / ROW_HEIGHT;

	return rows;
}

/*
 * Render a list of strings and show a caret on the currently selected row
 */
static void render_list(display_context_t display, char *list[], int currently_selected, int first_visible, int max_on_screen)
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
			graphics_draw_text(display, MARGIN_PADDING / 2, y, "  >");

			/* Increase Margin Padding to account for added caret width */
			x += MARGIN_PADDING;
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
}

static void draw_header_bar(display_context_t display, int fileCount)
{
	int x = 0, y = 0, width = SCREEN_WIDTH, height = MENU_BAR_HEIGHT;
	graphics_draw_box(display, x, y, width, height, graphics_convert_color(MENU_BAR_COLOR));

	char menuHeaderBuffer[100];
	sprintf(menuHeaderBuffer, "PicoCart64 OS (git rev %08x)\t\t\t\t%d Files", GIT_REV, fileCount);
	graphics_draw_text(display, MARGIN_PADDING, y + 4, menuHeaderBuffer);
}

/*
 * Init display and controller input then render a list of strings, showing currently selected
 */
static void show_list(void)
{
	// For now, hard code some strings in so we can have something to test with
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
		/*"お母さん３(JPN).n64", */// I don't think it likes this entry too much, will need a font that supports UTF8
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
		render_list(display, entries, currently_selected, first_visible, max_on_screen);

		/* Grab controller input and move the selection up or down */
		controller_scan();
		struct controller_data keys = get_keys_down();
		int mag = 0;
		if (keys.c[0].down) {
			mag = 1;
		} else if (keys.c[0].up) {
			mag = -1;
		}

		if ((mag > 0 && currently_selected + mag < NUM_ENTRIES) || (mag < 0 && currently_selected > 0)) {
			currently_selected += mag;
		}
		// If we have moved the cursor to an entry not yet visible on screen, move first_visible as well
		if ((mag > 0 && currently_selected >= (first_visible + max_on_screen)) || (mag < 0 && currently_selected < first_visible && currently_selected >= 0)) {
			first_visible += mag;
		}

		/* A little debug text at the bottom of the screen */
		char debugTextBuffer[100];
		snprintf(debugTextBuffer, 100, "currently_selected=%d, first_visible=%d, max_per_page=%d", currently_selected, first_visible, max_on_screen);
		graphics_draw_text(display, 5, 230, debugTextBuffer);

		/* Force the backbuffer flip */
		display_show(display);
	}
}

void start_shell(void)
{
	/* Init the screen and controller */
	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
	controller_init();

	/* Starts the shell by rendering the list of files from the SD card */
	show_list();
}
