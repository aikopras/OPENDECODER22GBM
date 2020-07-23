//************************************************************************************************
//
// file:      adc_hardware.c
//
// purpose:   C file for the Analog Digital Converion routines
//	      Measures the voltage on the AVR feedback port, to detect track occupancy
//
// This source file is subject of the GNU general public license 2,
// that is available at http://www.gnu.org/licenses/gpl.txt
//
// history:   2013-04-20 V0.1 Initial version
// authors:   ap
//
// Calling:
// - init_occupied_tracks() is called once from main during start up 
// - detect_occupied_tracks is called from main as aften as possible
//   detect_occupied_tracks uses internal logic that decides upon the best moment to perform the ADC
// 
// Results:
// Are made available via adc_result[8]
// - is_on  == 1: Track is certainly occupied by train (spikes have already been filtered) 
// - is_off == 1: Track is certainly free (we already waited a certain time to filter bad rail contacts) 
//
// Intermediate information is stored in the local strcture called adc_port[8]
// - adc_history: the last bit (thus mask 0x01) holds the current value for that input pin 
//   0 => track is free / 1 => track is occupied
// - on_is_stable == 1: if the last bit of adc_history is 1 that value can be used 
// - delay_before_off == 0: if the last bit of adc_history is 0 that value can be used 
// - max_delay_before_off: copied from the CV variables
//
//************************************************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>	// put var to program memory
#include <avr/eeprom.h>
#include <avr/interrupt.h>
// header files for own code
#include "global.h"             // global variables
#include "config.h"		// general definitions the decoder, cv's
#include "myeeprom.h"           // wrapper for eeprom
#include "dcc_receiver.h"	// hardware related DCC functions (layer 1 / physical layer)
#include "rs_bus_hardware.h"	// hardware related RS-bus functions (layer 1 / physical layer)
// header file for this c file
#include "adc_hardware.h"	// for the struct t_adc_result


//************************************************************************************************
// Define global (=external) variables
//************************************************************************************************
t_adc_result adc_result[8];	// we have eight feedback signals


//************************************************************************************************
// Define local variables
struct {
  unsigned int  adc_value;		// store all ADC values (only needed for debugging)
  unsigned int  max_delay_before_off;	// integer (in steps of 10 msec). Initialised from CV(s)
  unsigned int  delay_before_off;	// integer (in steps of 10 msec). Current value
  unsigned char adc_history;		// store 8 consequtive raw binary results, to later filter spikes
  unsigned char on_is_stable;		// Filter spikes: check if a certain number of on samples are identical
} adc_port[8];				// we have eight ADC input pins.

// The following variables are initialised / derived from CV values 
unsigned char ADC_Input_Pin;	// Keeps track which ADC input should be converter (between 0..7)
unsigned char Threshold_On;	// If the ADC value is above this value, the track is occupied 
unsigned char Threshold_Off;	// If the ADC value is below this value, the track is free 
unsigned char Min_Samples_Mask; // Mask which we create from Min_Samples


//************************************************************************************************
// set_multiplex_register of AVR hardware 
//************************************************************************************************
void set_multiplex_register(unsigned char pin_number) { 
  // Rewrite the ADMUX, which controls the ADC multplexing
  // Initialise first the ADC reference voltage
  // Use Internal 2.56V Voltage Reference with external capacitor at AREF pin
  ADMUX = (1 << REFS1) | (1 << REFS0);    // Note: we rewrite entire register
  // Two most significant bits of result are in the ADCH, the remaining are in ADCL
  ADMUX |= (0 << ADLAR);                  // Note: we do not rewrite, but set specific bit
  // Initialise the multiplexer
  if (pin_number == 0) {ADMUX |= (0 << MUX2) | (0 << MUX1) | (0 << MUX0); }
  if (pin_number == 1) {ADMUX |= (0 << MUX2) | (0 << MUX1) | (1 << MUX0); }
  if (pin_number == 2) {ADMUX |= (0 << MUX2) | (1 << MUX1) | (0 << MUX0); }
  if (pin_number == 3) {ADMUX |= (0 << MUX2) | (1 << MUX1) | (1 << MUX0); }
  if (pin_number == 4) {ADMUX |= (1 << MUX2) | (0 << MUX1) | (0 << MUX0); }
  if (pin_number == 5) {ADMUX |= (1 << MUX2) | (0 << MUX1) | (1 << MUX0); }
  if (pin_number == 6) {ADMUX |= (1 << MUX2) | (1 << MUX1) | (0 << MUX0); }
  if (pin_number == 7) {ADMUX |= (1 << MUX2) | (1 << MUX1) | (1 << MUX0); }
}


