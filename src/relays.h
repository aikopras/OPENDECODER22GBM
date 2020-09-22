#ifndef _RELAYS_H_
#define _RELAYS_H_

//-------------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
// 
//-------------------------------------------------------------------------------

void init_relays(void);				// called from main
void set_relay(void);				// called from main 
void set_all_relays(unsigned char pos);		// called from occupancy
void check_relays_time_out(void);		// called from main

#endif
