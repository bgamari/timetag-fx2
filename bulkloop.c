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
#include <setupdat.h>
#include <eputils.h>
#include <usbjt.h>

#define SYNCDELAY() SYNCDELAY4


volatile bit got_sud;

void main() {
	got_sud = FALSE;

	// Renumerate
	RENUMERATE(); 

	SETCPUFREQ(CLK_48M);
	SETIF48MHZ();

	FIFOPINPOLAR = 0x00; // Default polarities
	SYNCDELAY();
	IFCONFIG = 0xe3; // Enable slave FIFOs
	SYNCDELAY();

	USE_USB_INTS();
	ENABLE_SUDAV();
	ENABLE_HISPEED();
	ENABLE_USBRESET();
	
	// Enable stuff
	REVCTL = 0x03;

	// Configure endpoints
	EP2CFG = 0xa2; // Endpoint 2: OUT, bulk, 512 bytes, double-buffered
	SYNCDELAY();
	EP6CFG = 0xe0; // Endpoint 6: IN, bulk, 512 bytes, quad-buffered
	SYNCDELAY();
	EP8CFG = 0xe2; // Endpoint 8: IN, bulk, 512 bytes, double-buffered
	SYNCDELAY();

	// All others invalid
	EP1INCFG &= ~bmVALID;
	SYNCDELAY();
	EP1OUTCFG &= ~bmVALID;
	SYNCDELAY();
	EP4CFG &= ~bmVALID;
	SYNCDELAY();

	// NAKALL from all endpoints
	FIFORESET = 0x80;

	// Configure EP2 FIFO:
	FIFORESET = 0x82; // Reset
	SYNCDELAY();
	// Arm endpoint
	OUTPKTEND = 0x82;
	SYNCDELAY();
	OUTPKTEND = 0x82;
	SYNCDELAY();
	EP2FIFOCFG = 0x10; // Auto Out, 8-bit data bus
	SYNCDELAY();

	// Configure EP4 FIFO:
	FIFORESET = 0x84;
	SYNCDELAY();
	EP4FIFOCFG = 0x00; // Just make sure WORDWIDE=0
	SYNCDELAY();

	// Configure EP6 FIFO:
	FIFORESET = 0x86;
	SYNCDELAY();
	// Data packets are 512 bytes
	EP6FIFOCFG = 0x08; // Auto In
	SYNCDELAY();
	EP6AUTOINLENL = 0;
	SYNCDELAY();
	EP6AUTOINLENH = 2;
	SYNCDELAY();

	// Configure EP8 FIFO:
	FIFORESET = 0x88;
	SYNCDELAY();
	// Command replies are 8 bytes
	EP8FIFOCFG = 0x08; // Auto in
	SYNCDELAY();
	EP8AUTOINLENL = 8;
	SYNCDELAY();
	EP8AUTOINLENH = 0;
	SYNCDELAY();

	// Setup endpoint status flags
	PINFLAGSAB = 0xe8; // FLAGA=0x8=FIFO2 Empty, FLAGB=0xe=FIFO6 Full
	SYNCDELAY();
	PINFLAGSCD = 0x0f; // FLAGC=0xf=FIFO8 Full
	SYNCDELAY();

	FIFORESET = 0x00; // Turn off NAKALL
	SYNCDELAY();
	EA = 1; // Turn on interrupts
	SYNCDELAY();

	while(TRUE) {
		if (got_sud) {
			handle_setupdata();
			got_sud = FALSE;
		}
       	}
}

// return TRUE if you handle the command 
// you can directly get SETUPDAT[0-7] for the data sent with the command
BOOL handle_vendorcommand(BYTE cmd) { cmd=cmd; return FALSE; }

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

// None of these are enabled but are still necessary to shut-up the compiler
void sof_isr () interrupt SOF_ISR {}
void sutok_isr() interrupt SUTOK_ISR {}
void suspend_isr() interrupt SUSPEND_ISR {}
void ep0ack_isr() interrupt EP0ACK_ISR {}
void ep0in_isr() interrupt EP0IN_ISR {}
void ep0out_isr() interrupt EP0OUT_ISR {}
void ep1in_isr() interrupt EP1IN_ISR {}
void ep1out_isr() interrupt EP1OUT_ISR {}
void ep2_isr() interrupt EP2_ISR {}
void ep4_isr() interrupt EP4_ISR {}
void ep6_isr() interrupt EP6_ISR {}
void ep8_isr() interrupt EP8_ISR {}
void ibn_isr() interrupt IBN_ISR {}
void ep0ping_isr() interrupt EP0PING_ISR {}
void ep1ping_isr() interrupt EP1PING_ISR {}
void ep2ping_isr() interrupt EP2PING_ISR {}
void ep4ping_isr() interrupt EP4PING_ISR {}
void ep6ping_isr() interrupt EP6PING_ISR {}
void ep8ping_isr() interrupt EP8PING_ISR {}
void errlimit_isr() interrupt ERRLIMIT_ISR {}
void ep2isoerr_isr() interrupt EP2ISOERR_ISR {}
void ep4isoerr_isr() interrupt EP4ISOERR_ISR {}
void ep6isoerr_isr() interrupt EP6ISOERR_ISR {}
void ep8isoerr_isr() interrupt EP8ISOERR_ISR {}
void spare_isr() interrupt RESERVED_ISR {}
void ep2pf_isr() interrupt EP2PF_ISR {}
void ep4pf_isr() interrupt EP4PF_ISR {}
void ep6pf_isr() interrupt EP6PF_ISR {}
void ep8pf_isr() interrupt EP8PF_ISR {}
void ep2ef_isr() interrupt EP2EF_ISR {}
void ep4ef_isr() interrupt EP4EF_ISR {}
void ep6ef_isr() interrupt EP6EF_ISR {}
void ep8ef_isr() interrupt EP8EF_ISR {}
void ep2ff_isr() interrupt EP2FF_ISR {}
void ep4ff_isr() interrupt EP4FF_ISR {}
void ep6ff_isr() interrupt EP6FF_ISR {}
void ep8ff_isr() interrupt EP8FF_ISR {}
void gpifdone_isr() interrupt GPIFDONE_ISR {}
void gpifwf_isr() interrupt GPIFWF_ISR {}

