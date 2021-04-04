/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   r6522.c -- emulation of VIA Rockwell R6522
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

struct Via6522 {
	uint8_t orb;
	uint8_t irb;
	uint8_t ora;
	uint8_t ira;
	uint8_t ddra;
	uint8_t ddrb;
	uint8_t t1c_l;
	uint8_t t1c_h;
	uint8_t t1l_l;
	uint8_t t1l_h;
	uint8_t t2c_l;
	uint8_t t2c_h;
	uint8_t sr;
	uint8_t acr;
	uint8_t pcr;
	uint8_t ifr;
	uint8_t ier;
	int8_t ca2;
	int8_t setca2;
	int8_t cb1;
	int8_t setcb1;
	int8_t cb2;
	int8_t setcb2;
};

/*
	Rockwell 6522 VIA contains two 8 bits parallel ports (PIA),
	2 counter/timers ans some serial I/O lines
*/

// Initialisation at reset
void r6522_reset( struct Device *dev) {
	struct Via6522 *via;
	
	via = dev->registers;
	via->orb = 0;
	via->irb = 0;
	via->ora = 0;
	via->ira = 0;
	via->ddrb = 0;
	via->ddra = 0;
	via->sr = 0;
	via->acr = 0;
	via->pcr = 0;
	via->ifr = 0;
	via->ier = 0;

	via->ca2 = 0;
	via->setca2 =0;
	via->cb1 = 0;
	via->setcb1 =0;
	via->cb2 = 0;
	via->setcb2 =0;
}

// Creation of PIA
void r6522_init( char* name, int adr, char int_line) {
	struct Device *new;
	struct Via6522 *via;

	// Create a device and allocate space for data
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, name);
	new->type = R6522;
	new->addr = adr;
	new->end = adr+16;
	new->interrupt = int_line;
	via = mmalloc( sizeof( struct Via6522));
	new->registers = via;
	new->next = devices;
	devices = new;
	r6522_reset( new);
}

void r6522_run( struct Device *dev) {
// Call this every time around the loop
// Used for generating pulse on CA2 or CB2
// Pulse width is too large (next instruction exec time)
  struct Via6522 *via;
  via = dev->registers;
}

// handle reads from PIA registers
uint8_t r6522_read( struct Device *dev, tt_u16 reg) {
  struct Via6522 *via;
  via = dev->registers;
  switch( reg & 0x0f) {
	case 0x00 :
	case 0x0f :
		return via->irb; // always 0...
	case 0x01 :
	  return via->ira;
	case 0x02 :
	  return via->ddrb;
	case 0x03 :
	  return via->ddra;
	case 0x04 :
	  return via->t1c_l;
	case 0x05 :
	  return via->t1c_h;
	case 0x06 :
	  return via->t1l_l;
	case 0x07 :
	  return via->t1l_h;
	case 0x08 :
	  return via->t2c_l;
	case 0x09 :
	  return via->t2c_h;
	case 0x0a :
	  return via->sr;
	case 0x0b :
	  return via->acr;
	case 0x0c :
	  return via->pcr;
	case 0x0d :
	  return via->ifr;
	case 0x0e :
	  return via->ier;
  }
}

// handle writes to PIA registers
void r6522_write( struct Device *dev, tt_u16 reg, uint8_t val) {
  struct Via6522 *via;
  via = dev->registers;
  switch( reg & 0x0f) {
	case 0x00 :
	case 0x0f :
	  via->orb = val;
	  return;
	case 0x01 :
	  via->ira = val;
	  return;
	case 0x02 :
	  via->ddrb = val;
	  return;
	case 0x03 :
	  via->ddra = val;
	  return;
	case 0x04 :
	  via->t1c_l = val;
	  return;
	case 0x05 :
	  via->t1c_h = val;
	  return;
	case 0x06 :
	  via->t1l_l = val;
	  return;
	case 0x07 :
	  via->t1l_h = val;
	  return;
	case 0x08 :
	  via->t2c_l = val;
	  return;
	case 0x09 :
	  via->t2c_h = val;
	  return;
	case 0x0a :
	  via->sr = val;
	  return;
	case 0x0b :
	  via->acr = val;
	  return;
	case 0x0c :
	  via->pcr = val;
	  return;
	case 0x0d :
	  via->ifr = val;
	  return;
	case 0x0e :
	  via->ier = val;
	  return;
  }
}

void r6522_reg( struct Device *dev) {
  struct Via6522 *via;
  via = dev->registers;
  printf( "\n           PCR:%02X, DDRA:%02X, ORA:%02X, IRA:%02X, CA2:%02X",
		via->pcr, via->ddra, via->ora, via->ira, via->ca2);
  printf( "\n           CRB:%02X, DDRB:%02X, ORB:%02X, IRB:%02X, CB1:%02X, CB2:%02X",
		via->acr, via->ddrb, via->orb, via->irb, via->cb1, via->cb2);
  printf( "\n           T1C-L:%02X, T1C-H:%02X, T1L-L:%02X, T1L-H:%02X, CB1:%02X, CB2:%02X",
		via->t1c_l, via->t1c_h, via->t1l_l, via->t1l_h);
  printf( "\n           T2C-L:%02X, T12-H:%02X, SR:%02X, IFR:%02X, IER:%02X\n",
		via->t2c_l, via->t2c_h, via->sr, via->ifr, via->ier);
}
