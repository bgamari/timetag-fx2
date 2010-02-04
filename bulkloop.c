/**
 * Copyright (C) 2008 Ubixum, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 **/
#include <stdio.h>

#include <fx2regs.h>
#include <fx2macros.h>
#include <delay.h>
#include <autovector.h>
#include <setupdat.h>
#include <eputils.h>

#define SYNCDELAY() SYNCDELAY4


volatile bit got_sud;

void reset_toggle(int ep, int dir) {
	TOGCTL = ep | (dir << 4);
	SYNCDELAY();
	TOGCTL = ep | (dir << 4) | (1 << 5);
	SYNCDELAY();
}

void main() {
	got_sud = FALSE;

	// Renumerate
	RENUMERATE(); 

	CLRERRCNT = 1;
	CPUCS = 0x00; SYNCDELAY();
	//CPUCS = 0x12; SYNCDELAY(); // 48MHz, output to CLKOUT
	//IFCONFIG = 0xe3; SYNCDELAY(); // Configure IFCLK and enable slave FIFOs
	WAKEUPCS = 0x06; SYNCDELAY(); // Active-low polarity on WU2, enable as wake-up source

	USE_USB_INTS();
	ENABLE_SUDAV();
	ENABLE_HISPEED();
	ENABLE_USBRESET();
	
	// Enable stuff
	REVCTL = 0x03;

	// Configure endpoints
	EP2CFG = 0xa3; SYNCDELAY(); // Endpoint 2: OUT, bulk, 512 bytes, triple-buffered
	EP6CFG = 0xe3; SYNCDELAY(); // Endpoint 6: IN, bulk, 512 bytes, triple-buffered
	EP8CFG = 0xe2; SYNCDELAY(); // Endpoint 8: IN, bulk, 512 bytes, double-buffered

	// All others invalid
	//EP1INCFG  &= ~bmVALID; SYNCDELAY();
	EP1OUTCFG &= ~bmVALID; SYNCDELAY();
	EP4CFG    &= ~bmVALID; SYNCDELAY();

	// NAKALL all endpoints and reset
	FIFORESET = 0x80; SYNCDELAY();
	FIFORESET = 0x82; SYNCDELAY(); // EP2
	FIFORESET = 0x84; SYNCDELAY(); // EP4
	FIFORESET = 0x86; SYNCDELAY(); // EP6
	FIFORESET = 0x88; SYNCDELAY(); // EP8
	FIFORESET = 0x00; SYNCDELAY(); // Turn off NAKALL

	// Configure EP2 FIFO:
	OUTPKTEND = 0x82; SYNCDELAY();
	OUTPKTEND = 0x82; SYNCDELAY();
	OUTPKTEND = 0x82; SYNCDELAY(); // Arm endpoint
	EP2FIFOCFG = 0x10; SYNCDELAY(); // Auto Out, 8-bit data bus

	// Configure EP4 FIFO:
	EP4FIFOCFG = 0x00; SYNCDELAY(); // Just make sure WORDWIDE=0

	// Configure EP6 FIFO:
	// Data packets are 510=0x1fe bytes at most
	// This needs to be a multiple of 6 so we don't split up packets
	EP6FIFOCFG = 0x0C; SYNCDELAY(); // Auto In
	EP6AUTOINLENH = 0x01; SYNCDELAY();
	EP6AUTOINLENL = 0xFE; SYNCDELAY();

	// Configure EP8 FIFO:
	// Command replies are 8 bytes
	EP8FIFOCFG = 0x08; SYNCDELAY(); // Auto in
	EP8AUTOINLENH = 0x00; SYNCDELAY();
	EP8AUTOINLENL = 0x08; SYNCDELAY();

	// Setup endpoint status flags
	PINFLAGSAB = 0xe8; SYNCDELAY(); // FLAGA=0x8=FIFO2 Empty, FLAGB=0xe=FIFO6 Full
	PINFLAGSCD = 0x0f; SYNCDELAY(); // FLAGC=0xf=FIFO8 Full
	FIFOPINPOLAR = 0x00; SYNCDELAY(); // Use default (active-low) polarities

	IFCONFIG = 0xa3; SYNCDELAY(); // Configure IFCLK and enable slave FIFOs
	reset_toggle(2, 0);
	reset_toggle(6, 1);
	reset_toggle(8, 1);
	EA = 1; SYNCDELAY(); // Turn on interrupts

#if 0
	for (i=0; i<255; i++)
		EP6FIFOBUF[i] = i;
	EP6BCH = 0;
	EP6BCL = 255;
#endif

	while(TRUE) {

		if (got_sud) {
			handle_setupdata();
			got_sud = FALSE;
		}

		if (!(EP1INCS & 0x2)) {
			EP1INBUF[0] = EP6CS;
			EP1INBUF[1] = 0x00;
			EP1INBUF[2] = EP2468STAT;
			EP1INBUF[3] = 0x00;
			EP1INBUF[4] = EP6BCH;
			EP1INBUF[5] = EP6BCL;
			EP1INBUF[6] = 0x00;
			EP1INBUF[7] = ERRCNTLIM;

			EP1INBUF[8] =  0x00;
			EP1INBUF[9] =  0xAB;
			EP1INBUF[10] = 0xCD;
			EP1INBUF[11] = 0xEF;
			EP1INBUF[12] = 0x00;
			EP1INBUF[13] = 0x00;
			EP1INBUF[14] = 0x00;
			EP1INBUF[15] = 0x00;

			EP1INBC = 16;
		}

#define EP6_TEST 0
#if EP6_TEST
		if (!(EP6CS & 0x8)) { // Write until full
		//if (EP6CS & 0x4) { // Write one packet
			for (i=0; i<128; i++)
				EP6FIFOBUF[i] = i;
			EP6BCH = 0;
			EP6BCL = 128;
		}
#endif
	}
}