//************************************************************************************************
// init_occupied_tracks will be directly called from main externally
//************************************************************************************************
void init_occupied_tracks(void) { 
  unsigned char Min_Samples;	// Minimum number of samples a positive value should be stable
  unsigned int i;               // for-loop counter
  // Step 1: Initialise ADC prescaler and the "ADMUX register"
  // With a X-tal of 11.0592 Mhz, and a preferred ADC clock frequency between 50-200Khz
  // we need a prescaler of 64. This will give an ADC clock frequency of 172,8 Khz
  // This is roughly 6 micro seconds
  // For details see: ATMega-16A manual (secton 22)
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0); 
  // put the ADC into single-running mode
  ADCSRA |= (0 << ADATE); 
  // Enable the ADC
  ADCSRA |= (1 << ADEN); 
  // Note that we still need to set the ADMUX register.
  // This is done, however, in "set_multiplex_register".
  // For that purpose, we use the ADC_Input_Pin variable, which is set here to zero
  ADC_Input_Pin = 0;
  // STEP 2: Read the CVs that hold the Threshold values
  Threshold_On  = my_eeprom_read_byte(&CV.Threshold_on);
  Threshold_Off = my_eeprom_read_byte(&CV.Threshold_of);
  if (Threshold_On < 10) Threshold_On = 10;
  if (Threshold_Off < 5) Threshold_Off = 5;
  // STEP 3: Read the minimum number of positive samples that need to be the same, before the signal
  // is considered to be stable. Ensure validity and use this number to calculate a mask
  Min_Samples  = my_eeprom_read_byte(&CV.Min_Samples);
  if (Min_Samples == 0) {Min_Samples = 1;}
  if (Min_Samples > 8 ) {Min_Samples = 8;}  
  Min_Samples_Mask = 1;
  for (i = 1; i < Min_Samples; i++) {Min_Samples_Mask = Min_Samples_Mask * 2 + 1;}
  // STEP 4: Read the delay related CVs. These are CV11-CV18 (Lenz) and CV34 (OpenDecoder GBM)
  // CV11-CV18 allow specification per input; CV34 specifies for all inputs together.
  // By default we use CV11-CV18, but when its value is 0 we use CV34 instead
  // Note that CV11-CV18 is specified in 10 msec steps, whereas CV34 is in 100 msec steps
  // Our code works in 10 msec steps; to allow sufficient resolution we store values in integers
  adc_port[0].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn1);   // CV11
  adc_port[1].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn2);   // CV12
  adc_port[2].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn3);   // CV13
  adc_port[3].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn4);   // CV14
  adc_port[4].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn5);   // CV15
  adc_port[5].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn6);   // CV16
  adc_port[6].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn7);   // CV17
  adc_port[7].max_delay_before_off = my_eeprom_read_byte(&CV.DelayIn8);   // CV18
  for (i = 0; i < 8; i++)
  { if (adc_port[i].max_delay_before_off == 0)   // use CV34; multiply to compensate 100 ms resolution
  {adc_port[i].max_delay_before_off = my_eeprom_read_byte(&CV.Delay_off) * 10;}
  }
  // Timer variable incremented each ms in rs_bus_hardware
  T_Sample = 0;			// The interval (in ms) between successive AD conversions
  T_DelayOff = 0;		// The delay (in ms) before an OFF message is considered stable
}


