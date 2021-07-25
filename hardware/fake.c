/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   fake.c -- fake device, works like a 4 bytes memory
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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <errno.h>

#include <stdlib.h>
#include "../emu/config.h"
#include "../emu/emu6809.h"
#include "hardware.h"

struct Fake {
	int32_t size;
	uint8_t *byte;
};

/*
   This is a dummy device that can be used for any device where I/O
   are not useful or to put memory space inside I/O space in case of
   special address segmentation
*/

// Initialisation at reset
void fake_reset( struct Device *dev) {
	struct Fake *reg;
	int i;
	
	reg = dev->registers;
	for (i = 0; i < reg->size; i++)
	  reg->byte[i] = 0;
}

// Creation of fake device
void fake_init( char* name, uint16_t adr, uint16_t end) {
	struct Device *new;
	struct Fake *reg;

	// Create a device and allocate space for data
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, name);
	new->type = FAKE;
	new->addr = adr;
	if (end > adr) 	// end = 0 if not defined
	  new->end = end;
	else
	  new->end = adr+4;
	printf ("adr = %04X, end = %04X, size = %04X\n", new->addr, new->end, (uint16_t)(new->end - new->addr));
	reg = mmalloc( sizeof( struct Fake));
	new->registers = reg;
	reg->size = (uint16_t)(new->end - new->addr);
	reg->byte = mmalloc( sizeof(uint8_t) * reg->size);
	new->next = devices;
	devices = new;
	fake_reset( new);
}

// handle reads
uint8_t fake_read( struct Device *dev, uint16_t adr) {
  struct Fake *reg;
  uint16_t pos;
  reg = dev->registers;
  pos = adr - dev->addr;
  return reg->byte[pos];
}

// handle writes to PIA registers
void fake_write( struct Device *dev, uint16_t adr, uint8_t val) {
  struct Fake *reg;
  uint16_t pos;
  reg = dev->registers;
  pos = adr - dev->addr;
  reg->byte[pos] = val;
  return;
}
