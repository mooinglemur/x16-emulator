// Commander X16 Emulator
// Copyright (c) 2021 Michael Steil
// All rights reserved. License: 2-clause BSD

// MCP7940N RTC
// * RTC
//   * 24h and AM/PM supported
//   * oscillator can be turned off
//   * alarms etc. not supported

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "rtc.h"
#include "glue.h"

bool nvram_dirty = false;
uint8_t nvram[0x40];

static bool running;
static bool vbaten;
static bool h24;

static unsigned int clocks;
static int seconds;
static int minutes;
static int hours;
static int day_of_week;
static int day;
static int month;
static int year;

#define I2C_DATA_LEN 16
static uint8_t i2c_data[I2C_DATA_LEN];
static uint8_t i2c_data_pos = 0;

void
rtc_i2c_data(uint8_t v) {
	if (i2c_data_pos < I2C_DATA_LEN) {
		i2c_data[i2c_data_pos] = v;
		i2c_data_pos++;
	}
}



#define BCD(a) (((a) / 10) << 4 | ((a) % 10))
#define UNBCD(a) (((a) >> 4) * 10 + ((a) & 0xf))

void
rtc_init(bool set_system_time)
{
	vbaten = true;
	h24 = true;
	clocks = 0;

	if (set_system_time) {
		running = true;
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		seconds = tm.tm_sec;
		minutes = tm.tm_min;
		hours = tm.tm_hour;
		day_of_week = (tm.tm_wday == 0 ? 7 : tm.tm_wday);
		day = tm.tm_mday;
		month = tm.tm_mon + 1;
		year = tm.tm_year - 100;
	} else {
		running = false; // yes, the MCP7940N starts out this way!
		seconds = 0;
		minutes = 0;
		hours = 0;
		day_of_week = 1;
		day = 1;
		month = 1;
		year = 0;
	}
}

static uint8_t days_per_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

bool
is_leap_year() {
	// the clock does 2000-2099, where the "% 4 == 0" rule applies
	return !(year & 3);
}

void
rtc_step(int c)
{
	if (!running) {
		return;
	}

	clocks += c;
	if (clocks < (MHZ * 1000000)) {
		return;
	}

	clocks -= (MHZ * 1000000);
	seconds++;
	if (seconds < 60) {
		return;
	}

	seconds = 0;
	minutes++;
	if (minutes < 60) {
		return;
	}

	minutes = 0;
	hours++;
	if (hours < 24) {
		return;
	}

	hours = 0;
	day_of_week++;
	if (day_of_week > 7) {
		day_of_week = 1;
	}
	day++;
	uint8_t dpm = days_per_month[month - 1];
	if (month == 2 && is_leap_year()) {
		dpm++;
	}
	if (day <= dpm) {
		return;
	}

	day = 1;
	month++;
	if (month <= 12) {
		return;
	}

	month = 1;
	year++;
	if (year == 100) {
		year = 0; // Y2.1K problem! ;-)
	}
}


uint8_t
rtc_read() {
	//    printf("RTC READ $%02X\n", a);
	uint8_t ret;
	switch (i2c_data[0]) {
		case 0:
			ret = BCD(seconds) | (running << 7);
			break;
		case 1:
			ret = BCD(minutes);
			break;
		case 2: {
			uint8_t h = hours;
			bool pm = false;
			if (!h24) {
				// AM/PM
				if (h >= 12) {
					pm = true;
					h -= 12;
				}
				if (h == 0) {
					h = 12;
				}
			}
			h = BCD(h);
			h |= pm << 5;
			h |= (!h24) << 6;
			ret = h;
			break;
		}
		case 3: {
			uint8_t v = day_of_week;
			v |= vbaten << 3;
			v |= running << 5;
			ret = v;
			break;
		}
		case 4:
			ret = BCD(day);
			break;
		case 5:
			ret = BCD(month) | is_leap_year() << 5;
			break;
		case 6:
			ret = BCD(year);
			break;
		default:
			if (i2c_data[0] >= 0x20 && i2c_data[0] < 0x60) {
				ret = nvram[i2c_data[0] - 0x20];
			} else if (i2c_data[0] >= 0x60) {
				ret = 0xff;
			} else {
				ret = 0;
			}
			break;
	}

	i2c_data_pos = 0;
	return ret;
}

void
rtc_write() {
	//    printf("RTC WRITE $%02X, $%02X\n", a, v);
	switch (i2c_data[0]) {
		case 0:
			running = !!(i2c_data[1] & 0x80);
			seconds = UNBCD(i2c_data[1] & 0x7f);
			break;
		case 1:
			minutes = UNBCD(i2c_data[1]);
			break;
		case 2: {
			h24 = !(i2c_data[1] & 0x40);
			uint8_t h = i2c_data[1] & 0x3f;
			bool pm = false;
			if (!h24) {
				pm = i2c_data[1] & 0x20;
				h &= 0x1f;
			}
			h = UNBCD(h);
			if (!h24 && h == 12) {
				h = 0;
			}
			if (pm) {
				h += 12;
			}
			hours = h;
			break;
		}
		case 3: {
			day_of_week = i2c_data[1] & 7;
			vbaten = !!(i2c_data[1] & 0x20);
			break;
		}
		case 4:
			day = UNBCD(i2c_data[1]);
			break;
		case 5:
			month = UNBCD(i2c_data[1]);
			break;
		case 6:
			year = UNBCD(i2c_data[1]);
			break;
		default:
			if (i2c_data[0] >= 0x20 && i2c_data[0] < 0x60) {
				nvram[i2c_data[0]- 0x20] = i2c_data[1];
				nvram_dirty = true;
			}
	}

	i2c_data_pos = 0;
}

