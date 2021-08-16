# env-monitor

Make sure to clone the folders, not just the files. 

The Logo.cpp and Logo.h files are needed for the logo bitmap print to work in the monitor_ble_peripheral.ino sketch.

The python codes are also used for easy configuration of the devices. See the example script for a description on how to use the module.

The GUI is still a work in progress and the EnvironmentMonitor ble_scan and ble_connect methods are stil a bit buggy. Sometimes the pyserial Serial.write() does not properly write commands to the BLE central Arduino.

Full description of the code in the python files and sketches. I'm planning to add a jupyter notebook with some examples also.

Links to all the libraries found in the monitor_peripheral.ino sketch.

Any questions please email me! :)
joseph.mcgovern16@gmail.com
--------------------------------------------------------------------------------------------------
monitor_ble_central is the Arduino sketch for an Arduino connected to an SH1006 driver
128x64 OLED connected directly to a PC (basically the client to the bluetooth peripheral
that has the sensors.

monitor_ble_peripheral is the Arduino connected to the sensors and an OLED display.
Also attached to an Adalogger Featherwing with a microSD slot and RTC.

The Arduinos communicate to a PC using the python scripts or can back up to an onboard SD card.

--------------------------------------------------------------------------------------------------
Python Stuff
--------------------------------------------------------------------------------------------------
the scripts may reference paths that do not exist and throw an error. 
change the paths to wherever the files references are saved! :)

GUI is a work in progress. Uses tkinter, threading and the EnvironmentMonitor class.

csv_to_tsv just converts a csv sheet into a tsv (tab seperated) sheet

EnvironmentMonitor module contains a py class which talks to the Arduino using the pyserial library.
Please see the examples code for more info!


Any changes I make will be available on Github.

All rights reserved by Coherent Inc.
