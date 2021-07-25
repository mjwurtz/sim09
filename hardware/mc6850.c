/* vim: set noexpandtab ai ts=4 sw=4 tw=4:
   mc6850.c -- emulation of Motorola MC6850 ACIA
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
#define __USE_POSIX199309

#include <sys/stat.h>
#include <time.h>

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
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

// ACIA_CLOCK : number of machine cycles needed for transmitting
// or receiving one character.
// If ACIA_CLOCK is not defined, this number is erived from the speed in bps.
// To speed the simulation, a small number can be used...

#define ACIA_CLOCK 0 // No wait for I/O

// Input conversion from LF to CR to make Flex system working with return key

#define FLEX

// Avoid using 100%cpu while waiting for input (select unusable
// since it returns allways 0 after testing a read on pts...
// To be avoided since even with ACIA_CLOCK 0, the system is very slow

// #define SLOWDOWN

#define ACIA_CR 0
#define ACIA_SR 0
#define ACIA_TDR 1
#define ACIA_RDR 1

/*
   The 6850 ACIA registers are:
   $00 control register / status register
   $01 transmit register / receive register
   RTS, CTS and DCD are not used, with the latter two held grounded
*/

struct Acia {
	uint8_t cr;
	uint8_t sr;
	uint8_t tdr;
	uint8_t rdr;
	uint32_t acia_cycles;	// number of cyles udes to transmit/receive a character
	uint32_t acia_clock_r;	// next time we can read
	uint32_t acia_clock_w;	// next time we can write 
} ;

char *ptsname(int);
int grantpt(int), unlockpt(int);
static int pts;
static FILE *xterm_stdout;
static FILE *acia_hardware;

#ifdef SLOWDOWN
static struct timespec delay, remain;
extern int nanosleep( const struct timespec *delay, struct timespec *remain);
#define SLOWDOWN_DELAY 50 // delay of 50 ns to avoid 100% CPU usage
#endif

void mc6850_init( char* devname, uint16_t adr, char int_line, int32_t speed) {
	struct Device *new;
	struct Acia *acia;

	// Create a device and allocate space for data
	new = mmalloc( sizeof( struct Device));
	strcpy( new->devname, "MC6850");
	new->type = MC6850;
	new->addr = adr;
	new->end = adr+2;
	new->interrupt = int_line;
	acia = mmalloc( sizeof( struct Acia));
	new->registers = acia;
	new->next = devices;
	devices = new;

	// Nb of cycles between characters function of speed in bps
#ifdef ACIA_CLOCK
	acia->acia_cycles = ACIA_CLOCK;
#else
	acia->acia_cycles = (int)((1e7/(float)(speed))+0.5);
#endif
	acia->acia_clock_r = cycles;	// start with a ready device
	acia->acia_clock_w = cycles;	// start with a ready device

// configure a pseudo terminal and print its name on the console
	char *slavename;

	int ptmx = open("/dev/ptmx", O_RDWR | O_NOCTTY);
//	printf ("ptmx = %d\n", ptmx);
	if (ptmx == -1) {
		printf("Failed to open pseudo-terminal master-slave for use with xterm. Aborting...\n");
		exit( 1); // closes any open streams and exits the program
	} else if (unlockpt( ptmx) != 0) {
		printf("Failed to unlock pseudo-terminal master-slave for use with xterm (errno. %d). Aborting...\n", errno);
		close(ptmx);
		exit( 1);
	} else if (grantpt(ptmx) != 0) {
		printf("Failed to grant access rights to pseudo-terminal master-slave for use with xterm (errno. %d). Aborting...\n", errno);
		close(ptmx);
		exit( 1);
	}

// open the corresponding pseudo-terminal slave (that's us)
	char *pts_name = ptsname(ptmx);
	printf("ACIA port: %s\n", pts_name);
	pts = open(pts_name, O_RDWR | O_NOCTTY);

	// Ensure that the echo is switched off 
	struct termios orig_termios;
	if (tcgetattr (pts, &orig_termios) < 0) {
		perror ("ERROR getting current terminal's attributes");
		exit( 1);
	}
		
	orig_termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON);
	orig_termios.c_oflag &= ~(ONLCR);
	orig_termios.c_cc[VTIME] = 0;
	orig_termios.c_cc[VMIN] = 0;
		
	int i = fcntl(pts, F_GETFL, 0);
		
	fcntl(pts, F_SETFL, i | O_NONBLOCK);
		
	if (tcsetattr (pts, TCSANOW, &orig_termios) < 0) {
		perror ("ERROR setting current terminal's attributes");
		exit( 1);
	}

