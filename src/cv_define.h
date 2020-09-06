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
// file:      cv_data_servo.h
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2007-08-03 V0.1 kw start
//            2011-01-15 V0.2 ap New CV have been added: System. 
//                               Note that "reserved CVs" have been (mis)used for this purpose
//                               System is included to correct "errors" made by LENZ LZV100
//                               master stations. Although this correction will not be needed if 
//                               the decoder is used in the traditional way (controlling four 
//                               switches via four subsequent addresses, no RS-bus feedback), it 
//                               will improve operation if it is used together with 
//                               LENZ LZV100 master stations.
//            2012-01-03 v0.3 ap New Cvs for GBM added. 
//            2012-12-27 v0.4 ap Cvs have been reorded and cleaned up, to better support PoM.
//            2013-03-12 v0.5 ap The ability is added to program the CVs on the main (PoM).
//
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            This is the cv-structure definition for the project
//            cv_data_xxxxx.h will contain the actual data.
//
//========================================================================

typedef struct
  {
    //            Name          CV   alt  Access comment
    unsigned char myAddrL;      //513   1  R/W    Accessory Address low (6 bits). Note: not the RS-Bus address
    unsigned char cv514;        //514   2  R      not used
    unsigned char T_on_F1;      //515   3  R      Hold time for relays 1 (in 20ms steps)
    unsigned char T_on_F2;      //516   4  R      Same dor relays 2
    unsigned char T_on_F3;      //517   5  R      Same dor relays 3
    unsigned char T_on_F4;      //518   6  R      Same dor relays 4
    unsigned char version;      //519   7  R      Version. Should be > 7
    unsigned char VID;          //520   8  R/W    Vendor ID (0x0D = DIY Decoder
                                                    // write value 0x0D = 13 to reset CVs to default values
    unsigned char myAddrH;      //521   9  R/W    Accessory Address high (3 bits)
    unsigned char MyRsAddr;     //522  10  R/W    RS-bus address of this feedback decoder
    unsigned char DelayIn1;     //523  11  R/W    Delay in 10ms steps before sending OFF signal (like Lenz)
    unsigned char DelayIn2;     //524  12  R/W    Same, for input 2. If 0, CV555 will be used instead
    unsigned char DelayIn3;     //525  13  R/W    Same, for input 3. If 0, CV555 will be used instead
    unsigned char DelayIn4;     //526  14  R/W    Same, for input 4. If 0, CV555 will be used instead
    unsigned char DelayIn5;     //527  15  R/W    Same, for input 5. If 0, CV555 will be used instead
    unsigned char DelayIn6;     //528  16  R/W    Same, for input 6. If 0, CV555 will be used instead
    unsigned char DelayIn7;     //529  17  R/W    Same, for input 7. If 0, CV555 will be used instead
    unsigned char DelayIn8;     //530  18  R/W    Same, for input 8. If 0, CV555 will be used instead
    unsigned char CmdStation;   //531  19  R/W    Command Station. 0 = standard / 1 = Lenz
    unsigned char RSRetry;      //532  20  R/W    Number of RS-Bus retransmissions
    unsigned char SkipEven;     //533  21  R/W    Only Decoder Addresses 1, 3, 5 .... 1023 will be used
    unsigned char cv534;        //534  22  R      not used
    unsigned char Search;       //535  23  R/W*   If set to 1: decoder LED blinks. Value will be 0 after restart
    unsigned char cv536;        //536  24  R      not used
    unsigned char Restart;      //537  25  R/W*   To restart (as opposed to reset) the decoder: use after PoM write
    unsigned char DccQuality;   //538  26  R      DCC Signal Quality
    unsigned char DecType;      //539  27  R      Decoder Type (see global.h for possible values)
    unsigned char BiDi;         //540  28  R      Bi-Directional Communication Config. Since BiDi is not used, keep at 0
    unsigned char Config;       //541  29  R      Accessory Decoder configuration (similar to CV#29)
    unsigned char VID_2;        //542  30  R      Second Vendor ID, to detect AP decoders
    unsigned char cv543;        //543  31  R      not used
    unsigned char cv544;        //544  32  R      not used

    // CVs used by all variants of the Track Occupancy Decoder
    unsigned char Min_Samples;  //545  33  R/W    Minimum number of samples a value should be stable
						    //How many consequtive ON samples are needed with the same 
						    //outcome before the result is considered to be stable. Note: 
						    //every sample takes 8 msec, so "3" gives 24 msec extra delay
    unsigned char Delay_off;    //546  34  R/W    Delay (in 100 ms steps) before previous occupancy will be relased
    unsigned char Threshold_on; //547  35  R/W    Above this value a previous OFF sample will be regarded as ON
    unsigned char Threshold_of; //548  36  R/W    Below this value a previous ON sample will be regarded as OFF
  						    // Note that the value of Threshold_of should lower than 
						    // Threshold_on
						    // Below some values for specific resistor values
						    // 82K=7,  68K=8,  56K=10, 47K=12, 39K=15, 33K=18
						    // 27K=22, 22K=28, 18K=34, 15K=41, 12K=52, 10K=68
						    // CVs used by the Reverser specific variant of the Track Occupancy Decoder

    // CVs used by the Speed Measurement specific variant of the Track Occupancy Decoder
    unsigned char Speed1_Out;   //549  37  R/W    Track number (1..8) for the first speed measurement track (0=none)
    unsigned char Speed1_LL;    //550  38  R/W    Length in cm of the first speed measurement track (low order byte)
    unsigned char Speed1_LH;    //551  39  R/W    Length in cm of the first speed measurement track (high order byte)
    unsigned char Speed2_Out;   //552  41  R/W    Track number (1..8) for the second speed measurement track (0=none)
    unsigned char Speed2_LL;    //553  41  R/W    Length in cm of the second speed measurement track (low order byte)
    unsigned char Speed2_LH;    //554  42  R/W    Length in cm of the second speed measurement track (high order byte)
    
    // CVs used by the Reverser specific variant of the Track Occupancy Decoder
    unsigned char FB_A;         //558  43  R/W    Feedback bit if track A is occupied
    unsigned char FB_B;         //559  44  R/W    Feedback bit if track B is occupied
    unsigned char FB_C;         //560  45  R/W    Feedback bit if track C is occupied
    unsigned char FB_D;         //561  46  R/W    Feedback bit if track D is occupied
    unsigned char FB_S1;        //562  47  R/W    Feedback bit if Sensor 1 is active
    unsigned char FB_S2;        //563  48  R/W    Feedback bit if Sensor 2 is active
    unsigned char FB_S3;        //564  49  R/W    Feedback bit if Sensor 3 is active
    unsigned char FB_S4;        //565  50  R/W    Feedback bit if Sensor 4 is active
    unsigned char Polarization; //566  51  R/W    If 0: J&K connected normal / if 1: J&K polarization changed

    
 } t_cv_record;
