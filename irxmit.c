/* Mock IR transmitter to test the badge code's handling of IR packets */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "build_bug_on.h"

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
}

static void send_hit_packet(void)
{
	send_a_packet(1234);
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
		default:
			printf("Invalid choice, try again.\n");
			break;
		}
	} while (choice != 0);
	return 0;
}

