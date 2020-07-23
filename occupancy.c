//************************************************************************************************
//
// file:      occupancy.c
//
// purpose:   file for the track occupancy handling routines
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2010-11-10 V0.1 Initial version
//            2011-02-06 V0.2 First complete production version
//            2013-04-20 V0.3 All ADC code removed. Generalized to allow reversers
//
// This code can be used to send feedback information from decoder to master station via
// the RS-bus. This code implements the datalink layer routines (define the byte contents).
// For details on the operation of the RS-bus, see: 
// http://www.der-moba.de/index.php/RS-R%C3%BCckmeldebus
//
// Called by:
// - init_occupancy() is called once from main during start up 
// - handle_occupied_tracks() is called from main every 20 ms
// 
// Will call:
// - set_all_relays() will be called to set the reverser relays 
//
// Input data used:
// Reads adc_result[8], which is maintained within adc_hardware,c
// - is_on  == 1: Track is certainly occupied by train (spikes have already been filtered) 
// - is_off == 1: Track is certainly free (we already waited a certain time to filter bad rail contacts) 
//
//************************************************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>	// put var to program memory
#include <avr/io.h>		// needed for UART
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/parity.h>	// Needed to calculate the parity bit

#include "global.h"             // global variables
#include "config.h"		// general definitions the decoder, cv's
#include "myeeprom.h"           // wrapper for eeprom
#include "hardware.h"		// port definitions for target
#include "dcc_receiver.h"	// hardware related DCC functions (layer 1 / physical layer)
#include "led.h"                // LED specific functions
#include "adc_hardware.h"       // for reading values from the struct array adc_result
#include "rs_bus_hardware.h"	// hardware related RS-bus functions (layer 1 / physical layer)
#include "rs_bus_messages.h"    // for sending RS-bus nibbles via format_and_send_RS_data_nibble() 
#include "timer1.h"       	// for start_up_phase() and time_for_next_feedback() 
#include "relays.h"       	// to set the reverser relays 

#include "lcd.h"		// Peter Fleury's LCD routines
#include "lcd_ap.h"		// Included by AP for debugging purposes

#include "main.h"
#include "rs_bus_messages.h"


//************************************************************************************************
// Define the bits of the RS-bus "packet" (1 byte)
//************************************************************************************************
// Define the bits of the RS-bus "packet" (1 byte)
// Note: least significant bit (LSB) first. Thus the parity bit comes first,
// immediately after the USART's start bit. Because of this (unusual) order, the USART hardware
// can not calculate the parity bit itself; such calculation must be done in software.
#define DATA_0          7       // feedback 1 or 5
#define DATA_1          6       // feedback 2 or 6
#define DATA_2          5       // feedback 3 or 7
#define DATA_3          4       // feedback 4 or 8
#define NIBBLE          3       // low or high order nibble
#define TT_BIT_0        2       // this bit must always be 0
#define TT_BIT_1        1       // this bit must always be 1
#define PARITY          0       // parity bit; will be calculated by software


//************************************************************************************************
// Define "global" variables for within this file
//************************************************************************************************
struct
{
  unsigned char should_be_on;    	 // according to our hardware the bit should be on
  unsigned char should_be_off;    	 // according to our hardware the bit should be off
  unsigned char previous_transmitted;    // this position has previously been send to the master
  unsigned char next_to_transmit;        // this position will be send next to the master
  unsigned char number_of_transmissions; // 0: nothing needs to be send anymore
  					 // >1: info needs to be send / Note: a higher value is
 					 // used to transmit multiple times (forward error correction)
} feedback[8];                  	 // we have eight feedback signals

// The following array is used to map to adc pins to RS-Bus feedback bits (needed since we have sensor tracks)
unsigned char map[8];			 // Note that multiple adc pins may map upon the same feedback bit

// The following variable is initialised from CV RSRetry
unsigned char RS_tranmissions;	// Number of times a RS-bus message is transmitted


