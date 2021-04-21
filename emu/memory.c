/* memory.c -- Memory emulation
   Copyright (C) 1998 Jerome Thoen

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


#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "emu6809.h"
#include "console.h"

#include "../hardware/hardware.h"

uint8_t *ramdata;    /* 64 kb of ram */

int memory_init(void)
{
  ramdata = (uint8_t *)mmalloc(0x10000);

  return 1;
}

uint8_t get_memb(uint16_t adr)
{
  // not hardware
  if (adr < io_low || adr >= io_high) {
    if (adr < mem_low || (adr >= mem_high && adr < rom) || adr > 0xFFFF) {
      printf( "read %04X mem_low %04X mem_high %04X, ROM %04X\n", adr, mem_low, mem_high, rom);
      err6809 = ERR_NO_MEMORY;
      return (0);
    }
    return ramdata[adr];
  } else {
	return read_device( adr);  // hardware mapper
  }
}

uint16_t get_memw(uint16_t adr)
{
  return (uint16_t)get_memb(adr) << 8 | (uint16_t)get_memb(adr + 1);
}

void set_memb(uint16_t adr, uint8_t val)
{
// Protecting some memory space
  if (loading) {
    ramdata[adr] = val;
	return;
  }
  if (adr >= rom) {
      printf( "write %04X mem_low %04X mem_high %04X, ROM %04X\n", adr, mem_low, mem_high, rom);
      err6809 = ERR_WRITE_PROTECTED;
    return;
  }

// managing memory available on simulated hardware
  if (adr < io_low || adr >= io_high) { // not inside io space ?
    if (adr < mem_low || adr >= mem_high) {
      printf( "write %04X mem_low %04X mem_high %04X, ROM %04X\n", adr, mem_low, mem_high, rom);
      err6809 = ERR_NO_MEMORY;
      return;
    }
    ramdata[adr] = val;
    return;
  } else
	write_device( adr, val);
}

void set_memw(uint16_t adr, uint16_t val)
{
  set_memb(adr, (uint8_t)(val >> 8));
  set_memb(adr + 1, (uint8_t)val);
}


