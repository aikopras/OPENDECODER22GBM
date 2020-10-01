#ifndef _CV_DATA_GBM_H
#define _CV_DATA_GBM_H

//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder2.2
//
// Copyright (c) 2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      cv_data_gbm.h
// author:    Wolfgang Kufer / Aiko Pras
// history:   2007-08-03 V0.1 kw start
//            2007-08-18 V0.2 kw default for myADDR high changed to 0x80
//                               -> this means: unprogrammed    
//            2011-01-15 V0.3 ap initial values have been included for the new CVs: System and
//                               SkipUnEven. CV546 supports a new feedback method: RS-bus
//            2012-01-03 v0.4 ap New Cvs for GBM added.
//            2013-03-12 v0.5 ap The ability is added to program the CVs on the main (PoM).
//                               For this purpose, the feedback decoder listens to a loc decoder address
//                               that is equal to the RS-Bus address + 6000
//                               Transmission of PoM SET commands conforms to the NMRA standards.
//                               PoM VERIFY commands do NOT conform to the NMRA standards, since the
//                               CV Value is send back via the RS-Bus (a proprietary solution)
//                               The trick to listen to a loc address is needed since LENZ equipment
//                               nor to xpressnet spcification supports PoM for accessory decoders.
//            2020-09-21 v0.6 ap Several changes such that software can now also be programmed
//                               via the Arduino IDE. Software version updated to 0x10      
//
//-----------------------------------------------------------------------------
// data in EEPROM:
// Note: the order of these data corresponds to physical CV-Address
//       CV1 is coded at #00
//       see RP 9.2.2 for more information

// Content         Name         CV  Access Comment
   0x01,        // myAddrL       1  R/W    Accessory Address low (6 bits).
   0,           // cv514         2  R      not used
   5,           // T_on_F1       3  R      Hold time for relays 1 (in 20ms steps)
   5,           // T_on_F2       4  R      Same dor relays 2
   5,           // T_on_F3       5  R      Same dor relays 3
   5,           // T_on_F4       6  R      Same dor relays 4
   0x10,        // version       7  R      Software version. Should be > 7
   0x0D,        // VID           8  R/W    Vendor ID (0x0D = DIY Decoder
                                          // write value 0x0D = 13 to reset CVs to default values
   0x80,        // myAddrH       9  R/W    Accessory Address high (3 bits)
   0,           // MyRsAddress  10  R/W    RS-bus address (=Main) for this decoder (1..128 / not set: 0)
   0,           // DelayIn1     11  R/W    Delay in 10ms steps before sending OFF signal (like Lenz)
   0,           // DelayIn2     12  R/W    Same, for input 2. If 0, CV555 will be used instead
   0,           // DelayIn3     13  R/W    Same, for input 3. If 0, CV555 will be used instead
   0,           // DelayIn4     14  R/W    Same, for input 4. If 0, CV555 will be used instead
   0,           // DelayIn5     15  R/W    Same, for input 5. If 0, CV555 will be used instead
   0,           // DelayIn6     16  R/W    Same, for input 6. If 0, CV555 will be used instead
   0,           // DelayIn7     17  R/W    Same, for input 7. If 0, CV555 will be used instead
   0,           // DelayIn8     18  R/W    Same, for input 8. If 0, CV555 will be used instead
   1,           // CmdStation   19  R/W    To handle manufacturer specific address coding
						// 0 - Standard (such as Roco 10764)
						// 1 - Lenz
   0,           // RSRetry      20  R/W    Number of RS-Bus retransmissions
   0,           // SkipUnEven   21  R/W    Only Decoder Addresses 2, 4, 6 .... 1024 will be used
   0,           // cv534        22  R      not used
   0,           // Search       23  R/W    If 1: decoder LED blinks
   0,           // cv536        24  R      not used
   0,           // Restart      25  R/W    To restart (as opposed to reset) the decoder: use after PoM write
   0,           // DccQuality   26  R/W    DCC Signal Quality
   0b00110000,  // DecType      27  R/W    Decoder Type
						// 0b00110000 - Track Occupancy decoder
						// 0b00110001 - Track Occupancy decoder with reverser board
						// 0b00110010 - Track Occupancy decoder with relays board
						// 0b00110100 - Track Occupancy decoder with speed measurement
   0,           // BiDi         28  R      Bi-Directional Communication Config. Keep at 0.
                // Config       29  R      similar to CV#29; for acc. decoders
      (1<<7)                                    // 1 = we are accessory
    | (0<<6)                                    // 0 = we do 9 bit decoder adressing
    | (0<<5)                                    // 0 = we are basic accessory decoder
    | 0,                                        // 4..0: all reserved
   0x0D,        // VID_2        30  R      Second Vendor ID, to detect AP decoders
   0,           // cv543        31  R      not used
   0,           // cv544        32  R      not used

// CVs used by all variants of the Track Occupancy Decoder
   3,           // Min_Samples  33  R/W    Minimum number of samples a value should be stable
						//How many consequtive ON samples are needed with the same 
						//outcome before the result is considered to be stable. Note: 
						//every sample takes 8 msec, so "3" gives 24 msec extra delay
   15,          // Delay_off    34  R/W    Delay (in 100 ms steps) before previous occupancy will be relased
   20,          // Threshold_on 35  R/W    Above this value a previous OFF sample will be regarded as ON
   15,          // Threshold_of 36  R/W    Below this value a previous ON sample will be regarded as OFF
						// Note that the value of Threshold_of should lower than 
						// Threshold_on
						// Below some values for specific resistor values
						// 82K=7,  68K=8,  56K=10, 47K=12, 39K=15, 33K=18
						// 27K=22, 22K=28, 18K=34, 15K=41, 12K=52, 10K=68

// CVs used by the Speed Measurement specific variant of the Track Occupancy Decoder
   0,           // Speed1_Out   37  R/W    Track number (1..8) for the first speed measurement track (0=none)
   0,           // Speed1_LL    38  R/W    Length in cm of the first speed measurement track (low order byte)
   0,           // Speed1_LH    39  R/W    Length in cm of the first speed measurement track (high order byte)
   0,           // Speed2_Out   40  R/W    Track number (1..8) for the second speed measurement track (0=none)
   0,           // Speed2_LL    41  R/W    Length in cm of the second speed measurement track (low order byte)
   0,           // Speed2_LH    42  R/W    Length in cm of the second speed measurement track (high order byte)

// CVs used by the Reverser specific variant of the Track Occupancy Decoder
   0,           // FB_A         43  R/W    Feedback bit if track A is occupied
   1,           // FB_B         44  R/W    Feedback bit if track B is occupied
   2,           // FB_C         45  R/W    Feedback bit if track C is occupied
   3,           // FB_D         46  R/W    Feedback bit if track D is occupied
   0,           // FB_S1        47  R/W    Feedback bit if Sensor 1 is active
   1,           // FB_S2        48  R/W    Feedback bit if Sensor 2 is active
   1,           // FB_S3        49  R/W    Feedback bit if Sensor 3 is active
   2,           // FB_S4        50  R/W    Feedback bit if Sensor 4 is active
   0,           // Polarization 51  R/W    If 0: J&K connected normal / if 1: J&K polarization changed


#endif
