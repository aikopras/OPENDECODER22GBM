//************************************************************************************************
//
// file:      speed.c
//
// purpose:   speed measurements routines
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2014-01-08 V0.1 Initial version
//
// Each GBM can support two speed measurment tracks. The time it took for a train to pass that
// track is measured, and since the length of that track is also known, the train's speed can 
// be determined. Speed will be indicated in km/h, compensating for the train's scale (H0)
//
// Called by:
// - init_speed_track() is called once from main during start up 
// - check_speed_tracks() is called from main every 20 ms
// 
// Will call:
// - write_lcd_string1() and write_lcd_string2 (lcd_ap) to display the results 
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
#include <stdio.h>		// since we use sprintf()
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
#include "adc_hardware.h"       // for reading values from the struct array adc_result
#include "timer1.h"       	// for start_up_phase() and time_for_next_feedback() 
#include "lcd_ap.h"		// to display the speed
#include "speed.h"	 	// to measure the train's speed on one of the special tracks

#include "main.h"

//************************************************************************************************
// Constant definitions
//************************************************************************************************
#define LCD_SIZE	16	// Number of characters on LCD display
#define DISPLAY_TIME	400	// Number of 20 msec ticks the result should be displayed
#define SCALE		87	// H0: 87 - N:160

// Do not modify below this line
// Define as global constants the values tracks[i].status may take
#define NOT_INITIALISED	0
#define INACTIVE	1
#define ACTIVE		2
#define SHOW		3
#define DONE		4
#define ERROR		255

//************************************************************************************************
// Define "global" variables for within this file
//************************************************************************************************
struct
{
  unsigned int  length;	// the length of this measurement track (in millimeters)
  unsigned char number;	// the ADC input port for this measurement track
  unsigned char next;	// the ADC input port that should be triggered to complete the measurement
  unsigned char status;	// if the measurement has already started, or if we're done or have errors
  unsigned int  time; 	// the time the train neede to pass this measurement track (in 40ms ticks)
} tracks[2];		// we have two measurement tracks


// The following variables are used to hold temporary data
unsigned char TA;	// Shortcut for tracks[i].number - 1 (ADC input port before)
unsigned char TB;	// Shortcut for tracks[i].number     (ADC input port of measurement track)
unsigned char TC;	// Shortcut for tracks[i].number + 1 (ADC input port after)
unsigned char TN;	// Shortcut for tracks[i].next       (ADC input port to be triggered)

//           TA                  TB                   TC
// ---------------------|==================|---------------------
//          (TN)                                     (TN)
//
//************************************************************************************************
// State tables that show the next status in case of certain conditions (+ is occupied track)
//************************************************************************************************
//
// Status:
//     INACTIVE			  ACTIVE / NEXT = TC		  ACTIVE / NEXT = TA
// TA  TB  TC	ACTION		 TA  TB  TC	ACTION		 TA  TB  TC	ACTION
// --------------------		 ---------------------		 ---------------------
// +   +   +	None		 +   +   +	SHOW		 +   +   +	SHOW
// +   +   -	NEXT=TC		 +   +   -	None		 +   +   -	SHOW
// +   -   +	None		 +   -   +	ERROR		 +   -   +	ERROR
// +   -   -	None		 +   -   -	ERROR		 +   -   -	ERROR
// -   +   +	NEXT=TA		 -   +   +	SHOW		 -   +   +	None
// -   +   -	None		 -   +   -	None		 -   +   -	None
// -   -   +	None		 -   -   +	ERROR		 -   -   +	ERROR
// -   -   -	None		 -   -   -	ERROR		 -   -   -	ERROR

//************************************************************************************************
// init_speed_track will be directly called from main externally
//************************************************************************************************
void init_speed_track(void) {
  init_lcd();
  write_lcd_string("OpenDecoder GBM");
  write_lcd_string2("Speed detection");
  // Read the CV values stored in FLASH memory
  unsigned char cv37 = my_eeprom_read_byte(&CV.Speed1_Out);
  unsigned char cv38 = my_eeprom_read_byte(&CV.Speed1_LL);
  unsigned char cv39 = my_eeprom_read_byte(&CV.Speed1_LH);
  unsigned char cv40 = my_eeprom_read_byte(&CV.Speed2_Out);
  unsigned char cv41 = my_eeprom_read_byte(&CV.Speed2_LL);
  unsigned char cv42 = my_eeprom_read_byte(&CV.Speed2_LH); 
  // Initialise the tracks array
  tracks[0].number = cv37 - 1; 		// CVs range from 1..8 / Pins from 0..7
  tracks[1].number = cv40 - 1; 
  tracks[0].length = (cv39 << 8) + cv38; 
  tracks[1].length = (cv42 << 8) + cv41;; 
  tracks[0].status = INACTIVE; 
  tracks[1].status = INACTIVE; 
  // Do some sanity checks 
  // 1: check if the CVs for the measurement pins have been initialised
  if (cv37 == 0) {tracks[0].status = NOT_INITIALISED;}
  if (cv40 == 0) {tracks[1].status = NOT_INITIALISED;}
  // 2: measurement tracks can not be on the "outmost" pins
  if ((tracks[0].number <= 0) || (tracks[0].number >= 7)) {tracks[0].status = NOT_INITIALISED;}
  if ((tracks[1].number <= 0) || (tracks[1].number >= 7)) {tracks[1].status = NOT_INITIALISED;}
  // 3: measurement tracks should have a decent length (in mm)
  if ((tracks[0].length < 100) || (tracks[0].length > 5000)) {tracks[0].status = NOT_INITIALISED;}
  if ((tracks[1].length < 100) || (tracks[1].length > 5000)) {tracks[1].status = NOT_INITIALISED;}
}


