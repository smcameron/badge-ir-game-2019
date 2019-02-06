/* Mock IR transmitter to test the badge code's handling of IR packets */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "badge-ir-game-protocol.h"
#include "build_bug_on.h"

#define BASE_STATION_BADGE_ID 99

static int fifo_fd = -1;

static void open_fifo(char *fifoname)
{
	printf("Opening fifo %s...", fifoname);
	fflush(stdout);
	fifo_fd = open(fifoname, O_WRONLY);
	if (fifo_fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", fifoname, strerror(errno));
		exit(1);
	}
	printf("opened.  Proceeding.\n");
}

static unsigned int build_packet(unsigned char cmd, unsigned char start,
			unsigned char address, unsigned short badge_id, unsigned short payload)
{
	return ((cmd & 0x01) << 31) |
		((start & 0x01) << 30) |
		((address & 0x01f) << 25) |
		((badge_id & 0x1ff) << 16) |
		(payload);
}

static void send_a_packet(unsigned int packet)
{
	int count, rc, bytesleft = 4;
	unsigned char *b = (unsigned char *) &packet;

	BUILD_ASSERT(sizeof(unsigned int) == 4); /* this generates no code */

	count = 0;
	bytesleft = 4;
	do {
		rc = write(fifo_fd, &b[count], bytesleft);
		if (rc < 0 && errno == EINTR)
			continue;
		if (rc < 0) {
			printf("Failed to write packet: %s\n", strerror(errno));
			return;
		}
		bytesleft -= rc;
		count += rc;
	} while (bytesleft > 0);
	printf("\nSent packet: %08x\n", packet);
	printf("      cmd: 0x%01x\n", (packet >> 31) & 0x01);
	printf("    start: 0x%01x\n", (packet >> 30) & 0x01);
	printf("  address: 0x%02x\n", (packet >> 25) & 0x1f);
	printf(" badge ID: 0x%03x\n", (packet >> 16) & 0x1ff);
	printf("  payload: 0x%04x\n\n", packet & 0x0ffff);
}

static unsigned int get_a_number(char *prompt, unsigned int mask)
{
	char *x;
	char buffer[256];
	unsigned int number;
	int rc;

	do {
		printf("Enter %s: ", prompt);
		x = fgets(buffer, sizeof(buffer), stdin);
		if (!x) {
			printf("Something went wrong.\n");
			break;
		}
		rc = sscanf(buffer, "%u", &number);
		if (rc != 1) {
			printf("Bad number, try again.\n");
			continue;
		}
		number &= mask;
		return number;
	} while (1);
	return 0xffffffff & mask;
}

static void send_hit_packet(void)
{
	unsigned int badge_id, team_id;

	badge_id = get_a_number("Shooter's badge ID", 0x01ff);
	team_id = get_a_number("Shooter's team ID", 0x0f);

	send_a_packet(build_packet(1, 1, BADGE_IR_GAME_ADDRESS, badge_id,
		(OPCODE_HIT << 12) | team_id));
}

static void send_game_start_time(void)
{
	unsigned int start_time;
	unsigned short badge_id = BASE_STATION_BADGE_ID;

	start_time = get_a_number("seconds until game starts", 0x0fff);
	send_a_packet(build_packet(1, 1, BADGE_IR_GAME_ADDRESS, badge_id,
			(OPCODE_SET_GAME_START_TIME << 12) | start_time));
}

static void send_game_duration(void)
{
	unsigned int duration;
	unsigned short badge_id = BASE_STATION_BADGE_ID;

	duration = get_a_number("duration of game in seconds", 0x0fff);
	send_a_packet(build_packet(1, 1, BADGE_IR_GAME_ADDRESS, badge_id,
		(OPCODE_SET_GAME_DURATION << 12) | duration));
}

static void send_game_variant(void)
{
	unsigned int game_variant;
	unsigned short badge_id = BASE_STATION_BADGE_ID;

	game_variant = get_a_number("game variant", 0x0f);
	send_a_packet(build_packet(1, 1, BADGE_IR_GAME_ADDRESS, badge_id,
		(OPCODE_SET_GAME_VARIANT << 12) | game_variant));
}

static void send_team(void)
{
	unsigned int team_id;
	unsigned short badge_id = BASE_STATION_BADGE_ID;

	team_id = get_a_number("badge team ID",	0x0f);
	send_a_packet(build_packet(1, 1, BADGE_IR_GAME_ADDRESS, badge_id,
		(OPCODE_SET_BADGE_TEAM << 12) | team_id));
}

static void send_game_id(void)
{
	unsigned int game_id;
	unsigned short badge_id = BASE_STATION_BADGE_ID;

	game_id = get_a_number("game ID", 0x0f);
	send_a_packet(build_packet(1, 1, BADGE_IR_GAME_ADDRESS, badge_id,
		(OPCODE_GAME_ID << 12) | game_id));
}

static void send_request_badge_dump(void)
{
	send_a_packet(build_packet(1, 1, BADGE_IR_GAME_ADDRESS, BASE_STATION_BADGE_ID,
		(OPCODE_REQUEST_BADGE_DUMP << 12)));
}

int main(int argc, char *argv[])
{
	int rc, choice = 0;
	char buffer[256];
	char *x;

	open_fifo("/tmp/badge-ir-fifo");

	do {
		printf("\n");
		printf("  Mock IR transmitter badge tester\n");
		printf("\n");
		printf("  1. Send a \"hit\" packet to the badge.\n");
		printf("  2. Send game start time.\n");
		printf("  3. Send game duration.\n");
		printf("  4. Send game variant.\n");
		printf("  5. Send team.\n");
		printf("  6. Send game ID.\n");
		printf("  7. Send badge request dump.\n");
		printf("  0. Exit\n");
		printf("\n");
		printf("  Enter choice: ");

		x = fgets(buffer, sizeof(buffer), stdin);
		if (!x)
			return 0;
		rc = sscanf(buffer, "%d", &choice);
		if (rc < 0) {
			printf("Invalid choice, try again.\n");
			continue;
		}
		switch (choice) {
		case 0:
			break;
		case 1: send_hit_packet();
			break;
		case 2: send_game_start_time();
			break;
		case 3: send_game_duration();
			break;
		case 4: send_game_variant();
			break;
		case 5: send_team();
			break;
		case 6: send_game_id();
			break;
		case 7: send_request_badge_dump();
			break;
		case 8:
			break;
		default:
			printf("Invalid choice, try again.\n");
			break;
		}
	} while (choice != 0);
	return 0;
}

