// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#ifndef _GLUE_H_
#define _GLUE_H_

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>

#include "cpu/registers.h"

//#define TRACE
//#define PERFSTAT

#define NUM_MAX_BANKS 256
#define NUM_MAX_RAM_BANKS 256
#define NUM_ROM_BANKS 32
#define NUM_CART_BANKS (256 - 32)

#define RAM_SIZE (num_banks * BANK_SIZE)    /* $0000-$9EFF, $9F00-$FFFF "wasted", then optionally $010000+ */
#define BRAM_SIZE (num_ram_banks * 8192)    /* banks at $A000-$BFFF */
#define ROM_SIZE (NUM_ROM_BANKS * 16384)    /* banks at $C000-$FFFF */
#define CART_SIZE (NUM_CART_BANKS * 16384)  /* expansion banks at $C000-$FFFF */

#define WINDOW_TITLE "Commander X16"

#ifdef __APPLE__
#define MOUSE_GRAB_MSG " (\xE2\x87\xA7\xE2\x8C\x98M to end mouse/keyboard capture)"
#else
#define MOUSE_GRAB_MSG " (Ctrl+M to end mouse/keyboard capture)"
#endif

typedef enum {
	ECHO_MODE_NONE,
	ECHO_MODE_RAW,
	ECHO_MODE_COOKED,
	ECHO_MODE_ISO,
} echo_mode_t;

// GIF recorder commands
typedef enum {
	RECORD_GIF_PAUSE,
	RECORD_GIF_SNAP,
	RECORD_GIF_RESUME
} gif_recorder_command_t;

// GIF recorder states
typedef enum {
	RECORD_GIF_DISABLED,
	RECORD_GIF_PAUSED,
	RECORD_GIF_SINGLE,
	RECORD_GIF_ACTIVE
} gif_recorder_state_t;

extern struct regs regs;
extern uint16_t opcode_addr;
extern uint8_t *RAM;
extern uint8_t *BRAM;
extern uint8_t ROM[];
extern uint8_t *CART;

extern uint16_t num_banks;
extern uint16_t num_ram_banks;

extern bool debugger_enabled;
extern bool log_video;
extern bool log_keyboard;
extern bool log_speed;
extern echo_mode_t echo_mode;
extern bool save_on_exit;
extern bool disable_emu_cmd_keys;
extern gif_recorder_state_t record_gif;
extern char *gif_path;
extern uint8_t *fsroot_path;
extern uint8_t *startin_path;
extern uint8_t keymap;
extern bool warp_mode;
extern bool grab_mouse;
extern bool testbench;
extern bool has_via2;
extern uint32_t host_sample_rate;
extern bool enable_midline;

extern bool has_midi_card;
extern uint16_t midi_card_addr;

extern void machine_dump(const char* reason);
extern void machine_reset();
extern void machine_nmi();
extern void machine_paste(char *text);
extern void machine_toggle_warp();
extern void init_audio();
extern void main_shutdown();

extern bool video_is_tilemap_address(int addr);
extern bool video_is_tiledata_address(int addr);
extern bool video_is_special_address(int addr);

extern int ieee_unit;
extern bool using_hostfs;
extern uint8_t activity_led;
extern bool nvram_dirty;
extern uint8_t nvram[0x40];

extern uint8_t MHZ;

extern bool mouse_grabbed;
extern bool no_keyboard_capture;
extern bool kernal_mouse_enabled;
extern char window_title[];
extern bool pwr_long_press;
extern bool is_gen2;
#endif
