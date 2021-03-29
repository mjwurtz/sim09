/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   hardware.c -- emulation of 68xx and 65xx hardware
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

struct Device *devices = NULL;

/*
 * Looking for a config file ".sim6809.ini" in login dir or current dir
 * for describing the memory mapping and hardware configuration
 * Defaut: ROM >= F000, RAM = 0000-EFFF, I/O = E000-E7FF, 6850 ACIA @ E100
 * I/O space may split RAM... devices are named and can be added further
 * S
 * # comment line begins with #, 
 * memory layout set by keywords mem <low> <high+1>, io <low> <high+1>, rom
 * other lines contain name of device followed by its base address and
 * interrupt line. For ACIA, also speed in bps (default to 9600)
 * name recognised: m6840, m6850, m6820, m6821, m6520, m6521, m6522, m6532
 * Ex: rom F800 # 2K of rom from F800 to FFFF
 *     mem 0000 8000 # 32 K ram @0000
 *     io E000 E100  # I/O space from E000 to E0FF 
 *     m6840 E020 FIRQ # TIMER @ 0x020, connected to FIRQ
 *     m6850 E000 IRQ 19200 # ACIA @ 0xe000, connected to IRQ, 19200 bps
 *     m6522 E040 # VIA @ 0xe040, interrupt line not connected
*/

// Default values

extern char *readstr(char **c);

int mem_low = 0;
int mem_high = 0xF000;
int io_low = 0xE000;
int io_high = 0xE800;
int rom = 0xF000;

int loading = 0;

void m6850_init( char* devname, int adr, char int_line, int speed){
  printf ("device %s (type %d) @ 0x%04X, %ld bds, interrupt: %c\n", devname, M6850, adr, speed, int_line);
}
void new_device( int devtype, char* devname, int adr, char int_line) {
  printf ("device %s (type %d) @ 0x%04X interrupt: %c\n", devname, devtype, adr, int_line);
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
  }

  if (fconf != NULL) { // Overwrite default values
    while( fgets( line, 80, fconf) != NULL) {
	  if (*line == '#')
	    continue;
	  strptr = line;
	  while (*strptr) {
	    *strptr = toupper( *strptr);
		strptr++;
	  }
	  strptr = line;
	  keyword = readstr( &strptr);
	  if (more_params( &strptr))
	    param1 = readhex( &strptr);
	  else
	    param1 = -1;
	  if (more_params( &strptr)) {
	  	int_line = *strptr;
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
	  } else if (strcmp( keyword, "M6840") == 0)
		new_device( M6840, keyword, param1, int_line);
	  else if (strcmp( keyword, "M6850") == 0) {
	  while (isalpha( *strptr2))
		strptr2++;
		if (more_params( &strptr2))
		  speed = strtol( strptr2, tail, 10);
		else
		  speed = 9600;
		m6850_init( keyword, param1, int_line, speed);
	  } else if (strcmp( keyword, "M6820") == 0)
		new_device( M6820, keyword, param1, int_line);
	  else if (strcmp( keyword, "M6821") == 0)
		new_device( M6820, keyword, param1, int_line);
	  else if (strcmp( keyword, "R6520") == 0)
		new_device( M6820, keyword, param1, int_line);
	  else if (strcmp( keyword, "R6521") == 0)
		new_device( M6820, keyword, param1, int_line);
	  else if (strcmp( keyword, "R6522") == 0)
		new_device( R6522, keyword, param1, int_line);
	  else if (strcmp( keyword, "R6532") == 0)
		new_device( R6532, keyword, param1, int_line);
	  else
	    printf( "Unrecognised keyword '%s' in '%s'\n", keyword, filename);
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
//	  verify_devices();
	}
  }
}

// show devices with their status
void dump_dev() {
//	printf( "m6850 : status =  %02x ........\n", acia.sr);
}
