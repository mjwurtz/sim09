/* vim: set noexpandtab ai ts=4 sw=4 tw=4: */
/* hardware.h -- emulation of various Motorola and Rockwell peripherals
   Copyright (C) 2021 Michel J Wurtz

   based on acia.h -- emulation of 6850 ACIA
   Copyright (C) 2012 Gordon JC Pearce

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

#ifndef __ACIA_H
#define __ACIA_H

#include <stdint.h>
#include "emu6809.h"

#define ACIA_CR 0
#define ACIA_SR 0
#define ACIA_TDR 1
#define ACIA_RDR 1

extern void acia_init();
extern void acia_destroy();
extern void acia_run();
uint8_t acia_rreg(int reg);
void acia_wreg(int reg, uint8_t val);

extern struct Acia {
	uint8_t cr;
	uint8_t sr;
	uint8_t tdr;
	uint8_t rdr;
	uint32_t acia_cycles;
	uint32_t acia_clock;
} acia;

#endif
