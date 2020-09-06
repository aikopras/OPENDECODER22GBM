//*****************************************************************************************************
//
// OpenDCC - OpenDecoder2.2
//
// Copyright (c) 2011, Pras
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
// 
//*****************************************************************************************************
//
// file:      relays.c
// author:    Aiko Pras
// history:   2012-01-08 V0.1 ap based upon port_engine.c from the OpenDecoder2 project
//
//
// A DCC Feedback Decoder for ATmega16A and other AVR. The decoder also supports switching four relays 
// (for normal use or for a reverser); the relays should be mounted on a separate board.
//
// The relays part of the decoder can operate in one of the following modes:
// 0x00110001 - Relays are switched by the decoder board, while operating as reverser
// 0x00110010 - Relays are switched after a switch command is received from the master station 
// Setting the mode can be controlled via CV27 (DecType).
//
// Uses the following global variables:
// - TargetDevice: the relays that is being addressed. Range: 0 .. (NUMBER_OF_DEVICES - 1)
//   For normal switch / relays4 decoders, the range will be 0..3
// - TargetGate: Targetted coil within that Port. Usually + or - / green or red
// - TargetActivate: Coil activation (value = 1) or deactivation (value = 0) 

//*****************************************************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#include "global.h"
#include "config.h"
#include "myeeprom.h"
#include "hardware.h"
#include "timer1.h"
#include "relays.h"
#include "led.h"

//*****************************************************************************************************
//************************************ Definitions and declarations ***********************************
//*****************************************************************************************************
#define RED 	          0     // The red coil 
#define GREEN 	          1     // The green coil 
#define UNKNOWN           2     // If we start up and do not know the position before power-down


typedef struct {
  unsigned char gate_pos;	// which of the two gates is currently on (RED or GREEN)
  unsigned char hold_time;	// maximum pulse duration to activate the gate (in 20 ms ticks)
  unsigned char rest_time;	// remaining puls duration during gate activation
} t_device;

t_device devices[4];		// we have 4 devices (switches, relays, ...) with each two coils  


//*****************************************************************************************************
//********************************** Local functions (called locally) *********************************
//*****************************************************************************************************
void init_relay(unsigned char device) {
  // device range: 0..3
  // initialise the "administration" for the requested relay
  devices[device].gate_pos = UNKNOWN;
  // store the maximum puls time
  devices[device].hold_time = my_eeprom_read_byte(&CV.T_on_F1 + device);
  // initialise the gate to a default position by setting the remaining puls time
  // never do this for switches, however...
  devices[device].rest_time = 0;
}

void init_relay_and_block(unsigned char device) {
  // Similar to above, but this time set the relay to a defined position
  // and subsequently block for roughly 50 ms
  // device range: 0..3
  // initialise the "administration" for the requested relay
  devices[device].gate_pos = RED;
  // store the maximum puls time
  devices[device].hold_time = my_eeprom_read_byte(&CV.T_on_F1 + device);
  // initialise the gate to a default position by setting the remaining puls time
  // never do this for switches, however...
  devices[device].rest_time = devices[device].hold_time;
  // do the actual setting of the coil now! Note we set the first (of both) coils
  unsigned char coil = device * 2;	
  RELAYS_PORT |= (1<<coil);	// activate the first gate (coil) of the relays
  // Wait for roughly 50 seconds now
  unsigned char number_of_ticks = 0;
  while (number_of_ticks < 3) {
    if (timer1fired) { 
      number_of_ticks ++;
      check_led_time_out();
      check_relays_time_out();
    }
    timer1fired = 0;
  }
}


//*****************************************************************************************************
//********************************* Main functions (called externally) ********************************
//*****************************************************************************************************
void init_relays(void) {
  // first disable all outputs
  RELAYS_PORT = 0x00;
  // initialise for all relays the "administration"
  // continue from the last relays positions before power-down.
  init_relay(0);
  init_relay(1);
  init_relay(2);
  init_relay(3);
  // note that we should call init_relay_and_block() in case we want to set the 
  // relays to a predefined position and wait 50 ms between setting each relay  
}


void set_relay(void) { 
  // This function is called from main, after a DCC accessory decoder command  
  // or a loco F1..F4 command is received
  // We do timer-based de-activation, so no need to react on de-activation messages 
  if (TargetActivate) {
    // Only react if the current gate position is different from the requested position
    if (devices[TargetDevice].gate_pos != TargetGate) {
      relays_led();
      // first deactivate all gates (coils)
      RELAYS_PORT &= ~(1<<(2*TargetDevice));	  // clear first gate (coil) of this device
      RELAYS_PORT &= ~(1<<(2*TargetDevice + 1));  // clear second gate (coil) of this device
      // select the active gate
      devices[TargetDevice].gate_pos = TargetGate;
      // set the activation time
      devices[TargetDevice].rest_time = devices[TargetDevice].hold_time;
      // Activate the gate (coil)
      RELAYS_PORT |= (1<<(2*TargetDevice + TargetGate));	// set the requested port
    }
  }
} 


void set_all_relays(unsigned char pos) {
  // This function is called from occupancy, after an occupied sensor track has been detected
  unsigned char i;
  // Do we need to change polarization?
  if (my_eeprom_read_byte(&CV.Polarization)) {
    if (pos) pos = 0;
    else pos = 1;
  }
  // Only react if at least one the current gate positions is different from the requested position
  if ((devices[0].gate_pos != pos) || (devices[1].gate_pos != pos) || 
      (devices[2].gate_pos != pos) || (devices[3].gate_pos != pos)) {
    relays_led();
    // first deactivate all gates (coils)
    RELAYS_PORT = 0x00;
    // for each device set the gate position and activation time
    for (i = 0; i < 4; i++) {
      devices[i].gate_pos = pos;
      devices[i].rest_time = devices[i].hold_time;
    }
    // Activate the gate (coil)
    if (pos == 1) RELAYS_PORT = 0b10101010;	// set the requested gates
    else 	  RELAYS_PORT = 0b01010101;	// set the other gates
  }
}


void check_relays_time_out(void) { 
  // This function is called from main, every time tick (20 ms)  
  unsigned char i;
  unsigned char rest_ticks;
  for (i=0; i<4; i++) {			// check each device (relays, switch, ...)
    rest_ticks = devices[i].rest_time;	// use a local variable to force compiler to tiny code
    if (rest_ticks !=0) {		// coil is active / active time is not over yet
      rest_ticks = rest_ticks - 1;	// decrease remaining time coil should still be active
      if (rest_ticks == 0) {		// coil should no longer be active?
        RELAYS_PORT &= ~(1<<(2*i));	// clear first gate (coil) of this device
        RELAYS_PORT &= ~(1<<(2*i + 1));	// clear second gate (coil) of this device
      }
      devices[i].rest_time = rest_ticks;
    }
  }
}

