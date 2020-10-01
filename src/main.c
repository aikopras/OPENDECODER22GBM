//*****************************************************************************************************
//
// OpenDCC - OpenDecoder2.2
//
// Copyright (c) 2006, 2007 Kufer
// Copyright (c) 2011, 2012, 2013 AP
//
// This source file is subject of the GNU general public license 2,
// that is available at http://www.gnu.org/licenses/gpl.txt
// 
//*****************************************************************************************************
//
// file:      main.c
// author:    Wolfgang Kufer / Aiko Pras
// history:   2011-12-31 V0.01 ap first version
//
//*****************************************************************************************************
//
// purpose:   DCC Occupancy decoder for 8 tracks
//            here: init, mainloop
//
//
// ====== >>>> config.h  is the central definition file for the project
//
//
// content:   A DCC / RS-bus Occupancy decoder for ATmega16A PU and other AVR
//
//             1. Defines 
//             2. Init
//             3. Programming the initial RS-bus address
//             5. MAIN: analyze command, call the action, do programming
//
//*****************************************************************************************************
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>        // put var to program memory
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>

#include "global.h"              // global variables
#include "config.h"              // general definitions the decoder, cv's
#include "myeeprom.h"            // wrapper for eeprom
#include "hardware.h"            // port definitions for target
#include "dcc_receiver.h"        // receiver for dcc
#include "dcc_decode.h"          // decode dcc and handle CV access
#include "timer1.h"         	 // handling of LED control
#include "led.h"                 // LED specific functions
#include "relays.h"              // handling of relays
#include "cv_pom.h"              // Programming on the Main

#include "lcd.h"		 // Peter Fleury's LCD routines
#include "lcd_ap.h"		 // LCD messages to display speed or debugging messages

#include "adc_hardware.h"	 // Hardware for the Analog - Digital Converter
#include "occupancy.h"	 	 // For handling track occupancy data
#include "speed.h"	 	 // For measuring the train's speed on a special track

#include "rs_bus_hardware.h"	 // Hardware for the RS-bus feedback (UART, timer and interrupt)
#include "rs_bus_messages.h"	 // Analyse and collect feedback information 

#include "main.h"


//*****************************************************************************************************
//***************************************** AVR hardware ports ****************************************
//*****************************************************************************************************
// Port B is connected to the extension connector
// Depending on the type of decoder, initialisation settings can be overwritten in later functions
void init_hardware(void) {
    
    PORTD = (0<<LED)            // LED off
          | (0<<RSBUS_TX)       // output default off (UART controlled)
          | (1<<RSBUS_RX)       // 1 = pullup
          | (1<<DCCIN)          // 1 = pullup
          | (1<<NC1)            // 1 = pullup (pin is Not Connected)
          | (1<<NC2)            // 1 = pullup (pin is Not Connected)
          | (1<<PROGTASTER)     // 1 = pullup
          | (0<<DCC_ACK);       // ACK off

    DDRD  = (1<<LED)            // output
          | (1<<RSBUS_TX)       // output
          | (0<<RSBUS_RX)       // input (INT0)
          | (0<<DCCIN)          // input (INT1)
          | (0<<NC1)            // input (OC1B)
          | (0<<NC2)            // input (OC1A)
          | (0<<PROGTASTER)     // input    
          | (1<<DCC_ACK);       // output, sending 1 makes an ACK

    DDRA  = 0x00;               // PORTA: inputs
    DDRB  = 0xFF;               // PortB: All Bits as Output
    DDRC  = 0xFF;               // PORTC: All Bits as Output
       
    PORTA = 0x00;               // feedback: pull up
    PORTB = 0x00;               // output: all off, which means relays are off     
    PORTC = 0x00;               // output: all off
  }


//*****************************************************************************************************
//******************************* Programming after the button is pushed ******************************
//*****************************************************************************************************
// DoProgramming() is called when PROG is pressed
// -- manual programming and accordingly setting of RS-bus adress CV
//
void WaitDebounceTime(void) {
  // Busy waits till debouncing time is over
  // We choose as debouncing time 100 x 1 = 100ms
  signed int my_ms = 0;
  while(my_ms < 100) {
    _mydelay_us(1000);
    my_ms++;
  }
}

void DoProgramming(void) {
  unsigned int GlobalPortAddr;
  int Ticks_Waited = 0;
  WaitDebounceTime();                           // Busy wait debouncing time, for stable button pushed
  if (PROG_PRESSED) {                           // only act if key is still pressed after 100 ms
    turn_led_on();
    while(PROG_PRESSED) {      			// wait for release, and ...
      WaitDebounceTime();			// wait (again) 100 msec
      Ticks_Waited ++;
    }
    if (Ticks_Waited <= 50) {                   // button is released within 5 sec => programme address
      WaitDebounceTime();                       // Busy wait debouncing time, for stable button release
      while(!PROG_PRESSED) {
        if (semaphor_get(C_Received)) {         // Message
          analyze_message(&incoming);
          // CmdType == ANY_ACCESSORY_CMD => Accessory command but not for my current address 
          // CmdType == ACCESSORY_CMD     => Accessory command for my current address 
          if ((CmdType == ACCESSORY_CMD) || (CmdType == ANY_ACCESSORY_CMD)){
            // As opposed to the other OpenDecoder software, we do not store the accesory
            // address, but the RS-bus address, which will be in the range 1..128
            // The value 0 is reserved for an uninitialised RS-Bus address
            GlobalPortAddr = RecDecAddr * 4 + RecDecPort + 1;
            if (GlobalPortAddr <= 128) my_eeprom_write_byte(&CV.MyRsAddr, GlobalPortAddr);
            LED_OFF;
            // we got reprogrammed -> forget everthing running and restart decoder!
            _restart();
          }
        }
      }
    }
    else {                                        // button is pushed for more than 5 seconds => Reset
      ResetDecoder();                             // Is defined in cv_pom
      _restart();                                 // really hard exit
    }
  }
  return;   
}


