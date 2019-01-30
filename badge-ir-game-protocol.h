
/* We have 16 bits of payload. Let's say the high order 4 bits are the opcode.
 * That gives us 16 opcodes, with 12 bits of payload per opcode for single
 * packet opcodes (we can have multi-packet opcodes if needed).
 * All integer values are transmitted little endian (low order byte first).
 *
 * Badge packet is 32-bits:
 * 1 start bit
 * 1 cmd bit
 * 5 address bits (like port number)
 * 9 badge id bits
 * 16 payload bits
 */

#define OPCODE_SET_GAME_START_TIME 0x00
/* Remaining 12 bits are signed seconds until game starts, up to +/- 34 minutes. */

#define OPCODE_SET_GAME_DURATION 0x01
/* Remaining 12 bits are duration in seconds */

#define OPCODE_HIT 0x02
/* Low 4 bits of payload are team id of shooter */

#define OPCODE_SET_BADGE_TEAM 0x03
/* Low 4 bits of payload are the team ID */

#define OPCODE_REQUEST_BADGE_DUMP 0x04

#define OPCODE_SET_GAME_VARIANT 0x05

