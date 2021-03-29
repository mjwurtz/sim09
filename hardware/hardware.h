/* vim: set noexpandtab ai ts=4 sw=4 tw=4: */
/* hardware.h -- emulation of various Motorola and Rockwell peripherals
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

extern int mem_low ;	// base address of physical memory emulated
extern int mem_high ;	// upper limit of physical memory emulated
extern int io_low ;		// start of memory used by I/O devices
extern int io_high ;	// upper limit of memory used by I/O devices
extern int rom ;		// base address of rom (allways on top of memory)

extern int loading ;

extern struct Device {
	char devname[16];
	int type;
	int addr;
	int end;
	char interupt;
	uint8_t *registers;
	struct Device *next;
	} *devices;

extern void dump_dev();
extern void m6850_init( char* devname, int adr, char int_line, int speed);
extern void m6840_init( char* devname, int adr, char int_line);
extern void m6820_init( char* devname, int adr, char int_line);
extern void r6822_init( char* devname, int adr, char int_line);
extern void r6832_init( char* devname, int adr, char int_line);

// interrupt classification, 'X' for interrupt line not connected
#define IRQ 'I'
#define FIRQ 'F'
#define NMI 'N'

// Main peripherals kown, other can be added
#define M6840 1
#define M6850 2
#define M6820 4	// <=> M6821, R6520, R6521
#define R6522 5
#define R6532 6