//************************************************************************************************
// init_occupancy will be directly called from main externally
//************************************************************************************************
void init_occupancy(void) {
  // Step 1: Determine number of times the same RS-feedback nibble will be transmitted
  // Minimum is 1, but if CV.RSRetry > 0 the nibble will be retransmitted (forward error correction)
  RS_tranmissions = 1 + my_eeprom_read_byte(&CV.RSRetry);   
  if (RS_tranmissions > 3 ) {RS_tranmissions = 3;}
  // Step 2: initialise the mapping between the 8 ADC input pins and the 8 feedback bits 
  if (MyType == TYPE_REVERSER) {
    map[0] = my_eeprom_read_byte(&CV.FB_A);	// Track A
    map[1] = my_eeprom_read_byte(&CV.FB_S1);	// Sensor 1
    map[2] = my_eeprom_read_byte(&CV.FB_S2);	// Sensor 2
    map[3] = my_eeprom_read_byte(&CV.FB_B);	// Track B
    map[4] = my_eeprom_read_byte(&CV.FB_S3);	// Sensor 3
    map[5] = my_eeprom_read_byte(&CV.FB_S4);	// Sensor 4
    map[6] = my_eeprom_read_byte(&CV.FB_C);	// Track C
    map[7] = my_eeprom_read_byte(&CV.FB_D);	// Track D
  }
  else { // direct mapping between ADC input pins and RS-Bus feedback bits
    map[0] = 0;
    map[1] = 1;
    map[2] = 2;
    map[3] = 3;
    map[4] = 4;
    map[5] = 5;
    map[6] = 6;
    map[7] = 7;
  }
}


//************************************************************************************************
// Step 2A: check the ADC output (adc_result) if any action is needed
//************************************************************************************************
void analyse_track_occupation(void) {
  // Is called by handle_occupied_tracks, and acts as interface between the 
  // ADC specific code and the RS-bus code
  unsigned char i;	   // for loop counter for adc_result[8] and feedback[8]
  unsigned char previous;  // Technically not needed, but makes reading easier
  // Step 1: Reverser actions
  if (MyType == TYPE_REVERSER) {
    // sensor track 1 and / or 2 is occupied
    if ((adc_result[1].is_on) || (adc_result[2].is_on)) set_all_relays(1);
    if ((adc_result[4].is_on) || (adc_result[5].is_on)) set_all_relays(0);
    } 
  // Step 2: RS-Bus actions.  
  // Step 2A: ititialise for all feedback bits the "soll" value
  for (i = 0; i < 8; i++) {
    feedback[i].should_be_on = 0;	// initial value: no track occupied
    feedback[i].should_be_off = 1;	// initial value: all tracks free
  }
  // Step 2B: set for each ADC input pin the corresponding feedback bit. 
  // Multiple input pins may be mapped upon the same feedback bit
  for (i = 0; i < 8; i++) {
    // if one of the tracks associated with this feedback bit is occupied, this bit should become 1
    if (adc_result[i].is_on > 0) {feedback[map[i]].should_be_on = 1;}
    // if one of the tracks associated with this feedback bit is not free, this bit should become 0
    if (adc_result[i].is_off == 0) {feedback[map[i]].should_be_off = 0;}
  }
  // Step 2C: Check for each RS-Bus feedback bit if RS-Bus action is needed
  for (i = 0; i < 8; i++) {
    previous = feedback[i].previous_transmitted;
    if (feedback[i].should_be_on && (previous == 0)) {
      // change detected: is now on
      feedback[i].next_to_transmit = 1;
      feedback[i].number_of_transmissions = RS_tranmissions;
    }
    if (feedback[i].should_be_off && (previous != 0)) {
      // change detected: is now off
      feedback[i].next_to_transmit = 0;
      feedback[i].number_of_transmissions = RS_tranmissions;
    }
  }
}


//************************************************************************************************
// The routine "save_changes" is called by RS_connect() and send_feedbacks()
//************************************************************************************************
void save_changes(unsigned char start, unsigned char end) {
  // This function saves the changes for the feedbacks between "start" and "end"
  // after the nibble to which they belong has been send to the master
  unsigned char i;
  for (i = start; i <= end; i++) { 
    feedback[i].previous_transmitted = feedback[i].next_to_transmit;
    if (feedback[i].number_of_transmissions > 0)   {feedback[i].number_of_transmissions --;} }
}


