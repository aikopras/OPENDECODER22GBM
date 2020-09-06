//------------------------------------------------------------------------
//
// file:      rs_bus_hardware.h
//
// purpose:   Physical layer routines for the RS feedback bus
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2010-11-10 V0.1 Initial version
//
//--------------------------------------------------------------------------------------
// Global Data: 
volatile unsigned char RS_Layer_1_active;    // Flag to signal valid RS-bus signal 
volatile unsigned char RS_Layer_2_connected; // Flag to signal slave must connect to the master
volatile unsigned char RS_data2send_flag;    // Flag that this feedback module wants to send data
volatile unsigned char RS_data2send;         // Actual data byte that will be send over the RS-bus

volatile unsigned char T_Sample;             // Used by adc_hardware as interval between AD conversions
volatile unsigned char T_DelayOff;           // Used by adc_hardware as to time delay before OFF message

// Hardware initialisation and ISR routines
void init_RS_hardware(void);

//------------------------------------------------------------------------------------------------
