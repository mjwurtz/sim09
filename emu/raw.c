/* vim: set noexpandtab ai ts=4 sw=4 tw=4: */
/* raw.c -- Raw binary support (rom image)
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "config.h"
#include "emu6809.h"

void load_raw( char *filename, char *pos)
{
  long int bin_end, bin_pos, bin_len, addr;
  FILE *fi;
  int c;
	
  printf("loading %s ... ", filename);
  fi=fopen(filename,"r");
  if (fi==NULL)
  {
    printf("can't open it, sorry.\n");
    exit( 1);
  }
  fseek( fi, 0L, SEEK_END);
  if( (bin_len = ftell( fi)) < 0) {
	printf( "File error, exiting.\n");
	exit( 1);
  }
  if (pos[0] == '0' && pos[1] == 'x')
	sscanf( pos, "%lx", &bin_pos);
  else
	sscanf( pos, "%ld", &bin_pos);

  if( bin_pos == 0) 
	bin_pos = 0x10000 - bin_len;

  if( bin_pos + bin_len > 0x10000 || bin_pos < 0) {
	printf( "Position/length mismatch : 0x%04X/0x%04X, exiting.\n", bin_pos, bin_len);
	exit( 1);
  }

// reading and processing file...
  
  fseek( fi, 0L, SEEK_SET);
  for (addr = bin_pos; addr < bin_pos + bin_len; addr++) {
	c =  fgetc(fi);
	set_memb( addr, c);
    
  }

  printf( "0x%04X bytes loaded at Ox%04X.\n", bin_len, bin_pos);
  fclose(fi);
}
