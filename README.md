# GBM (Gleis Besetzt Melder) - Track Occupancy Decoder
Decoder for the DCC (Digital Command Control) system to detect if tracks are occupied. Detection is performed by measuring voltage over a resistor / (high speed) diode. Feedback to the master station uses the RS-Bus. The software can additionally be used to control 3 or 4 relays, to act as a reverser module or to switch track power off (useful for shadow stations). In addition, it can drive a (2x16 character) LCD display to act as speed monitor.

The code has been tested on the following boards: 
 - [https://easyeda.com/aikopras/gbm-eagle](https://easyeda.com/aikopras/gbm-eagle)
 - [https://easyeda.com/aikopras/vitrine-decoder](https://easyeda.com/aikopras/vitrine-decoder)
A description of this decoder and related decoders can be found on [https://sites.google.com/site/dcctrains](https://sites.google.com/site/dcctrains).

The software is written in C and runs on ATMEGA16A and similar processors (32A, 644A). It is an extension of the [Opendecoder](https://www.opendcc.de/index_e.html) project, and written in "pre-Arduino times". 
It can be compiled, linked and uploaded using the [<b>Makefile</b> file](/src/Makefile) in the src directory, or via the Arduino IDE. Instructions for using the Arduino IDE can be found in the [<b>Arduino-GBM.ino</b> file](/src/Arduino-GBM.ino). Note that you have to rename the /src directory into "Arduino-GBM" before you open the .ino file.


## First use
After the fuse bits are set and the program is flashed, the decoder is ready to use.
Note that the Arduino IDE is unable to flash the decoder's configuration variables to EEPROM. Thefore the main program performs a check after startup and, if necessary, initialises the EEPROM. For this check the value of two CVs is being tested: VID and VID_2 (so don't change these CVs). After the EEPROM has been initialised, the LED is blinking to indicate that it expects an accessory (=switch) command to set the RS-Bus address. Push the PROGRAM button, and the next accessory (=switch) address received will be used as RS-Bus address (1..127)


## Configuration Variables (CVs)
The operation of the software can be controlled via Configuration Variables (CVs).

The default values of the Configuration Variables (CVs) can be modified in the file [cv_data_gbm.h](/src/cv_data_gbm.h)
In particular the decoder type (CV27: DecType) can be changed, for example to make the decoder operate as a reverser.

Configuration Variables can also be changed using Programming on the Main (PoM). 
Unfortunately many Command Stations, including the LENZ LZV100, do not support PoM for accessory decoders. Therefore this software implements PoM for LOCO decoders. The feedback decoder therefore listens to a LOCO  address that is equal to the RS-Bus address + 6000. Transmission of PoM SET commands conforms to the NMRA standards.
PoM VERIFY commands do use railcom feedback messages and therefore do NOT conform to the NMRA standards. Instead, the CV Value is send back via the RS-Bus using address 128 (a proprietary solution).
 
For MAC users an easy to use OSX program to read and modify CVs can be downloaded from: [https://github.com/aikopras/Programmer-GBM-POM](https://github.com/aikopras/Programmer-GBM-POM).
