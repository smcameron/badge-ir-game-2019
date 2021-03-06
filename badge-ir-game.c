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
#include <sys/time.h>

#include "linuxcompat.h"

int argcount;
char **arguments;

#define SETUP_IR_SENSOR do { setup_ir_sensor(argcount, arguments); } while (0)

#else

#include "colors.h"
#include "menu.h"
#include "buttons.h"
#include "ir.h"
#include "flash.h" /* for G_sysData */

/* TODO: I shouldn't have to declare these myself. */
#define size_t int
extern char *strcpy(char *dest, const char *src);
extern char *strncpy(char *dest, const char *src, size_t n);
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern char *strcat(char *dest, const char *src);
#ifndef NULL
#define NULL 0
#endif

/* TODO: Is there a canonical header I should include to get the screen dimensions? */
#define SCREEN_XDIM 132
#define SCREEN_YDIM 132

#define SETUP_IR_SENSOR

static int get_badge_id(void)
{
	return G_sysData.badgeId & 0x1ff; /* TODO: Is this right? */
}

#endif

#include "badge-ir-game-protocol.h"
#include "build_bug_on.h"

#define ARRAYSIZE(x) (sizeof((x)) / sizeof((x)[0]))

/* These need to be protected from interrupts. */
#define QUEUE_SIZE 5
static int queue_in;
static int queue_out;
static int packet_queue[QUEUE_SIZE] = { 0 };

static int screen_changed = 0;
#define NO_GAME_START_TIME -1000000
static volatile int seconds_until_game_starts = NO_GAME_START_TIME;
static volatile unsigned int game_id = -1;
static volatile int game_duration = -1;
static volatile int game_start_timestamp = -1;
static volatile int current_time = -1;
static volatile int team = -1;
static volatile int game_variant = GAME_VARIANT_NONE;
static volatile int suppress_further_hits_until = -1;
static const char *game_type[] = {
	"FREE FOR ALL",
	"TEAM BATTLE",
	"ZOMBIES!",
	"CAPTURE BADGE",
};

#define MAX_HIT_TABLE_ENTRIES 256
static struct hit_table_entry {
	unsigned short badgeid;
	unsigned short timestamp;
	unsigned char team;
} hit_table[MAX_HIT_TABLE_ENTRIES];
static int nhits = 0;

/* Builds up a 32 bit badge packet.
 * 1 bit for start
 * 1 bit for cmd,
 * 5 bits for addr
 * 9 bits for badge_id,
 * 16 bits for payload
 */
static unsigned int build_ir_packet(unsigned char start, unsigned char cmd,
		unsigned char addr, unsigned short badge_id, unsigned short payload)
{
	unsigned int packet;

	BUILD_ASSERT(sizeof(unsigned int) == 4); /* these generate no code */
	BUILD_ASSERT(sizeof(unsigned short) == 2);

	packet = ((start & 0x01) << 31) | ((cmd & 0x01) << 30) | ((addr & 0x01f) << 25) |
		 ((badge_id & 0x1ff) << 16) | (payload & 0x0ffff);
	return packet;
}

static unsigned char __attribute__((unused)) get_start_bit(unsigned int packet)
{
	return (unsigned char) ((packet >> 31) & 0x01);
}

static unsigned char __attribute__((unused)) get_cmd_bit(unsigned int packet)
{
	return (unsigned char) ((packet >> 30) & 0x01);
}

static unsigned char __attribute__((unused)) get_addr_bits(unsigned int packet)
{
	return (unsigned char) ((packet >> 25) & 0x01f);
}

static unsigned char __attribute__((unused)) get_badge_id_bits(unsigned int packet)
{
	return (unsigned char) ((packet >> 16) & 0x01f);
}

static unsigned short get_payload(unsigned int packet)
{
	return (unsigned short) (packet & 0x0ffff);
}

typedef void (*game_state_function)(void);

static void initial_state(void);
static void game_exit(void);
static void game_main_menu(void);
static void game_process_button_presses(void);
static void game_screen_render(void);
static void game_shoot(void);
static void game_confirm_exit(void);
static void game_exit_abandoned(void);