// launch an xterm that uses the pseudo-terminal master we have opened
	char xterm_cmd[160];
	int count = sprintf(xterm_cmd, "xterm -bg black -fg green -fn \"-urw-nimbus mono-bold-r-normal--0-0-0-0-m-0-iso8859-1\" -S%s/%d", pts_name, ptmx);
	xterm_stdout = popen(xterm_cmd, "r");
	if (!xterm_stdout) {
		printf("Failed to open xterm process. Aborting...\n");
		ptmx = 0;
		close(ptmx);
		exit( 1);
	}

//	printf( "ACIA OK\n");
	char *s1 = "+------------------------------------------+\r\n";
	char *s2 = "| simc6809 v0.1 - Emulated MC6850 ACIA I/O |\r\n";
	char *s3 = "+------------------------------------------+\r\n";
	write( pts, s1, strlen( s1));
	write( pts, s2, strlen( s2));
	write( pts, s3, strlen( s3));

	char buf; // why pts input get something in the first 1/10 sec ?
#ifdef SLOWDOWN
	delay.tv_sec = 0;
	delay.tv_nsec = 100000000;	// Wait 1/10 sec and empty input buffer
	nanosleep( &delay, &remain);
	delay.tv_nsec = SLOWDOWN_DELAY;
#else
	sleep( 1);
#endif
	while (read( pts, &buf, 1) > 0);
}

void acia_destroy() {
	pclose( xterm_stdout);
	close(pts);
}

void mc6850_run( struct Device *dev) {
	// call this every time around the loop
	int i, n;
	char buf;
	struct Acia *acia;

	acia = dev->registers;
	if (cycles >= acia->acia_clock_w) {	// something to do ?

	// got a character to send?
	  if ((acia->sr & 0x02) == 0) {
		buf = acia->tdr;
		write(pts, &buf, 1);
		acia->sr |= 0x02;
		if ((acia->cr & 0x60) == 0x20) {
			acia->sr |= 0x80;
			switch (dev->interrupt) {
			  case 'F': firq(); break;
			  case 'I': irq(); break;
			  case 'N': nmi();
			  default: break;
			}
		}
		return;
	  }
	}
	
	if (cycles < acia->acia_clock_r) return;	// nothing to do yet

	// character ready in input buffer ?
	if ((acia->sr & 0x01) == 0) {
	  i = read(pts, &buf, 1);
	  if(i > 0) {
#ifdef FLEX
		if (buf == '\n')	// Unix to Flex conversion...
		  buf = '\r';
#endif
		acia->rdr = buf;
		acia->sr |= 0x01;
		if (acia->cr & 0x80) {
		  acia->sr |= 0x80;
		  switch (dev->interrupt) {
			case 'F': firq(); break;
			case 'I': irq(); break;
			case 'N': nmi();
			default: break;
		  }
		}
	  } else {
		acia->sr &= 0xFE;
#ifdef SLOWDOWN
		nanosleep( &delay, &remain);
#endif
	  }
	}
}

// handle reads from ACIA registers
uint8_t mc6850_read( struct Device *dev, uint16_t reg) {
  struct Acia *acia;
  acia = dev->registers;
	switch (reg & 0x01) {   // not fully mapped
	  case ACIA_SR:
		return acia->sr;
	  case ACIA_RDR:
		acia->sr &= 0x7e;	// clear IRQ, RDRF
		acia->acia_clock_r = cycles + acia->acia_cycles;
		return acia->rdr;
	}
	return 0xff;	// maybe the bus floats
}

// handle writes to ACIA registers
void mc6850_write( struct Device *dev, uint16_t reg, uint8_t val) {
  struct Acia *acia;
  acia = dev->registers;
	switch (reg & 0x01) {   // not fully mapped CR0->CR4 ignored
		case ACIA_CR:
			acia->cr = val;
			break;
		case ACIA_TDR:
			acia->tdr = val;
			acia->acia_clock_w = cycles + acia->acia_cycles;
			acia->sr &= 0x7d;	// clear IRQ, TDRE
			break;
	}
}

void mc6850_reg( struct Device *dev) {
  struct Acia *acia;
  char rdr, tdr;
  acia = dev->registers;
  if (isprint( acia->rdr))
  	rdr = acia->rdr;
  else
	rdr = '.';
  if (isprint( acia->tdr))
  	tdr = acia->tdr;
  else
	tdr = '.';
  printf( "CR:%02X, SR:%02X, RDR:'%c' (Ox%02X), TDR:'%c' (Ox%02X)\n",
  		acia->cr, acia->sr, rdr, acia->rdr, tdr, acia->tdr);
  printf( "                           read clock=%ld, write_clock=%ld, cycles=%ld\n",
  		acia->acia_clock_r, acia->acia_clock_w, cycles);
}