//************************************************************************************************
// Step 2B: connect the decoder to the master station
//************************************************************************************************
void RS_connect(void) {
  // Register feedback module: send low and high order nibble in 2 consequtive cycles
  // Note that interference on the AVR's input lines during AVR restart requires us 
  // to wait till all feedback signals have become stable. Note that spurious resets on the
  // AVR 644A have also been detected (the AVR signals "brown-out" reset, although no power
  // problems can be measured), which means that the AVR can also be restarted during normal
  // operation. Therefore we have to make sure we always send correct (thus stable) values.
  unsigned char nibble;
  if (RS_Layer_1_active)  // wait till RS-bus is active 
  {
    // send first nibble
    while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
    nibble = (feedback[0].next_to_transmit<<DATA_0)
           | (feedback[1].next_to_transmit<<DATA_1)
           | (feedback[2].next_to_transmit<<DATA_2)
           | (feedback[3].next_to_transmit<<DATA_3)
           | (0<<NIBBLE);
    save_changes(0,3);
    format_and_send_RS_data_nibble(nibble);      
    // send second nibble
    while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
    nibble = (feedback[4].next_to_transmit<<DATA_0)
           | (feedback[5].next_to_transmit<<DATA_1)
           | (feedback[6].next_to_transmit<<DATA_2)
           | (feedback[7].next_to_transmit<<DATA_3)
           | (1<<NIBBLE);             
    save_changes(4,7);
    format_and_send_RS_data_nibble(nibble);
    RS_Layer_2_connected = 1;		// This module should now be connected to the master station
  }
}


//************************************************************************************************
// Step 2C: send a RS-Bus feedback message
//************************************************************************************************
unsigned char send_needed(unsigned char start, unsigned char end) {
  // This function checks if one or more RS-bus messages needs to be send
  // Needed to deal with retransmisisons
  unsigned char i, result;
  result = 0;                         // initial assumption: no feedback signal has changed
  for (i = start; i <= end; i++) {    // does this assumption hold for all feedback signals?
    if (feedback[i].number_of_transmissions > 0) {result = 1;} }
  return result;
}


void send_feedbacks(void)
{ // We send a feedback nibble to the master station if:
  // 1: the USART has completed transmission of the previous data
  // 2: at least one of the feedback signals of that nibble has changed
  unsigned char nibble;
  // check if we may send data (thus the USART has completed transmission of the previous data)
  if (RS_data2send_flag == 0) 
  { 
    // For the GBM we use a single feedback address for all 8 feedback signals
    if (send_needed(0,3)) 
    {
      nibble = (feedback[0].next_to_transmit<<DATA_0)
      | (feedback[1].next_to_transmit<<DATA_1)
      | (feedback[2].next_to_transmit<<DATA_2)
      | (feedback[3].next_to_transmit<<DATA_3)
      | (0<<NIBBLE);
      save_changes(0,3);
      format_and_send_RS_data_nibble(nibble);
    } 
    else if (send_needed(4,7))
    {
      nibble = (feedback[4].next_to_transmit<<DATA_0)
      | (feedback[5].next_to_transmit<<DATA_1)
      | (feedback[6].next_to_transmit<<DATA_2)
      | (feedback[7].next_to_transmit<<DATA_3)
      | (1<<NIBBLE);             
      save_changes(4,7);
      format_and_send_RS_data_nibble(nibble);
    }
  }
}


//************************************************************************************************
// The handle_occupied_tracks routine is called from main
//************************************************************************************************
void handle_occupied_tracks(void) {
  // Is called from main every 20 ms
  if (time_for_next_feedback()) {
    // around 40 ms have passed since we tried to send a message
    // Step 1: check the ADC output (adc_result) if any action is needed
    // Possible actions include: 
    // - preparation of RS-bus messages (not sending!) 
    // - setting Reverser relays
    analyse_track_occupation();
    // Step 2: send RS-bus messages, if needed 
    if (My_RS_Addr == 0) return;
    // check if the start-up phase is over, to ensure values will be stable
    if (start_up_phase()) return;
    // If needed, connect the RS-Bus to the master station
    // If already connected to the master station, send RS-Bus feedback messages
    if (RS_Layer_2_connected == 0) RS_connect();
      else send_feedbacks();
  }
}

//************************************************************************************************