static enum game_state_type {
	INITIAL_STATE = 0,
	GAME_EXIT = 1,
	GAME_MAIN_MENU = 2,
	GAME_PROCESS_BUTTON_PRESSES = 3,
	GAME_SCREEN_RENDER = 4,
	GAME_SHOOT = 5,
	GAME_CONFIRM_EXIT = 6,
	GAME_EXIT_ABANDONED = 7
} game_state = INITIAL_STATE;

/* Note, game_state_fn[] array must have an entry for every value
   defined by enum game_state_type.  */
static game_state_function game_state_fn[] = {
	initial_state,
	game_exit,
	game_main_menu,
	game_process_button_presses,
	game_screen_render,
	game_shoot,
	game_confirm_exit,
	game_exit_abandoned,
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
	char str[15], str2[15], title[15], timecode[15];
	int color;

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

	y = SCREEN_YDIM / 2 - 12 * (menu.current_item - first_item);
	for (i = first_item; i <= last_item; i++) {
		if (i == menu.current_item)
			FbColor(GREEN);
		else
			FbColor(WHITE);
		FbMove(10, y);
		FbWriteLine(menu.item[i].text);
		y += 12;
	}

	FbColor(GREEN);
	FbHorizontalLine(5, SCREEN_YDIM / 2 - 2, SCREEN_XDIM - 5, SCREEN_YDIM / 2 - 2);
	FbHorizontalLine(5, SCREEN_YDIM / 2 + 10, SCREEN_XDIM - 5, SCREEN_YDIM / 2 + 10);
	FbVerticalLine(5, SCREEN_YDIM / 2 - 2, 5, SCREEN_YDIM / 2 + 10);
	FbVerticalLine(SCREEN_XDIM - 5, SCREEN_YDIM / 2 - 2, SCREEN_XDIM - 5, SCREEN_YDIM / 2 + 10);


	title[0] = '\0';
	timecode[0] = '\0';
	color = WHITE;
	if (seconds_until_game_starts == NO_GAME_START_TIME) {
		suppress_further_hits_until = -1;
		strcpy(title, "GAME OVER");
		strcpy(timecode, "");
		color = YELLOW;
	} else {
		if (seconds_until_game_starts > 0) {
			strcpy(title, "GAME STARTS IN");
			itoa(timecode, seconds_until_game_starts, 10);
			strcat(timecode, " SECS");
			color = YELLOW;
		} else {
			if (seconds_until_game_starts > -game_duration) {
				strcpy(title, "TIME LEFT:");
				itoa(timecode, game_duration + seconds_until_game_starts, 10);
				strcat(timecode, " SECS");
				color = WHITE;
			} else {
				suppress_further_hits_until = -1;
				strcpy(title, "GAME OVER");
				strcpy(timecode, "");
				game_duration = -1;
				seconds_until_game_starts = NO_GAME_START_TIME;
				screen_changed = 1;
				color = YELLOW;
			}
		}
        }
	if (suppress_further_hits_until > 0) { /* have been hit recently */
		static int old_deadtime = 0;
		color = RED;
		FbColor(color);
		FbMove(10, 40);
		strcpy(str, "DEAD TIME:");
		itoa(str2, suppress_further_hits_until - current_time, 10);
		if (old_deadtime != suppress_further_hits_until - current_time) {
			old_deadtime = suppress_further_hits_until - current_time;
			screen_changed = 1;
		}
		strcat(str, str2);
		FbWriteLine(str);
	}
	FbColor(color);
	if (game_variant != GAME_VARIANT_NONE) {
		FbMove(10, 10);
		strcpy(str2, game_type[game_variant % 4]);
		FbWriteLine(str2);
	}
	if (team >= 0) {
		FbMove(10, 20);
		itoa(str, team, 10);
		strcpy(str2, "TEAM:");
		strcat(str2, str);
		FbWriteLine(str2);
	}
	FbMove(10, 30);
	strcpy(str, "HITS:");
	itoa(str2, nhits, 10);
	strcat(str, str2);
	FbWriteLine(str);
	FbMove(10, 100);
	FbWriteLine(title);
	FbMove(10, 110);
	FbWriteLine(timecode);
	game_state = GAME_SCREEN_RENDER;
}

