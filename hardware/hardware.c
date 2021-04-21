/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   hardware.c -- emulation of Motorola 68xx and Rockwell 65xx devices
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
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include <error.h>
#include <errno.h>

#include <stdlib.h>
#include "config.h"
#include "emu6809.h"
#include "hardware.h"

/*
 * Looking for a config file ".simc6809.ini" in login dir or current dir
 * for describing the memory mapping and hardware configuration
 * Defaut: ROM >= F000, RAM = 0000-EFFF, I/O = E000-E7FF, 6850 ACIA @ E100
 * I/O space may split RAM... devices are named and can be added further
 * S
 * # comment line begins with #, 
 * memory layout set by keywords mem <low> <high+1>, io <low> <high+1>, rom
 * other lines contain name of device followed by its base address and
 * interrupt line. For ACIA, also speed in bps (default to 9600)
 * name recognised: mc6840, mc6850, mc6820, mc6821, m6520, m6521, m6522, m6532
 * Ex: rom F800 # 2K of rom from F800 to FFFF
 *     mem 0000 8000 # 32 K ram @0000
 *     io E000 E100  # I/O space from E000 to E0FF 
 *     mc6840 E020 FIRQ # TIMER @ 0x020, connected to FIRQ
 *     mc6850 E000 IRQ 19200 # ACIA @ 0xe000, connected to IRQ, 19200 bps
 *     m6522 E040 # VIA @ 0xe040, interrupt line not connected
*/

// Default values

extern char *readstr(char **c);

// Default values, with mc6850 ACIA at 0xE100
int mem_low = 0;
int mem_high = 0xE000;
int io_low = 0xE000;
int io_high = 0xF000;
int rom = 0xF000;

int loading = 0;
struct Device *devices = NULL;

// show devices with their status
void showdev() {
  struct Device *dev = devices;
  char *itxt;
  int i;
  
  while( dev != NULL) {

	switch (dev->interrupt){
	case 'I': itxt = "IRQ"; break;
	  case 'F': itxt = "FIRQ"; break;
	  case 'N': itxt = "NMI"; break;
  	  default : itxt = "not connected"; break;
	}
	printf ("%s @ 0x%04X (interrupt: %s) ", dev->devname, dev->addr, itxt);
	
    switch (dev->type) {
	  case MC6850: mc6850_reg( dev); break;
	  case MC6840: mc6840_reg( dev); break;
	  case MC6820: mc6820_reg( dev); break;
	  case R6522:  r6522_reg( dev); break;
	  case R6532:  r6532_reg( dev); break;
	  case FD1795: fd1795_reg( dev); break;
	  default:     printf( "length=%04X\n", dev->end-dev->addr); break;
	}
	dev = dev->next;
  }
}

void verify_config()	// display devices emulated
{
  int halt = 0;
  struct Device *dev = devices;
  while( dev != NULL) {
	if (dev->addr < io_low || dev->end > io_high) {
	  printf( "Bad address for %s @ 0x%04X, outside I/O space [%04X-%04X[\n",
	  		dev->devname, dev->addr, io_low, io_high);
	  halt = 1;
	}
	dev = dev->next;
  }
  if (halt)
	exit( 1);
}

