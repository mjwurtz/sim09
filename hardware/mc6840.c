/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   mc6840.c -- emulation of Motorola MC6840 Timer
   Copyright (C) 2021 Michel J Wurtz

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define _XOPEN_SOURCE

#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <error.h>
#include <errno.h>

#include <stdlib.h>
#include "../emu/config.h"
#include "../emu/emu6809.h"
#include "hardware.h"

#define TIMER_CR13 0
#define TIMER_CR2 1
#define TIMER_SR 1
#define TIMER_MSB1 2
#define TIMER_T1C 2
#define TIMER_T1L 3
#define TIMER_LSB1 3
#define TIMER_MSB2 4
#define TIMER_T2C 4
#define TIMER_T2L 5
#define TIMER_LSB2 5
#define TIMER_MSB3 6
#define TIMER_T3C 6
#define TIMER_T3L 7
#define TIMER_LSB3 7

struct Timer {
	uint8_t cr1;
	uint8_t cr2;
	uint8_t cr3;
	uint8_t sr;
	uint16_t timer1;
	uint16_t timer2;
	uint16_t timer3;
	uint16_t latch1;
	uint16_t latch2;
	uint16_t latch3;
	uint32_t cycles_t1_last;
	uint32_t cycles_t2_last;
	uint32_t cycles_t3_last;
} ;


/*
   The 6840 TIMER is not fully implemented. Only the continuous operating mode
   and the sigle shot mode are implemented, with interrupt management.
   Frequency and pulse width can't be managed without hardware...
*/

// Timer initialisation after reset (soft or hard)
void mc6840_reset( struct Device *dev) {
// @TODO
}

// Timer creation
void mc6840_init( char* devname, int adr, char int_line) {
	struct Device *new;
	struct Timer *timer;

	// Create a device and allocate space for data
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, "MC6840");
	new->type = MC6840;
	new->addr = adr;
	new->end = adr+8;
	new->interrupt = int_line;
	timer = mmalloc( sizeof( struct Timer));
	new->registers = timer;
	new->next = devices;
	devices = new;
	mc6840_reset( new);
}

void mc6840_run( struct Device *dev) {
	// call this every time around the loop
	int i;
	char buf;
	struct Timer *timer;

	timer = dev->registers;
	// @TODO : manage counter change at processor frequency (1Mhz)


	// An interrupt condition occured
	if (timer->sr & 0x80) {
	  switch (dev->interrupt) {
		case 'F': firq(); break;
		case 'I': irq(); break;
		case 'N': nmi();
		default: break;
	  }
	}
}

// handle reads from TIMER registers
uint8_t mc6840_read( struct Device *dev, tt_u16 reg) {
  struct Timer *timer;
  timer = dev->registers;
	switch (reg & 0x07) {   // not fully mapped
		case TIMER_SR:
			return timer->sr;
		case TIMER_T1C:
			// clear interrupt flag if set after SR read
			if (timer->sr & 0x01)
			  timer->sr |= 0xfe;
			return (timer->timer1 >> 8) & 0xff;
		case TIMER_LSB1:
			return (timer->timer1) & 0xff;
		case TIMER_T2C:
			if (timer->sr & 0x02)
			  timer->sr |= 0xfd;
			return (timer->timer2 >> 8) & 0xff;
		case TIMER_LSB2:
			return (timer->timer2) & 0xff;
		case TIMER_T3C:
			if (timer->sr & 0x04)
			  timer->sr |= 0xfb;
			return (timer->timer3 >> 8) & 0xff;
		case TIMER_LSB3:
			return (timer->timer3) & 0xff;
	}
	return 0xff;	// maybe the bus floats
}

// handle writes to TIMER registers
void mc6840_write( struct Device *dev, tt_u16 reg, uint8_t val) {
  struct Timer *timer;
  timer = dev->registers;
	switch (reg & 0x07) {   // not fully mapped
	  case TIMER_CR13:
		if (timer->cr2 & 0x01)
		  timer->cr1 = val;
		else
		  timer->cr3 = val;
		break;
	  case TIMER_CR2:
		timer->cr2 = val;
		break;
	}
}

void mc6840_reg( struct Device *dev) {
  struct Timer *timer;
  timer = dev->registers;
  printf( "\n       Timer 1 - CR1:%02X, TIMER:%02X, LATCH1:%02X,",
	  	timer->cr1, timer->timer1, timer->latch1);
  printf( "\n       Timer 2 - CR2:%02X, TIMER:%02X, LATCH2:%02X,",
		timer->cr2, timer->timer2, timer->latch2);
  printf( "\n       Timer 3 - CR3:%02X, TIMER:%02X, LATCH3:%02X, SR:%02X)\n",
		timer->cr3, timer->timer3, timer->latch3, timer->sr);
}
