/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   fd1795.c -- emulation of Western Digital FD1795 Floppy controler
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
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <error.h>
#include <errno.h>

#include "../emu/config.h"
#include "../emu/emu6809.h"
#include "hardware.h"

struct Fdc {
	char label[16];
	uint8_t *dsk;
	uint8_t cr;
	uint8_t sr;
	uint8_t track;
	uint8_t sector;
	uint8_t data;
	uint16_t pos;
};

static uint8_t readonly, nbtrk, nbsec, *ptr, *end, track_id;
static int fd;
static int count, stepdir;

// Initialisation at reset
void fd1795_reset( struct Device *dev) {
	struct Fdc *fdc;
	
	fdc = dev->registers;
	fdc->cr = 0;
	fdc->sr = readonly;
	fdc->track = 0;
	fdc->sector = 0;
	fdc->data = 0;
	fdc->pos = 0;
}

// Partial implementation :
// - only one disk supported (no provision for drive select)
// - no provision for track R/W
// - blocs are only 256 bytes
// - guessing number of tracks/sectors from file size may fail...

// Creation of Floppy Controler
void fd1795_init( char* name, int adr, char int_line, char *dskname) {
	struct Device *new;
	struct Fdc *fdc;
	struct stat dsk_stat;
	int flags;

	// Create a device and map data in memory
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, name);
	new->type = FD1795;
	new->addr = adr;
	new->end = adr+4;
	new->interrupt = int_line;
	fdc = mmalloc( sizeof( struct Fdc));
	new->registers = fdc;
	new->next = devices;
	devices = new;

	if (stat( dskname, &dsk_stat)) {
		printf( "disk image %s unreachable\n");
		readonly = 0x80;
	} else {
	  if (dsk_stat.st_mode & S_IWUSR) {
		readonly = 0;
		fd = open( dskname, O_RDONLY);
		flags = PROT_READ | PROT_WRITE;
	  } else {
		readonly = 0x40;
		fd = open( dskname, O_RDONLY);
		flags = PROT_READ;
	  }
	  fdc->dsk = NULL;
	  fdc->dsk = mmap( &fdc->dsk, dsk_stat.st_size, flags, MAP_PRIVATE, fd, 0);
	  nbtrk = fdc->dsk[0x226];
	  nbsec = fdc->dsk[0x227];
	  for (int i=0; i<8; i++)
	    fdc->label[i] = fdc->dsk[i+0x210];
	  fdc->label[8] = 0;
	  printf( "disk %s, label '%s', %d tracks, %d sectors %s\n",
	  	dskname, fdc->label, nbtrk+1, nbsec, readonly?"(READONLY)":"");
	}
	fd1795_reset( new);
}

// handle reads from Floppy Controler registers
uint8_t fd1795_read( struct Device *dev, tt_u16 reg) {
  struct Fdc *fdc;
  uint8_t buf;
  fdc = dev->registers;
  switch( reg & 0x03) {
	case 0x00 :
	  return fdc->sr;
	case 0x01 :
	  return fdc->track;
	case 0x02 :
	  return fdc->sector;
	case 0x03 :
	  if (ptr != NULL && ptr < end)
	    fdc->data = *ptr++; 
	  if (ptr == end)
	    fdc->sr &= 0xFC;
	  return fdc->data;
  }
}

