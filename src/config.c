//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder2
//
// Copyright (c) 2006, 2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      main.c
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2007-02-25 V0.01 kw start
//            2007-04-01 V0.02 kw added variables for Servo Curve
//            2007-05-07 V0.03 kw added Servo Repeat
//            2007-08-06 V0.04 changed to CV-struct
//            2010-09-14 V0.05 added reverser
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            here: global variables (RAM and EEPROM)
//
//            Note: all eeprom variables must be allocated here
//            (reason: AVRstudio doesn't handle eeprom directives
//            correctly)
//
// content:   A DCC-Decoder for ATmega8515 and other AVR
//
//
//------------------------------------------------------------------------

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>        // put var to program memory
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>


#include "config.h"
#include "hardware.h"
#include "dcc_receiver.h"
#include "timer1.h"

#include "main.h"

#define SIMULATION  0            // 0: real application
                                 // 1: test receive routine
                                 // 2: test timing engine
                                 // 3: test action

//----------------------------------------------------------------------------
// Timing Definitions:
// (note: every timing is given in us)

#define TICK_PERIOD 20000L       // 20ms tick for Timing Engine
                                 // => possible values for timings up to
                                 //    5.1s (=255/0.020)
                                 // note: this is also used as frame for
                                 // Servo-Outputs (OC1A and OC1B)

//----------------------------------------------------------------------------
// Global Data


volatile signed char timerval;          // generell timer tick, this is incremented
                                        // by Timer-ISR, wraps around. 1 Tick = 20 ms

volatile unsigned char timer1fired; // Indicates timer 1 has fired


volatile unsigned char Communicate = 0; // Communicationregister (for semaphors)
   

//-----------------------------------------------------------------------------
// data in EEPROM:
// Note: the order of these data corresponds to physical CV-Address
//       CV1 is coded at #00
//       see RP 9.2.2 for more information



    const unsigned char compilat[] PROGMEM = {"..... GBM AP ....."};

    t_cv_record CV EEMEM =
      {
        #include "cv_data_gbm.h"
      };


    const t_cv_record CV_PRESET PROGMEM =
      {
        #include "cv_data_gbm.h"
      };
