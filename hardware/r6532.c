/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   r6532.c -- emulation of VIA Rockwell R6532
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

struct Via6532 {
	uint8_t ram[128];
	uint8_t dra;
	uint8_t ddra;
	uint8_t drb;
	uint8_t ddrb;
	uint8_t timer;
	uint8_t timer_w;
	uint16_t settings; // bit 0-1 = divisor, bit 5 = enable int, bit 7 = PA7 int
	uint8_t ifr;
	uint8_t edc;
};

/*
	Rockwell 6532 VIA contains two 8 bits parallel ports (PIA),
	2 counter/timers ans some serial I/O lines
*/

// Initialisation at reset
void r6532_reset( struct Device *dev) {
	struct Via6532 *via;
	
}

// Creation of PIA
void r6532_init( char* name, int adr, char int_line) {
	struct Device *new;
	struct Via6532 *via;

	// Create a device and allocate space for data
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, name);
	new->type = R6532;
	new->addr = adr;
	new->end = adr+160;
	new->interrupt = int_line;
	via = mmalloc( sizeof( struct Via6532));
	new->registers = via;
	new->next = devices;
	devices = new;
	r6532_reset( new);
}

void r6532_run( struct Device *dev) {
// Call this every time around the loop
// Used for generating pulse on CA2 or CB2
// Pulse width is too large (next instruction exec time)
  struct Via6532 *via;
  via = dev->registers;
}

// handle reads from VIA registers
uint8_t r6532_read( struct Device *dev, tt_u16 reg) {
  struct Via6532 *via;
  via = dev->registers;
  if (reg & 0x80 == 0)	// RAM
	return via->ram[reg & 0x7F];

  if (reg & 0x04) {		// TIMER
  	if (reg & 0x01)
	  return via->ifr;
	else {
	  if (reg & 0x08)
		via->settings |= 0x08;
	  else
		via->settings &= 0xf7;
	  return via->timer;
	}
  } else {				// PIA
	switch (reg & 0x03) {
	  case 0: return via->dra;
	  case 1: return via->ddra;
	  case 2: return via->drb;
	  case 3: return via->ddrb;
	}
  }
}

// handle writes to PIA registers
void r6532_write( struct Device *dev, tt_u16 reg, uint8_t val) {
  struct Via6532 *via;
  via = dev->registers;
  if (reg & 0x80 == 0) {
	via->ram[reg & 0x7F] = val;
	return;
  }

  if (reg & 0x04) {
	if (reg & 0x10) {	// TIMER
		via->settings &= 0xf4;
		via->settings |= (reg & 0x0b);
		via->timer = val;
	} else {			// EDC
	  if (reg & 0x02)
	  	via->settings |= 0x80;
	  else
		via->settings &= 0x7f;
	  via->edc = val;
	}
  } else {				// PIA
	switch (reg & 0x03) {
	  case 0: via->dra = val; return;
	  case 1: via->ddra = val; return;
	  case 2: via->drb = val; return;
	  case 3: via->ddrb = val; return;
	}
  }
}

void r6532_reg( struct Device *dev) {
  struct Via6532 *via;
  int tdiv[] = {1, 8, 64, 1024};
  char iset[12];
  via = dev->registers;
  if (via->settings & 0x80)
	strcpy( iset, "timer");
  else
	*iset = 0;
  if (via->settings & 0x80)
	if (*iset)
	  strcat(iset, ", PA7");
	else
	  strcpy(iset, "PA7");
  if (*iset != 0)
	strcpy( iset, "none");

  printf( "\n           DRA:%02X, DDRA:%02X, DRB:%02X, DDRB:%02X",
		via->dra, via->ddra, via->drb, via->ddrb);
  printf( "\n           TIMER:%02X [/%dT], IFR:%02X, EDC:%02X, interrupt : %s\n",
		via->timer, tdiv[via->settings & 0x03], via->ifr, via->edc, iset);
}
