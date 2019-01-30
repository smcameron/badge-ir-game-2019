/*

This is code for a kind of "laser-tag"-ish game meant to be played
with little PIC32 badges that have IR transmitters and receivers.
If the code seems a little strange, it's due to the environment this
code must run in.

*/

#ifdef __linux__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "linuxcompat.h"
#endif

#include "build_bug_on.h"

#define ARRAYSIZE(x) (sizeof((x)) / sizeof((x)[0]))

/* Builds up a 32 bit badge packet.
 * 1 bit for start
 * 1 bit for cmd,
 * 5 bits for addr
 * 9 bits for badge_id,
 * 16 bits for payload
 */
static unsigned int __attribute__((unused)) build_ir_packet(unsigned char start, unsigned char cmd,
		unsigned char addr, unsigned short badge_id, unsigned short payload)
{
	BUILD_ASSERT(sizeof(unsigned int) == 4); /* these generate no code */
	BUILD_ASSERT(sizeof(unsigned short) == 2);

	unsigned int packet;

	packet = ((start & 0x01) << 15) | ((cmd & 0x01) << 14) | ((addr & 0x01f) << 9) | payload;
	return packet;
}

static unsigned char __attribute__((unused)) get_start_bit(unsigned int packet)
{
	return (unsigned char) ((packet >> 15) & 0x01);
}

static unsigned char __attribute__((unused)) get_cmd_bit(unsigned int packet)
{
	return (unsigned char) ((packet >> 14) & 0x01);
}

static unsigned char __attribute__((unused)) get_addr_bits(unsigned int packet)
{
	return (unsigned char) ((packet >> 9) & 0x01f);
}

static unsigned short __attribute__((unused)) get_payload(unsigned int packet)
{
	return (unsigned short) (packet & 0x0ffff);
}

typedef void (*game_state_function)(void);

static void initial_state(void);
static void game_exit(void);
static void game_main_menu(void);
static void game_process_button_presses(void);
static void game_screen_render(void);

static enum game_state_type {
	INITIAL_STATE = 0,
	GAME_EXIT = 1,
	GAME_MAIN_MENU = 2,
	GAME_PROCESS_BUTTON_PRESSES = 3,
	GAME_SCREEN_RENDER = 4,
} game_state = INITIAL_STATE;

/* Note, game_state_fn[] array must have an entry for every value
   defined by enum game_state_type.  */
static game_state_function game_state_fn[] = {
	initial_state,
	game_exit,
	game_main_menu,
	game_process_button_presses,
	game_screen_render,
};

struct menu_item {
	char text[15];
	enum game_state_type next_state;
	unsigned char cookie;
};

static struct menu {
	char title[15];
	struct menu_item item[20];
	unsigned char nitems;
	unsigned char current_item;
	unsigned char menu_active;
	unsigned char chosen_cookie;
} menu;

static void menu_clear(void)
{
	strncpy(menu.title, "", sizeof(menu.title) - 1);
	menu.nitems = 0;
	menu.current_item = 0;
	menu.menu_active = 0;
	menu.chosen_cookie = 0;
}

static void menu_add_item(char *text, enum game_state_type next_state, unsigned char cookie)
{
	int i;

	if (menu.nitems >= ARRAYSIZE(menu.item))
		return;

	i = menu.nitems;
	strncpy(menu.item[i].text, text, sizeof(menu.item[i].text) - 1);
	menu.item[i].next_state = next_state;
	menu.item[i].cookie = cookie;
	menu.nitems++;
}

static void draw_menu(void)
{
	int i, y, first_item, last_item;

	first_item = menu.current_item - 4;
	if (first_item < 0)
		first_item = 0;
	last_item = menu.current_item + 4;
	if (last_item > menu.nitems - 1)
		last_item = menu.nitems - 1;

	FbClear();
	FbColor(WHITE);
	FbMove(8, 5);
	FbWriteLine(menu.title);

	y = SCREEN_YDIM / 2 - 10 * (menu.current_item - first_item);
	for (i = first_item; i <= last_item; i++) {
		if (i == menu.current_item)
			FbColor(GREEN);
		else
			FbColor(WHITE);
		FbMove(10, y);
		FbWriteLine(menu.item[i].text);
		y += 10;
	}

	FbColor(GREEN);
	FbHorizontalLine(5, SCREEN_YDIM / 2 - 2, SCREEN_XDIM - 5, SCREEN_YDIM / 2 - 2);
	FbHorizontalLine(5, SCREEN_YDIM / 2 + 10, SCREEN_XDIM - 5, SCREEN_YDIM / 2 + 10);
	FbVerticalLine(5, SCREEN_YDIM / 2 - 2, 5, SCREEN_YDIM / 2 + 10);
	FbVerticalLine(SCREEN_XDIM - 5, SCREEN_YDIM / 2 - 2, SCREEN_XDIM - 5, SCREEN_YDIM / 2 + 10);
	game_state = GAME_SCREEN_RENDER;
}

static void menu_change_current_selection(int direction)
{
	menu.current_item += direction;
	if (menu.current_item < 0)
		menu.current_item = menu.nitems - 1;
	else if (menu.current_item >= menu.nitems)
		menu.current_item = 0;
}

static void ir_packet_callback(unsigned int packet)
{
	/* Interrupts will be already disabled when this is called. */
}

static void initial_state(void)
{
	setup_ir_sensor();
	register_ir_packet_callback(ir_packet_callback);	
	game_state = GAME_MAIN_MENU;
}

static void game_exit(void)
{
}

static void game_main_menu(void)
{
	menu_clear();
	menu.menu_active = 1;
	menu_add_item("EXIT GAME", GAME_EXIT, 0);
	draw_menu();
	game_state = GAME_SCREEN_RENDER;
}

static void button_pressed()
{
	if (menu.menu_active) {
		game_state = menu.item[menu.current_item].next_state;
		menu.chosen_cookie = menu.item[menu.current_item].cookie;
		menu.menu_active = 0;
		return;
	}
}

static void game_process_button_presses(void)
{
#ifdef __linux__
    int kp;

    wait_for_keypress();
    kp = get_keypress();
    if (kp < 0) {
        game_state = GAME_EXIT;
        return;
    }
    switch (kp) {
    case 'w':
        if (menu.menu_active)
            menu_change_current_selection(-1);
        break;
    case 's':
        if (menu.menu_active)
            menu_change_current_selection(1);
        break;
    case 'a':
        break;
    case 'd':
        break;
    case ' ':
        button_pressed();
        break;
    case 'q':
	exit(1);
	break;
    default:
        break;
    }
#else

    if (BUTTON_PRESSED_AND_CONSUME) {
        button_pressed();
    } else if (TOP_TAP_AND_CONSUME) {
        if (menu.menu_active)
            menu_change_current_selection(-1);
    } else if (BOTTOM_TAP_AND_CONSUME) {
        if (menu.menu_active)
            menu_change_current_selection(1);
    } else if (LEFT_TAP_AND_CONSUME) {
    } else if (RIGHT_TAP_AND_CONSUME) {
    } else {
        return;
    }
#endif
}

static void game_screen_render(void)
{
	FbSwapBuffers();
	game_state = GAME_PROCESS_BUTTON_PRESSES;
}

#ifdef __linux__

int main(int argc, char *argv[])
{
	do {
		game_state_fn[game_state]();
	} while (game_state != GAME_EXIT);
	return 0;
}

#else

int badge_ir_game_loop(void)
{
	game_state_fn[game_state]();
	return 0;
}

#endif