//************************************************************************************************
// LCD display string
//************************************************************************************************
char lcd_string[LCD_SIZE];		// Variable that holds the string to display

void clear_lcd_string(void) {
  int x;
  for (x = 0; x < LCD_SIZE; x++) {lcd_string[x] = ' ';}
}

//************************************************************************************************
// check speed for one of the tracks (i: 0..1)
//************************************************************************************************
void check_speed_track(unsigned char i) {
  TA = tracks[i].number - 1;
  TB = tracks[i].number;
  TC = tracks[i].number + 1;
  TN = tracks[i].next;
  // OPTION 1: The status is INACTIVE
  if (tracks[i].status == INACTIVE) {
    // check if train comes from left and we should become ACTIVE
    if ((adc_result[TA].is_on) && (adc_result[TB].is_on) && (adc_result[TC].is_off)) {
      tracks[i].next = TC;
      tracks[i].status = ACTIVE;
      tracks[i].time = 0;
    }
    // check if train comes from right and we should become ACTIVE
    if ((adc_result[TA].is_off) && (adc_result[TB].is_on) && (adc_result[TC].is_on)) {
      tracks[i].next = TA;
      tracks[i].status = ACTIVE;
      tracks[i].time = 0;
    }
  }
  // OPTION 2: The status ACTIVE
  else if (tracks[i].status == ACTIVE) {
    tracks[i].time ++;
    // STEP 2A: Make sure that the measurement track is still occupied
    if (adc_result[TB].is_off) {tracks[i].status = ERROR;}
    // STEP 2B: Check if the next track has been reached
    else if (adc_result[TN].is_on) {
      // yes, show the results
      tracks[i].status = SHOW;
      // calculate the speed. Use long long, which are 8 bytes
      long long l_time;
      long long l_speed;
      // Covert the 2 byte value to 8 bytes, to force the compiler to calculate with 8 bytes
      l_speed = tracks[i].length;
      // Make first the values as large as possible
      // 18 is the "compensation" factor, since we use 20 msec ticks and millimeters
      l_speed = l_speed * 18 * SCALE;
      l_time = tracks[i].time;
      l_time = l_time * 100;
      l_speed = l_speed / l_time;
      // Casts the long long to an unsigned integer
      unsigned int speed = l_speed;
      sprintf(lcd_string, "Speed: %u Km/h", speed);
      write_lcd_string_line(i, lcd_string);
      // reset the timer, and use if for timing out the LCD text
      tracks[i].time = 0;
    }
    // STEP 2C: Next track has not yet been reached, show progress
    else {
      // progress is shown by a moving "*"; the "*' is moved every 0,5 seconds
      // first clear the string that will be shown on the LCD display
      clear_lcd_string();
      // determine position for the "*" within the LCD display string
      // time is incremented every 20 ms, thus 25 times per 0,5 seconds
      int pos = (tracks[i].time / 25) % LCD_SIZE;
      lcd_string[pos] = '*';
      // show the string with the "*" on the LCD
      write_lcd_string_line(i, lcd_string);
    }
  }
  // OPTION 3: The status is SHOW
  else if (tracks[i].status == SHOW) {
    tracks[i].time ++;
    if (tracks[i].time > DISPLAY_TIME) {
      tracks[i].status = DONE;
    }
  }
  // OPTION 4: The status is DONE or ERROR
  else if ((tracks[i].status == DONE) || (tracks[i].status == ERROR)) {
    if ((adc_result[TA].is_off) && (adc_result[TB].is_off) && (adc_result[TC].is_off)) {
      tracks[i].status = INACTIVE;
      //write_lcd_string_line(i, "INACTIVE        ");
      clear_lcd_string();
      write_lcd_string_line(i, lcd_string);
    }
  }
}


//************************************************************************************************
// check_speed_tracks is called from main every 20 ms
//************************************************************************************************
void check_speed_tracks(void) {
  check_speed_track(0);
  check_speed_track(1);
}


//************************************************************************************************
