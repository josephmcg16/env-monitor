# env-monitor

Make sure to clone the folders, not just the files. 

The Logo.cpp and Logo.h files are needed for the logo bitmap print to work in the monitor_ble_peripheral.ino sketch.

The python codes are also used for easy configuration of the devices. See the example script for a description on how to use the module.

The GUI is still a work in progress and the EnvironmentMonitor ble_scan and ble_connect methods are stil a bit buggy. Sometimes the pyserial Serial.write() does not properly write commands to the BLE central Arduino.

Full description of the code in the python files and sketches. I'm planning to add a jupyter notebook with some examples also. 