// handle writes to Floppy Controler Registers
void fd1795_write( struct Device *dev, tt_u16 reg, uint8_t val) {
  struct Fdc *fdc;
  uint8_t cmd;

  fdc = dev->registers;
  switch( reg & 0x03) {
	case 0x00 :
	  fdc->cr = val;
	  cmd = val & 0xf0;
	  switch (cmd) {
	    case 0x00:		// Restore
		  track_id = fdc->track = 0;
		  fdc->sr = readonly & 0x24;
		  ptr = NULL;
		  break;
		case 0x10:	// SEEK
		  if (fdc->data > nbtrk)
		  	track_id = fdc->track = nbtrk;
		  else
		  	track_id = fdc->track = fdc->data;
		  if (track_id)
		    fdc->sr = readonly & 0x20;
		  else
		    fdc->sr = readonly & 0x24;
		  ptr = NULL;
		  break;
		case 0x30:	// STEP
		  fdc->track += stepdir;
		  if (fdc->track > nbtrk)
			fdc->track = nbtrk;
		  if (fdc->track == 0xff)
			fdc->track = 0;
		case 0x20:	// id, but no track register update
		  track_id += stepdir;
		  if (track_id > nbtrk)
			track_id = nbtrk;
		  if (track_id == 0xff)
			track_id = 0;
		  if (track_id)
		    fdc->sr = readonly & 0x20;
		  else
		    fdc->sr = readonly & 0x24;
		  ptr = NULL;
		  break;
		case 0x50:	// STEP IN  // @TODO verify range
		  if (fdc->track <= nbtrk)
		    fdc->track++;
		case 0x40:	// id, but no track register update
		  stepdir = 1;
		  if (track_id <= nbtrk)
		    track_id++;
		  fdc->sr = readonly & 0x20;
		  ptr = NULL;
		  break;
		case 0x70:	// STEP OUT  // @TODO verify range
		  if (fdc->track)
		    fdc->track--;
		case 0x60:	// id, but no track register update
		  stepdir = -1;
		  if (track_id)
		    track_id--;
		  if (track_id)
		    fdc->sr = readonly & 0x20;
		  else
		    fdc->sr = readonly & 0x24;
		  ptr = NULL;
		  break;
		case 0x80:	// READ SECTOR
		  ptr = fdc->dsk + (track_id * nbsec + fdc->sector - 1) * 256;
		  end = ptr + 256;
		  fdc->sr |= 0x03;
		  break;
		case 0x90:	// READ MULTIPLE
		  ptr = fdc->dsk + (track_id * nbsec + fdc->sector - 1) * 256;
		  end = fdc->dsk + (track_id + 1) * 256;
		  fdc->sr |= 0x03;
		  break;
		case 0xA0:	// WRITE SECTOR
		  ptr = fdc->dsk + (track_id * nbsec + fdc->sector - 1) * 256;
		  end = ptr + 256;
		  fdc->sr |= 0x03;
		  break;
		case 0xB0:	// WRITE MULTIPLE
		  ptr = fdc->dsk + (track_id * nbsec + fdc->sector - 1) * 256;
		  end = fdc->dsk + (track_id + 1) * 256;
		  fdc->sr |= 0x03;
		  break;
		case 0xC0:	// READ ADDRESS
		  fdc->data = track_id;
		  ptr = NULL;
		  break;
		case 0xE0:	// READ TRACK - not implemented
		  printf( "Read track %d - not implemented !\n", track_id);
		  ptr = NULL;
		  break;
		case 0xF0:	// WRITE TRACK - not implemented
		  printf( "write track %d - not implemented !\n", track_id);
		  ptr = NULL;
		  break;
		case 0xD0:	// FORCE INTERRUPT
		  fdc->sr &= 0xFD;
		  ptr = NULL;
		  break;
	  }
	  return;
	case 0x01 :
	  fdc->track = val;
	  return;
	case 0x02 :
	  fdc->sector = val;
	  return;
	case 0x03 :
	  fdc->data = val;
	  if (ptr != NULL && ptr < end)
	    *ptr++ = val; 
	  if (ptr == end)
	    fdc->sr &= 0xFC;
	  return;
  }
}

void fd1795_reg( struct Device *dev) {
  struct Fdc *fdc;
  fdc = dev->registers;
  printf( "SR:%02X,CR:%02X, track=%d, sector=%d, track_id=%d, data:%02X\n",
	fdc->sr, fdc->cr, fdc->track, fdc->sector, track_id, fdc->data);
}
