// Copyright (c) 2009 Michael Steil
// All rights reserved. License: 2-clause BSD

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "fake6502.h"
#include "disasm.h"
#include "memory.h"
#include "video.h"
#include "ps2.h"
#include "sdcard.h"
#include "loadsave.h"
#include "glue.h"

#define MHZ 8

//#define TRACE
#define LOAD_HYPERCALLS
#define SDCARD_SUPPORT

#ifdef TRACE
#include "rom_labels.h"
char *
label_for_address(uint16_t address)
{
	for (int i = 0; i < sizeof(addresses) / sizeof(*addresses); i++) {
		if (address == addresses[i]) {
			return labels[i];
		}
	}
	return NULL;
}
#endif

void
machine_reset()
{
	video_reset();
	reset6502();
}

static bool
is_kernal()
{
	return ROM[0x3ff6] == 'M' && // only for KERNAL
	       ROM[0x3ff7] == 'I' &&
	       ROM[0x3ff8] == 'S' &&
	       ROM[0x3ff9] == 'T';
}

int
main(int argc, char **argv)
{
#ifdef TRACE
	bool trace = false;
#endif

	if (argc < 3) {
#ifdef SDCARD_SUPPORT
		printf("Usage: %s <rom.bin> <chargen.bin> [<app.prg>[,<load_addr>]]\n\n", argv[0]);
#else
		printf("Usage: %s <rom.bin> <chargen.bin> [<sdcard.img>]\n\n", argv[0]);
#endif
		printf("<rom.bin>:     ROM file:\n");
		printf("                 $0000-$1FFF bank #0 of banked ROM (BASIC)\n");
		printf("                 $2000-$3FFF fixed ROM at $E000-$FFFF (KERNAL)\n");
		printf("                 $4000-$5FFF bank #1 of banked ROM\n");
		printf("                 $6000-$7FFF bank #2 of banked ROM\n");
		printf("                 ...\n");
		printf("               The file needs to be at least $4000 bytes in size.\n\n");
		printf("<chargen.bin>: Character ROM file:\n");
		printf("                 $0000-$07FF upper case/graphics\n");
		printf("                 $0800-$0FFF lower case\n\n");
#ifdef SDCARD_SUPPORT
		printf("<sdcard.img>:  SD card image (partition map + FAT32)\n\n");
#else
		printf("<app.prg>:     Application PRG file (with 2 byte start address header)\n\n");
		printf("<load_addr>:   Override load address (hex, no prefix)\n\n");
#endif
		exit(1);
	}

	// 1st argument: ROM
	FILE *f = fopen(argv[1], "r");
	if (!f) {
		printf("Cannot open %s!\n", argv[1]);
		exit(1);
	}
	fread(ROM, 1, ROM_SIZE, f);
	fclose(f);

	// 2nd argument: Character ROM
	f = fopen(argv[2], "r");
	if (!f) {
		printf("Cannot open %s!\n", argv[2]);
		exit(1);
	}
	uint8_t chargen[4096];
	fread(chargen, 1, sizeof(chargen), f);
	fclose(f);

#ifdef SDCARD_SUPPORT
	// 3rd argument: SD card image (optional)
	if (argc == 4) {
		sdcard_file = fopen(argv[3], "r");
		if (!sdcard_file) {
			printf("Cannot open %s!\n", argv[3]);
			exit(1);
		}
	}
#else
	// 3rd argument: application (optional)
	FILE *prg_file = NULL;
	int prg_override_start = -1;
	if (argc == 4) {
		char *filename = argv[3];
		char *comma = strchr(filename, ',');
		if (comma) {
			prg_override_start = (uint16_t)strtol(comma + 1, NULL, 16);
			*comma = 0;
		}

		prg_file = fopen(filename, "r");
		if (!prg_file) {
			printf("Cannot open %s!\n", argv[3]);
			exit(1);
		}
	}
#endif

	video_init(chargen);
	sdcard_init();

	machine_reset();

	int instruction_counter = 0;
	for (;;) {
#ifdef TRACE
//		if (pc == 0xffc0) {
		if (pc == 0xffd5) {
			trace = true;
		}
		if (trace) {
			printf("\t\t\t\t[%6d] ", instruction_counter);

			char *label = label_for_address(pc);
			int label_len = label ? strlen(label) : 0;
			if (label) {
				printf("%s", label);
			}
			for (int i = 0; i < 10 - label_len; i++) {
				printf(" ");
			}
			printf(" .,%04x ", pc);
			char disasm_line[15];
			int len = disasm(pc, RAM, disasm_line, sizeof(disasm_line));
			for (int i = 0; i < len; i++) {
				printf("%02x ", read6502(pc + i));
			}
			for (int i = 0; i < 9 - 3 * len; i++) {
				printf(" ");
			}
			printf("%s", disasm_line);
			for (int i = 0; i < 15 - strlen(disasm_line); i++) {
				printf(" ");
			}

			printf("a=$%02x x=$%02x y=$%02x s=$%02x p=", a, x, y, sp);
			for (int i = 7; i >= 0; i--) {
				printf("%c", (status & (1 << i)) ? "czidb.vn"[i] : '-');
			}
			printf(" --- %04x", RAM[0xf2]  | RAM[0xf3]  << 8);
			printf("\n");
		}
#endif

#ifdef LOAD_HYPERCALLS
		if ((pc == 0xffd5 || pc == 0xffd8) && is_kernal() && RAM[0xba] == 1) {
			if (pc == 0xffd5) {
				LOAD();
			} else {
				SAVE();
			}
			pc = (RAM[0x100 + sp + 1] | (RAM[0x100 + sp + 2] << 8)) + 1;
			sp += 2;
		}
#endif

		uint32_t old_clockticks6502 = clockticks6502;
		step6502();
		uint8_t clocks = clockticks6502 - old_clockticks6502;
		bool new_frame = false;
		for (uint8_t i = 0; i < clocks; i++) {
			ps2_step();
			sdcard_step();
			new_frame |= video_step(MHZ);
		}

		instruction_counter++;

		if (new_frame) {
			if (!video_update()) {
				break;
			}
			usleep(20000);
			if (!(status & 4)) {
				irq6502();
			}
		}

#ifndef SDCARD_SUPPORT
		if (pc == 0xffcf && is_kernal() && prg_file) {
			// as soon as BASIC starts reading a line,
			// inject the app
			uint8_t start_lo = fgetc(prg_file);
			uint8_t start_hi = fgetc(prg_file);
			uint16_t start;
			if (prg_override_start >= 0) {
				start = prg_override_start;
			} else {
				start = start_hi << 8 | start_lo;
			}
			fread(RAM + start, 1, 65536-start, prg_file);
			fclose(prg_file);
			prg_file = NULL;
		}
#endif
	}

	video_end();

	return 0;
}
