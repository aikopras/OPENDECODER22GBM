//--------------------------------------------------------------------------------------------
//
// file:      adc_hardware.h
//
// purpose:   Header file for the Analog Digital Converion routines
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
//--------------------------------------------------------------------------------------------
void init_occupied_tracks(void);
void detect_occupied_tracks(void);

typedef struct {			// we use temporary buffer to "pre-process" the adc_port values	
  unsigned char is_on;			// the adc pin is high and stable
  unsigned char is_off;			// the adc pin is low for longer a period (>= delay off time)
} t_adc_result;

extern t_adc_result adc_result[8];	// we have eight feedback signals
