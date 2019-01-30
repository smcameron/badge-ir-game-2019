/*

This is code for a kind of "laser-tag"-ish game meant to be played
with little PIC32 badges that have IR transmitters and receivers.
If the code seems a little strange, it's due to the environment this
code must run in.

*/

#ifdef __linux__
#include <stdio.h>

#include "linuxcompat.h"
#endif

#include "build_bug_on.h"

typedef void (*game_state_function)(void);

static void initial_state(void);
static void game_exit(void);

static enum game_state_type {
	INITIAL_STATE = 0,
	GAME_EXIT = 1,
} game_state = INITIAL_STATE;

/* Note, game_state_fn[] array must have an entry for every value
   defined by enum game_state_type.  */
static game_state_function game_state_fn[] = {
	initial_state,
	game_exit,
};

static void initial_state(void)
{
}

static void game_exit(void)
{
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