//*****************************************************************************************************
//********************************* Initialisation of global variables ********************************
//*****************************************************************************************************
void init_global(void)
{ // Step 1: Determine the RS-Bus address. The valid range is 1..128
  // It can be 0, however, if the myRSAddr CV has not been initialised yet
  // In that case it can later be initialised via a PoM message
  My_RS_Addr = my_eeprom_read_byte(&CV.MyRsAddr);
  if (My_RS_Addr > 128) {My_RS_Addr = 0;}
  // Step 2: Determine the kind of accessory decoder addressing we react upon
  MyConfig = my_eeprom_read_byte(&CV.Config) & (1<<6);  // (0=basic, 1=extended)
  // Step 3: Determine the decoder type
  // Will b one of the following: TYPE_NORMAL, TYPE_REVERSER, TYPE_RELAYS or TYPE_SPEED
  MyType = my_eeprom_read_byte(&CV.DecType);
  // Step 4: Determine the decoder address, based on CV1 and CV9.
  // On the web there is some confusion regarding the exact relationship between the
  // decoder address within the DCC decoder hardware and CV1 and CV9. The convention used by 
  // my decoders is: My_Dec_Addr = CV1  + (CV9*64).
  // Note that this implies that the minimum value for CV1 is 0. However, if CV9 is 0 as well
  // CV1 should start from 1.
  // Thus we have the following:
  // The valid range of CV1 is 0..63
  // The valid range of CV9 is 0..7  (or 128, if the decoder has not been initialised)
  unsigned char cv1 = my_eeprom_read_byte(&CV.myAddrL);
  unsigned char cv9 = my_eeprom_read_byte(&CV.myAddrH);
  cv9 = cv9 & 0x07; 					// Select only the last three bits
  if (MyConfig != 1) My_Dec_Addr = (cv9 << 6) + cv1 - 1;	// Basic Acc. Addressing
  else My_Dec_Addr = (cv9 << 8) + cv1 - 1;			// Extended Acc. Addressing
  // The valid range of My_Dec_Addr is 0..511 (0..255 if Xpressnet is used).
  // Note that My_Dec_Addr will be 0 if the decoder has not been initialised  
  if ((cv1 > 63)) My_Dec_Addr = INVALID_DEC_ADR;
  if (My_Dec_Addr > 511) My_Dec_Addr = INVALID_DEC_ADR;
  if (my_eeprom_read_byte(&CV.myAddrH) & 0x80) My_Dec_Addr = INVALID_DEC_ADR;
  // Step 5: Determine the address for (Loco) PoM messages. Use My_RS_Addr
  My_Loco_Addr = My_RS_Addr + LOCO_OFFSET;
  if (My_Loco_Addr > (128 + LOCO_OFFSET)) {My_Loco_Addr = LOCO_OFFSET;}
  // Step 6: Initialise global variables
  CmdType = IGNORE_CMD;
}


//*****************************************************************************************************
//********************************************* Main loop *********************************************
//*****************************************************************************************************
int main(void)
  {
    init_hardware();			// setup hardware ports
    init_global();			// initialise the global variables

    init_dcc_receiver();		// setup dcc receiver
    init_dcc_decode();
    init_timer1();
    init_RS_hardware();
    init_occupied_tracks();
    init_occupancy();
    
    sei();				// Global enable interrupts

    if (MyType == TYPE_REVERSER) init_relays();
    if (MyType == TYPE_RELAYS)   init_relays();
    if (MyType == TYPE_SPEED)    init_speed_track();

    // check if the decoder has a valis RS-BUS address
    if ((My_RS_Addr == 0) || (My_RS_Addr > 128)) flash_led_fast(5);
 
    // init_lcd();
    // write_lcd_int(My_Dec_Addr);
    // write_lcd_int2(my_eeprom_read_byte(&CV.myAddrL));
    
    
    while(1) {
      if (PROG_PRESSED) DoProgramming();
      if (semaphor_query(C_Received)) {	// DCC message received
        analyze_message(&incoming);
        if (CmdType >= 1) {   
          if (CmdType == ANY_ACCESSORY_CMD) {;}
          if (CmdType == ACCESSORY_CMD)	{set_relay();}
          if (CmdType == LOCO_F0F4_CMD)	{set_relay();}
          if (CmdType == POM_CMD)	      {cv_operation(POM_CMD);}
          if (CmdType == SM_CMD) 	      {cv_operation(SM_CMD);}
        }
        semaphor_get(C_Received);	// now take away the protection
      }
      detect_occupied_tracks();		// prepare new AD conversion (runs every 1ms)
      if (timer1fired) {		// 1 time tick (20ms) has passed)
        handle_occupied_tracks();	// if track occupance changed, send RS-bus message / set reverser relays
        check_led_time_out();
        check_relays_time_out();
        check_PoM_time_out();
        timer1fired = 0;
        // Step 3: check actions for both of our Speed Measurement Tracks
        if (MyType == TYPE_SPEED) {check_speed_tracks();}
      }
    }
  }

//*****************************************************************************************************
