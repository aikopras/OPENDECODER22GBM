OpenDecoder2 / GBM decoder
-----------------------------------------------------------------------------------------
Decoder for the DCC (Digital Command Control) system to detect if tracks are occupied.
Eight tracks can be monitored.
Detection is performed by measuring voltage over a resistor / (high speed) diode.
Feedback to the master station uses the RS-Bus.
The software can additionally be used:
-  to control 3 or 4 relays,
- to act as a reverser module or
- to switch track power off (useful for shadow stations).
- drive a (2x16 character) LCD display to act as speed monitor.

The operation of the software can be controlled via Configuration Variables (CVs);
details can be found in [cv_data_gbm.h](cv_data_gbm.h).

The software is written in C and runs on ATMEGA16A and similar processors (32A, 644A).
It is an extension of the [Opendecoder](https://www.opendcc.de/) project; 
the copyright for the initial code is with Wolfgang Kufer. For details, see www.opendcc.de 

------------------------------------------------------------------------------------------
Initialising the GBM decoder

If a GBM decoder has not yet been initialised, it will listen to loco address 6000 
and allow Programming on the Main (PoM)

To program the decoder, push the Program button and enter on a handheld (or computer) 
the desired RS-bus address as switch address! This address should be in the range 
between 1 and 128, and will be stored in CV10.
From now on, the GBM decoder will listen to loco address 6000 + RS-bus address.
PoM is possible using this address.

------------------------------------------------------------------------------------------
Switching relays

The GBM decoder can additionally switch 4 relays.
The relays can be set using the F1—F4 functions for loco address 6000 + RS-bus address

The relays can also be set using traditional accessory (switch) addresses.
The accessory address of the decoder is stored in CV1 and CV9. 
The initial value of CV9 is “0x80”, representing an invalid address. 
On the web there is some confusion regarding the exact relationship between the
decoder address within the DCC decoder hardware and CV1 and CV9. 
The convention used by my decoders is: My_Dec_Addr = CV1 - 1 + (CV9*64). 
Note that this implies that the minimum value for CV1 should be 1 (and not 0), 
and that CV1=64 is allowed!.
Thus we have the following:
- The valid range of CV1 is 1..64
- The valid range of CV9 is 0..3  (or 128, if the decoder has not been initialised)

The easiest way to program the accessory address for the relays is to use the "Relays"
tab of the "Programmer GBM" software (this software has been developed for MAC-OS).
See: https://github.com/aikopras/Programmer-GBM-POM