/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   r6532.c -- emulation of Rockwell R6532 RAM-I/O-TIMER (RIOT)
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

struct Riot {
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
	Rockwell 6532 RIOT contains two 8 bits parallel ports,
	2 counter/timers ans some serial I/O lines
*/

// Initialisation at reset
void r6532_reset( struct Device *dev) {
	struct Riot *riot;
	
}

// Creation of PIA
void r6532_init( char* name, int adr, char int_line) {
	struct Device *new;
	struct Riot *riot;

	// Create a device and allocate space for data
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, name);
	new->type = R6532;
	new->addr = adr;
	new->end = adr+160;
	new->interrupt = int_line;
	riot = mmalloc( sizeof( struct Riot));
	new->registers = riot;
	new->next = devices;
	devices = new;
	r6532_reset( new);
}

void r6532_run( struct Device *dev) {
// Call this every time around the loop
// Used for generating pulse on CA2 or CB2
// Pulse width is too large (next instruction exec time)
  struct Riot *riot;
  riot = dev->registers;
}

// handle reads from RIOT registers
uint8_t r6532_read( struct Device *dev, tt_u16 reg) {
  struct Riot *riot;
  riot = dev->registers;
  if (reg & 0x80 == 0)	// RAM
	return riot->ram[reg & 0x7F];

  if (reg & 0x04) {		// TIMER
  	if (reg & 0x01)
	  return riot->ifr;
	else {
	  if (reg & 0x08)
		riot->settings |= 0x08;
	  else
		riot->settings &= 0xf7;
	  return riot->timer;
	}
  } else {				// PIA
	switch (reg & 0x03) {
	  case 0: return riot->dra;
	  case 1: return riot->ddra;
	  case 2: return riot->drb;
	  case 3: return riot->ddrb;
	}
  }
}

// handle writes to PIA registers
void r6532_write( struct Device *dev, tt_u16 reg, uint8_t val) {
  struct Riot *riot;
  riot = dev->registers;
  if (reg & 0x80 == 0) {
	riot->ram[reg & 0x7F] = val;
	return;
  }

  if (reg & 0x04) {
	if (reg & 0x10) {	// TIMER
		riot->settings &= 0xf4;
		riot->settings |= (reg & 0x0b);
		riot->timer = val;
	} else {			// EDC
	  if (reg & 0x02)
	  	riot->settings |= 0x80;
	  else
		riot->settings &= 0x7f;
	  riot->edc = val;
	}
  } else {				// PIA
	switch (reg & 0x03) {
	  case 0: riot->dra = val; return;
	  case 1: riot->ddra = val; return;
	  case 2: riot->drb = val; return;
	  case 3: riot->ddrb = val; return;
	}
  }
}

void r6532_reg( struct Device *dev) {
  struct Riot *riot;
  int tdiv[] = {1, 8, 64, 1024};
  char iset[12];
  riot = dev->registers;
  if (riot->settings & 0x80)
	strcpy( iset, "timer");
  else
	*iset = 0;
  if (riot->settings & 0x80)
	if (*iset)
	  strcat(iset, ", PA7");
	else
	  strcpy(iset, "PA7");
  if (*iset != 0)
	strcpy( iset, "none");

  printf( "\n           DRA:%02X, DDRA:%02X, DRB:%02X, DDRB:%02X",
		riot->dra, riot->ddra, riot->drb, riot->ddrb);
  printf( "\n           TIMER:%02X [/%dT], IFR:%02X, EDC:%02X, interrupt : %s\n",
		riot->timer, tdiv[riot->settings & 0x03], riot->ifr, riot->edc, iset);
}
