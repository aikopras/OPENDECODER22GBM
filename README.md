# GBM (Gleis Besetzt Melder) - Track Occupancy Decoder
Decoder for the DCC (Digital Command Control) system to detect if tracks are occupied. Detection is performed by measuring voltage over a resistor / (high speed) diode. Feedback to the master station uses the RS-Bus. The software can additionally be used to control 3 or 4 relays, to act as a reverser module or to switch track power off (useful for shadow stations). In addition, it can drive a (2x16 character) LCD display to act as speed monitor.

The operation of the software can be controlled via Configuration Variables (CVs); details can be found in [cv_data_gbm.h](cv_data_gbm.h).

The software is written in C and runs on ATMEGA16A and similar processors (32A, 644A). It is an extension of the [Opendecoder](https://www.opendcc.de/index_e.html) project, and written in "pre-Arduino times". However, it can be compiled and flashed via the Arduino IDE, after installing [MightyCore](https://github.com/MCUdude/MightyCore) to add the ATMEGA16 as new core.

The hardware and schematics can be downloaded from [my EasyEda homepage](https://easyeda.com/aikopras/gbm-eagle).
A description of this decoder and related decoders can be found on [https://sites.google.com/site/dcctrains](https://sites.google.com/site/dcctrains).