static void menu_change_current_selection(int direction)
{
	int old = menu.current_item;
	menu.current_item += direction;
	if (menu.current_item < 0)
		menu.current_item = menu.nitems - 1;
	else if (menu.current_item >= menu.nitems)
		menu.current_item = 0;
	screen_changed |= (menu.current_item != old);
}

static void ir_packet_callback(struct IRpacket_t packet)
{
	/* Interrupts will be already disabled when this is called. */
	int next_queue_in;

	next_queue_in = (queue_in + 1) % QUEUE_SIZE;
	if (next_queue_in == queue_out) /* queue is full, drop packet */
		return;
	memcpy(&packet_queue[queue_in], &packet, sizeof(packet_queue[0]));
	queue_in = next_queue_in;
}

static void setup_main_menu(void)
{
	menu_clear();
	menu.menu_active = 1;
	strcpy(menu.title, "");
	menu_add_item("SHOOT", GAME_SHOOT, 0);
	menu_add_item("EXIT GAME", GAME_CONFIRM_EXIT, 0);
	screen_changed = 1;
}

static void setup_confirm_exit_menu(void)
{
	menu_clear();
	menu.menu_active = 1;
	strcpy(menu.title, "REALLY QUIT?");
	menu_add_item("DO NOT QUIT", GAME_EXIT_ABANDONED, 0);
	menu_add_item("REALLY QUIT", GAME_EXIT, 0);
	screen_changed = 1;
}

#ifndef __linux__
static void (*old_callback)(struct IRpacket_t) = NULL;
static void register_ir_packet_callback(void (*callback)(struct IRpacket_t))
{
	/* This is pretty gross.  Ideally there should be some registration,
	 * unregistration functions provided by ir.[ch] and I shouldn't touch
	 * IRcallbacks[] directly myself, and all those hardcoded ir_app[1-7]()
	 * functions should disappear.
	 * Also, if an interrupt happens in the midst of the assignment we'll
	 * be in trouble.  I expect the assignment is probably atomic though.
	 */
	old_callback = IRcallbacks[BADGE_IR_GAME_ADDRESS].handler;
	IRcallbacks[BADGE_IR_GAME_ADDRESS].handler = callback;
}

static void unregister_ir_packet_callback(void)
{
	/* Gross. */
	IRcallbacks[BADGE_IR_GAME_ADDRESS].handler = old_callback;
}
#endif

static void initial_state(void)
{
	FbInit();
	SETUP_IR_SENSOR;
	register_ir_packet_callback(ir_packet_callback);
	queue_in = 0;
	queue_out = 0;
	setup_main_menu();
	game_state = GAME_MAIN_MENU;
	screen_changed = 1;
}

static void game_exit(void)
{
	unregister_ir_packet_callback();
	returnToMenus();
}

static void game_main_menu(void)
{
	menu.menu_active = 1;
	draw_menu();
	game_state = GAME_SCREEN_RENDER;
}

static void game_confirm_exit(void)
{
	setup_confirm_exit_menu();
	game_state = GAME_MAIN_MENU;
}