void get_config( uid_t uid) {
  char *filename;
  int n, param1, param2;
  FILE *fconf = NULL;
  char line[80];

  struct passwd *pw = getpwuid(uid);
  char int_line, *strptr, *strptr2, *keyword;
  int speed;
  char **tail;

  if (pw)
  {
    n = strlen( pw->pw_dir);
	filename = mmalloc( n + 16);
	strcpy( filename, pw->pw_dir);
	strcpy( filename + n, "/.sim6809.ini");
	if( (fconf = fopen( filename, "r")) == NULL) {
	  free( filename);
	  fconf = fopen( ".sim6809.ini", "r");
	}
  } else {
    printf( "Don't know who i am ! Aborting.\n");
	exit( 1);
  }

  if (fconf != NULL) { // Overwrite default values
    while( fgets( line, 80, fconf) != NULL) {
	  if (*line == '#')
	    continue;
	  strptr = line;
	  strptr = line;
	  keyword = readstr( &strptr);
	  strptr2 = keyword;
	  while (*strptr2) {
	    *strptr2 = toupper( *strptr2);
		strptr2++;
	  }
	  if (more_params( &strptr))
	    param1 = readhex( &strptr);
	  else
	    param1 = -1;
	  if (more_params( &strptr)) {
	  	int_line = toupper(*strptr);
		strptr2 = strptr;
	    param2 = readhex( &strptr);
	  } else {
	    int_line = 'X';
	    param2 = -1;
	  }
	  if (strcmp( keyword, "ROM") == 0)
	    rom = param1;
	  else if (strcmp( keyword, "MEM") == 0) {
		mem_low = param1;
		mem_high = param2;
	  } else if (strcmp( keyword, "IO") == 0) {
		io_low = param1;
		io_high = param2;
	  } else if (strcmp( keyword, "MC6840") == 0)
		mc6840_init( keyword, param1, int_line);
	  else if (strcmp( keyword, "MC6850") == 0) {
		while (isalpha( *strptr2))
		  strptr2++;
		if (more_params( &strptr2))
		  speed = strtol( strptr2, tail, 10);
		else
		  speed = 9600;
		mc6850_init( keyword, param1, int_line, speed);
	  } else if (strcmp( keyword, "MC6820") == 0)
		mc6820_init( keyword, param1, int_line);
	  else if (strcmp( keyword, "MC6821") == 0)
		mc6820_init( keyword, param1, int_line);
	  else if (strcmp( keyword, "R6520") == 0)
		mc6820_init( keyword, param1, int_line);
	  else if (strcmp( keyword, "R6521") == 0)
		mc6820_init( keyword, param1, int_line);
	  else if (strcmp( keyword, "R6522") == 0)
		r6522_init( keyword, param1, int_line);
	  else if (strcmp( keyword, "R6532") == 0)
		r6532_init( keyword, param1, int_line);
	  else if (strcmp( keyword, "FD1795") == 0)
		fd1795_init( keyword, param1, int_line, readstr( &strptr2));
	  else if (strcmp( keyword, "FAKE") == 0)
		fake_init( keyword, param1, param2);
	  else
	    printf( "Unrecognised device '%s' in '%s'\n", keyword, filename);

	  if (rom < 0)
		rom = 0x10000; // No rom ???
	  if (mem_low < 0)
	    mem_low = rom; // No ram ???
	  if (mem_high < 0)
	    mem_high = rom;
	  if (io_low < 0)  // No devices ???
	    io_low = rom;
	  if (io_high < 0)
	    io_high = rom;
	}
  } else {
    printf( "No config file, using default values...\n");
	mc6850_init( "MC6821", 0xE000, 'I', 9600);
  }
  verify_config();
}

// clock vs devices
void device_run() {
  struct Device *dev;
  dev = devices;
  while (dev != NULL) {
    switch (dev->type) {
	  case MC6850: mc6850_run( dev); break;
	  case MC6840: mc6840_run( dev); break;
	  case MC6820: mc6820_run( dev); break;
	  case R6522: r6522_run( dev); break;
	  case R6532: r6532_run( dev); break;
	  default: break;
	}
	dev = dev->next;
  }
}

// Search a device from its address
struct Device *look_dev( uint16_t adr)
{
  struct Device *dev;
  dev = devices;
  while (dev != NULL) {
  	if (adr >= dev->addr && adr < dev->end)
	  break;
	dev = dev->next;
  }
  return dev;
}

// reading a device
uint8_t read_device(uint16_t adr)
{
  struct Device *dev;
  if ((dev = look_dev( adr)) == NULL) {
    err6809 = ERR_NO_DEVICE;
	return 0;
  }
  switch (dev->type) {
    case MC6850: return mc6850_read( dev, adr);
    case MC6840: return mc6840_read( dev, adr);
    case MC6820: return mc6820_read( dev, adr);
    case R6522:  return r6522_read( dev, adr);
    case R6532:  return r6532_read( dev, adr);
    case FD1795: return fd1795_read( dev, adr);
    case FAKE:   return fake_read( dev, adr);
  }
}

// writing a device
extern void write_device(uint16_t adr, uint8_t val)
{
  struct Device *dev;
  if ((dev = look_dev( adr)) == NULL) {
    err6809 = ERR_NO_DEVICE;
	return;
  }
  switch (dev->type) {
    case MC6850: mc6850_write( dev, adr, val);return; 
    case MC6840: mc6840_write( dev, adr, val);return; 
    case MC6820: mc6820_write( dev, adr, val);return; 
    case R6522:  r6522_write( dev, adr, val);return; 
    case R6532:  r6532_write( dev, adr, val);return; 
    case FD1795: fd1795_write( dev, adr, val);return; 
    case FAKE:   fake_write( dev, adr, val);return; 
  }
}