// return TRUE if you handle the command 
// you can directly get SETUPDAT[0-7] for the data sent with the command
BOOL handle_vendorcommand(BYTE cmd) {
	if (cmd == 0x1) {
		// Update endpoint 6 AUTOINLEN
		EP6AUTOINLENL = SETUPDAT[0] & 0xff; SYNCDELAY();
		EP6AUTOINLENH = SETUPDAT[1] & 0x07; SYNCDELAY();
		return TRUE;
	} else if (cmd == 0x2) {
		// Discard pending data in endpoint 6 fifo
		// First we need to disable AUTOIN
		EP6FIFOCFG &= ~0x08; SYNCDELAY();
		// Discard data
		INPKTEND = (1<<7) | 6; SYNCDELAY();
		// Re-enable AUTOIN
		EP6FIFOCFG &= ~0x08; SYNCDELAY();
		return TRUE;
	}
	return FALSE;
}

// We only support interface 0,0
BOOL handle_get_interface(BYTE ifc, BYTE* alt_ifc) {
	if (ifc == 0) {
		*alt_ifc = 0;
		return TRUE;
	} else
		return FALSE;
}
	
BOOL handle_set_interface(BYTE ifc, BYTE alt_ifc) { return ifc == 0 && alt_ifc == 0; }

// We only handle configuration 1
BYTE handle_get_configuration() { return 1; }

BOOL handle_set_configuration(BYTE cfg) { return cfg==1 ? TRUE : FALSE; }

void sudav_isr() interrupt SUDAV_ISR {
	got_sud = TRUE;
	CLEAR_SUDAV();
}

void usbreset_isr() interrupt USBRESET_ISR {
	handle_hispeed(FALSE);
	CLEAR_USBRESET();
}
void hispeed_isr() interrupt HISPEED_ISR {
	handle_hispeed(TRUE);
	CLEAR_HISPEED();
}