//************************************************************************************************
// detect_occupied_tracks is directly called from main externally, as frequent as possible
//************************************************************************************************
void detect_occupied_tracks(void) { 
  // We want to have at least 1 msec between successive runs of the AD converter.
  // For that purpose the counter T_Sample is incremented every 1 ms by the Timer 2 ISR
  // (see rs_bus_hardware.c). 
  // We check whether T_Sample >= 2 (instead of >=1) since the Timer 2 is running independently
  // (within an ISR) and may therefore immediately fire after we set T_Sample = 0.
  // All eight inputs will on average be read in 8 * 1,5 = 12 ms
  unsigned int adc_value;
  unsigned int relevant_samples;
  unsigned char occupied;
  unsigned char on_stable;
  unsigned char i;
  // STEP 1: Check whether Timer 2 has fired twice since previous invocation and the ADC is ready
  // If yes, read the value from the ADC input pin and initialise reading of the following pin
  if ((T_Sample >= 2) && ((ADCSRA & 0x40) == 0))
  { // STEP 1A: Read ADC value
    adc_value = ADCL + (ADCH * 256);                // Note: ADCL MUST be read before ADCH
    adc_port[ADC_Input_Pin].adc_value = adc_value;  // store value for debugging purposes
    // STEP 1B: convert integer into binary value, and add to "adc_history" (= set of bits).
    // Note that the case in which Threshold_off is erroneously made higher than Threshold_on
    // the code still works, although Threshold_off will be ignored.
    // If adc_value is between both Thresholds, we ignore its value
    if (adc_value > Threshold_On)
    { // Shift all bits in the adc_history one to the left, the bit at the right becomes 0 
      adc_port[ADC_Input_Pin].adc_history = (adc_port[ADC_Input_Pin].adc_history << 1);
      // Add 1 to the right of adc_history (set bit 1)
      adc_port[ADC_Input_Pin].adc_history |= 0x01;
    }
    else if (adc_value < Threshold_Off)
    { // Same as above. We do not have to clear the bit at the right, since the shift made it 0
      adc_port[ADC_Input_Pin].adc_history = (adc_port[ADC_Input_Pin].adc_history << 1);
    }
    // STEP 1C: analyse adc_history to see whether the "track on" signal is stable
    // Use a mask to select the number of samples that should be considered
    // If the masked value is the same as the mask itself, all samples are 1, thus  "on" is stable
    // If the masked value is 0, all samples are 0, thus  "off" is stable
    // In that case reinitialise the delay_before_off
    relevant_samples = adc_port[ADC_Input_Pin].adc_history & Min_Samples_Mask; // bitwise AND (mask)
    if (relevant_samples == Min_Samples_Mask) {adc_port[ADC_Input_Pin].on_is_stable = 1;}
    else 
      if (relevant_samples == 0) {adc_port[ADC_Input_Pin].on_is_stable = 0;}
      else
      { adc_port[ADC_Input_Pin].on_is_stable = 0;
        adc_port[ADC_Input_Pin].delay_before_off = adc_port[ADC_Input_Pin].max_delay_before_off;
      }
    // STEP 1D: Create the conclusion whether the ADC input pin is definitely ON
    // use for readability two temporary veriables
    occupied  = adc_port[ADC_Input_Pin].adc_history & 0x01;
    on_stable = adc_port[ADC_Input_Pin].on_is_stable;
    if ((occupied != 0) && (on_stable != 0)) adc_result[ADC_Input_Pin].is_on = 1;
    else adc_result[ADC_Input_Pin].is_on = 0;   
    // STEP 1F: initialise next AD conversion
    ADC_Input_Pin = (ADC_Input_Pin + 1) % 8;        // next pin, modulo 8
    set_multiplex_register(ADC_Input_Pin);
    new_adc_requested = 1;
    T_Sample = 0; // Reset 1 ms interval timer
  }
  // STEP 2: Once every 10 msec we should decrease all "delay_before_off" values
  // and determine whether the ADC input pin is definitely OFF 
  // For that purpose the Timer 2 routine also maintains the T_DelayOff variable 
  if (T_DelayOff >= 10) { 
    T_DelayOff = 0;  // Reset
    for (i = 0; i < 8; i++) {
      // initialise the result to 0
      adc_result[i].is_off = 0;
      // STEP 2A: Decrease the delay_before_off value
      if (adc_port[i].delay_before_off > 0) {
        adc_port[i].delay_before_off --;}
      // STEP 2B: Check if "is_off" may now be used
      if (adc_port[i].delay_before_off == 0) {
        occupied = adc_port[i].adc_history & 0x01;
        if (occupied == 0) adc_result[i].is_off = 1;
      }
    }
  }
}

//************************************************************************************************

