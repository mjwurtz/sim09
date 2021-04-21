/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   mc6820.c -- emulation of Motorola MC6820 PIA and compatible devices
   like Motorola MC6821, Rockwell R6520 and R6521
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

struct Pia {
	uint8_t ora;
	uint8_t piba;
	uint8_t ddra;
	uint8_t cra;
	int8_t ca2;
	int8_t setca2;
	uint8_t orb;
	uint8_t pibb;
	uint8_t ddrb;
	uint8_t crb;
	int8_t cb2;
	int8_t setcb2;
};

/*
   The 6820 family PIA registers are:
   $00 Data direction register A / Input + Output register A
   $01 control register A
   $02 Data direction register B / Input + Output register B
   $03 control register B
   Partial implementation : no interrupt without simulation of
   physical circuit. DDRA, DDRB, PA, PB, CA2 and CB2 (output) OK
*/

// Initialisation at reset
void mc6820_reset( struct Device *dev) {
	struct Pia *pia;
	
	pia = dev->registers;
	pia->cra = 0;
	pia->ddra = 0;
	pia->ora = 0;
	pia->piba = 0;
	pia->ca2 = 0;
	pia->setca2 =0;

	pia->crb = 0;
	pia->ddrb = 0;
	pia->orb = 0;
	pia->pibb = 0;
	pia->cb2 = 0;
	pia->setcb2 =0;
}

// Creation of PIA
void mc6820_init( char* name, int adr, char int_line) {
	struct Device *new;
	struct Pia *pia;

	// Create a device and allocate space for data
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, name);
	new->type = MC6820;
	new->addr = adr;
	new->end = adr+4;
	new->interrupt = int_line;
	pia = mmalloc( sizeof( struct Pia));
	new->registers = pia;
	new->next = devices;
	devices = new;
	mc6820_reset( new);
}

void mc6820_run( struct Device *dev) {
// Call this every time around the loop
// Used for generating pulse on CA2 or CB2
// Pulse width is too large (next instruction exec time)
  struct Pia *pia;
  pia = dev->registers;
  if (pia->setca2) {
    pia->ca2 = 0;
	pia->setca2 = 0;
  } else if (pia->cra & 0x38  == 0x28)
	pia->ca2 = 1;

  if (pia->setcb2) {
    pia->cb2 = 0;
	pia->setcb2 = 0;
  } else  if (pia->crb & 0x38  == 0x28)
	pia->cb2 = 1;
}

// handle reads from PIA registers
uint8_t mc6820_read( struct Device *dev, uint16_t reg) {
  struct Pia *pia;
  pia = dev->registers;
  switch( reg & 0x03) {
	case 0x00 :
	  if (pia->cra & 0x02)
		return pia->piba; // always 0...
	  else
		return pia->ddra;
	case 0x01 :
	  return pia->cra;
	case 0x02 :
	  if (pia->crb & 0x02)
		return pia->pibb;
	  else
		return pia->ddrb;
	case 0x03 :
	  return pia->crb;
  }
}

// handle writes to PIA registers
void mc6820_write( struct Device *dev, uint16_t reg, uint8_t val) {
  struct Pia *pia;
  pia = dev->registers;
  switch( reg & 0x03) {
	case 0x00 :
	  if (pia->cra & 0x02)
		pia->ora = val & pia->ddra;
	  else
		pia->ddra = val;
	  return;
	case 0x01 :
	  pia->cra = val;
	  if (val & 0x30)
		pia->ca2 = ((val & 0x38) == 0x38);
	  else if (val & 0x20)
	  	pia->setca2 = 1;
	  return;
	case 0x02 :
	  if (pia->crb & 0x02) {
		pia->orb = val & pia->ddrb;
		pia->pibb = val; // port B can read output latch
	  } else
		pia->ddrb = val;
	  return;
	case 0x03 :
	  pia->crb = val;
	  if (val & 0x30)
		pia->cb2 = ((val & 0x38) == 0x38);
	  else if (val & 0x20)
	  	pia->setcb2 = 1;
	  return;
  }
}

void mc6820_reg( struct Device *dev) {
  struct Pia *pia;
  pia = dev->registers;
  printf( "\n           CRA:%02X, DDRA:%02X, ORA:%02X, PIBA:%02X, CA2:%02X",
	pia->cra, pia->ddra, pia->ora, pia->piba, pia->ca2);
  printf( "\n           CRB:%02X, DDRB:%02X, ORB:%02X, PIBB:%02X, CB2:%02X\n",
	pia->crb, pia->ddrb, pia->orb, pia->pibb, pia->cb2);
}