static void game_exit_abandoned(void)
{
	setup_main_menu();
	game_state = GAME_MAIN_MENU;
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

static void set_game_start_timestamp(int time)
{
#ifdef __linux__
	struct timeval tv;

	gettimeofday(&tv, NULL);
	game_start_timestamp = tv.tv_sec + time;
#else
	/* TODO: write this */
#endif
}

static void process_hit(unsigned int packet)
{
	int timestamp;
	unsigned char shooter_team = (get_payload(packet) | 0x0f);
	unsigned short badgeid = get_badge_id_bits(packet);
	timestamp = current_time - game_start_timestamp;
	if (timestamp < 0) /* game has not started yet  */
		return;
	if (seconds_until_game_starts == NO_GAME_START_TIME)
		return;
	if (current_time < suppress_further_hits_until)
		return;

	switch (game_variant) {
	case GAME_VARIANT_TEAM_BATTLE:
	case GAME_VARIANT_ZOMBIE:
	case GAME_VARIANT_CAPTURE_THE_BADGE:
		if (team == shooter_team) /* exclude friendly fire */
			return;
	case GAME_VARIANT_FREE_FOR_ALL:
		break;
	case GAME_VARIANT_NONE:
		/* fall through */
	default:
		return; /* hits have no effect if a known game is not in play */
	}

	hit_table[nhits].badgeid = badgeid;
	hit_table[nhits].timestamp = (unsigned short) timestamp;
	hit_table[nhits].team = shooter_team;
	nhits++;
	screen_changed = 1;
	if (nhits >= MAX_HIT_TABLE_ENTRIES)
		nhits = 0;
	suppress_further_hits_until = current_time + 30;
}

static void send_ir_packet(unsigned int packet)
{
	union IRpacket_u p;

	p.v = packet;
	p.v = packet | (1 << 30); /* Set command bit on outgoing packets */

	IRqueueSend(p);
}

static void send_game_id_packet(unsigned int game_id)
{
	send_ir_packet(build_ir_packet(1, 1, BADGE_IR_GAME_ADDRESS, get_badge_id(),
		(OPCODE_GAME_ID << 12) | (game_id & 0x0fff)));
}

static void send_badge_record_count(unsigned int nhits)
{
	send_ir_packet(build_ir_packet(1, 1, BADGE_IR_GAME_ADDRESS, get_badge_id(),
		(OPCODE_GAME_ID << 12) | (nhits & 0x0fff)));
}

static void send_badge_upload_hit_record_badge_id(struct hit_table_entry *h)
{
	send_ir_packet(build_ir_packet(1, 1, BADGE_IR_GAME_ADDRESS, get_badge_id(),
		(OPCODE_BADGE_UPLOAD_HIT_RECORD_BADGE_ID << 12) | (h->badgeid & 0x01ff)));
}

static void send_badge_upload_hit_record_timestamp(struct hit_table_entry *h)
{
	send_ir_packet(build_ir_packet(1, 1, BADGE_IR_GAME_ADDRESS, get_badge_id(),
		(OPCODE_BADGE_UPLOAD_HIT_RECORD_TIMESTAMP << 12) | (h->timestamp & 0x0fff)));
}

static void send_badge_upload_hit_record_team(struct hit_table_entry *h)
{
	send_ir_packet(build_ir_packet(1, 1, BADGE_IR_GAME_ADDRESS, get_badge_id(),
		(OPCODE_SET_BADGE_TEAM << 12) | (h->team & 0x0fff)));
}

static void dump_badge_to_base_station(void)
{
	int i;

	/*
	* 1. Base station requests info from badge: OPCODE_REQUEST_BADGE_DUMP
	* 2. Badge responds with OPCODE_GAME_ID
	* 3. Badge responds with OPCODE_BADGE_RECORD_COUNT
	* 4. Badge responds with triplets of OPCODE_BADGE_UPLOAD_HIT_RECORD_BADGE_ID,
	*    OPCODE_BADGE_UPLOAD_HIT_RECORD_TIMESTAMP, and OPCODE_SET_BADGE_TEAM.
	*/

	send_game_id_packet(game_id);
	send_badge_record_count(nhits);

	for (i = 0; i < nhits; i++) {
		send_badge_upload_hit_record_badge_id(&hit_table[i]);
		send_badge_upload_hit_record_timestamp(&hit_table[i]);
		send_badge_upload_hit_record_team(&hit_table[i]);
	}
}

static void process_packet(unsigned int packet)
{
	unsigned int payload;
	unsigned char opcode;
	int v;

	payload = get_payload(packet);
	opcode = payload >> 12;
	switch (opcode) {
	case OPCODE_SET_GAME_START_TIME:
		/* time is a 12 bit signed number */
		v = payload & 0x0fff;
		if (payload & 0x0800)
			v = -v;
		seconds_until_game_starts = v;
		set_game_start_timestamp(seconds_until_game_starts);
		if (seconds_until_game_starts > 0)
			nhits = 0; /* don't reset counter if game already started? */
		screen_changed = 1;
		break;
	case OPCODE_SET_GAME_DURATION:
		/* time is 12 unsigned number */
		game_duration = payload & 0x0fff;
		screen_changed = 1;
		break;
	case OPCODE_HIT:
		process_hit(packet);
		break;
	case OPCODE_REQUEST_BADGE_DUMP:
		dump_badge_to_base_station();
		break;
	case OPCODE_SET_BADGE_TEAM:
		team = payload & 0x0f; /* TODO sanity check this better. */
		screen_changed = 1;
		break;
	case OPCODE_SET_GAME_VARIANT:
		game_variant = payload & 0x0f; /* TODO sanity check this better. */
		screen_changed = 1;
		break;
	case OPCODE_GAME_ID:
		game_id = payload & 0x0fff;
		break;
	default:
		break;
	}
}

#ifdef __linux__
#define DISABLE_INTERRUPTS do { disable_interrupts(); } while (0)
#define ENABLE_INTERRUPTS do { enable_interrupts(); } while (0)
#else
#define DISABLE_INTERRUPTS
#define ENABLE_INTERRUPTS
#endif


static void check_for_incoming_packets(void)
{
	unsigned int new_packet;
	int next_queue_out;

	DISABLE_INTERRUPTS;
	while (queue_out != queue_in) {
		next_queue_out = (queue_out + 1) % QUEUE_SIZE;
		new_packet = packet_queue[queue_out];
		queue_out = next_queue_out;
		ENABLE_INTERRUPTS;
		process_packet(new_packet);
		DISABLE_INTERRUPTS;
	}
	ENABLE_INTERRUPTS;
}

static void advance_time()
{
#ifdef __linux__
	struct timeval tv;
	int old_time = seconds_until_game_starts;
	int old_suppress = suppress_further_hits_until;

	gettimeofday(&tv, NULL);
	current_time = tv.tv_sec;
	if (seconds_until_game_starts != NO_GAME_START_TIME && game_start_timestamp != NO_GAME_START_TIME) {
		seconds_until_game_starts = game_start_timestamp - tv.tv_sec;
	}
	if (suppress_further_hits_until <= current_time)
		suppress_further_hits_until = -1;
	if (old_time != seconds_until_game_starts ||
		old_suppress != suppress_further_hits_until)
		screen_changed = 1;
#else
	/* TODO: fill this in */
#endif
}

static void game_process_button_presses(void)
{
	check_for_incoming_packets();
	advance_time();
	if (BUTTON_PRESSED_AND_CONSUME) {
		button_pressed();
	} else if (UP_BTN_AND_CONSUME) {
		if (menu.menu_active)
			menu_change_current_selection(-1);
	} else if (DOWN_BTN_AND_CONSUME) {
		if (menu.menu_active)
			menu_change_current_selection(1);
	} else if (LEFT_BTN_AND_CONSUME) {
	} else if (RIGHT_BTN_AND_CONSUME) {
	}
	if (game_state == GAME_PROCESS_BUTTON_PRESSES && screen_changed)
		game_state = GAME_MAIN_MENU;
	return;
}

static void game_screen_render(void)
{
	game_state = GAME_PROCESS_BUTTON_PRESSES;
	if (!screen_changed)
		return;
	FbSwapBuffers();
	screen_changed = 0;
}

static void game_shoot(void)
{
	unsigned int packet;
	unsigned short payload;

	payload = (OPCODE_HIT << 12) | (team & 0x0f);
	packet = build_ir_packet(0, 1, BADGE_IR_GAME_ADDRESS, get_badge_id(), payload);
	send_ir_packet(packet);

	game_state = GAME_MAIN_MENU;
}

int badge_ir_game_loop(void)
{
	game_state_fn[game_state]();
	return 0;
}

#ifdef __linux__

int main(int argc, char *argv[])
{
	argcount = argc;
	arguments = argv;

	start_gtk(&argc, &argv, badge_ir_game_loop, 30);
	return 0;
}

#endif
