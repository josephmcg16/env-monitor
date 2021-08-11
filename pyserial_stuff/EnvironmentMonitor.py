"""
Arduino Environment Monitor Project

Author: Joseph McGovern (2021 Saltire Scholar at Coherent Inc.)

Script for the Arduino Environment Data-logger when directly connected to a PC via USB.
=====================================================================================================
Class Environment Monitor
=====================================================================================================
Arduino with nrf52 series BLE-enabled MCU

Device acts either as a BLE central or BLE peripheral.
BLE Central acts as a client to peripheral's i2c sensor data over a custom BLE GATT Characteristic.
serviceUUID (128-bit) - 19B10000-E8F2-537E-4F6C-D104768A1214
sensorsUUID (128 bit) - 19B10001-E8F2-537E-4F6C-D104768A1214

Central detects to first discovered peripheral advertising with the sensorsUUID.

Sensor data at each timestamp is logged to a specific directory each day.
Subdirectory is made when a new device is logged.
E.g. 'Arduino/01-08-2021.txt'

BLE Central Sketch available at 'L:\ ____.ino'
BLE Peripheral Sketch available at 'L:\ ____.ino'

======================       ========================================================================
Attribute                    Description
======================       ========================================================================
device_name                  BLE advertised local name, also used in making tsv <headers>.
                             8 chars max.

comport                      Selected port of the wired logger/ BLE central

start                        Keyword to represent if logger is active.

ser                          pyserial <serial.Serial> object for a given COM port

sensors                      Available sensors for the wired device/ connected BLE peripheral

ble_connected                True if a ble central is connected to the device, False otherwise
======================       ========================================================================
Method                       Description
======================       ========================================================================
open_port                    Opens the serial port

close_port                   Closes the serial port

restart_port                 Restarts the serial port (close then open)

log_to_directory             Starts tsv data logging to user specified <path>. Makes a new directory
                             for a new device_name and a new file each day.
                             E.g. 'path/device_name/YYYY-MM-DD.txt'
                             loop runs in background thread <log_loop>

start_logger                 Sets start keyword to True and starts logging to specified path

stop_logger                  Sets start keyword to False and stops logging

ble_scan                     Scan for n_peripherals available peripherals. Returns a list of
                             peripherals found. Return [None] if device is not a BLE central.

ble_connect                  Connect to specified available BLE peripheral. Updates device_name to
                             become the same as the peripheral

ble_disconnect               Disconnects from BLE by restarting BLE central device.
"""

import serial  # https://pyserial.readthedocs.io/en/latest/
from pathlib import Path
from datetime import date, datetime
from os import mkdir
import threading
import time


class EnvironmentMonitor:

    def __init__(self, comport, baud_rate=9600, delimiter='tsv'):

        # setup pyserial port object
        self.comport = comport
        self.ser = serial.Serial(self.comport, baud_rate, timeout=10)
        self.restart_port()

        # init variables
        self.delimiter = delimiter

        self.start = False  # logger status (stopped by default)
        self.timestamp = None  # logger timestamp
        self.path = None  # logger logger files path
        self.peripherals = [None]  # list of available ble peripherals
        self.timestamp = None  # logger timestamp
        self.current_data = [None]  # logger data-list

        # get device info and check if device is an Arduino
        self.ble, self.device_name = monitor_validate(self.ser)  # device_name is None if device is a BLE central

        if self.ble:
            # device is a BLE central
            self.ble_connected = False
            self.sensors = None  # sensors not defined until device is connected
        else:
            # device is a BLE peripheral wired directly to the PC
            self.ble_connected = None
            self.sensors = get_sensors(self.ser, self.device_name)  # update sensor names

    # close the COM port
    def close_port(self):
        self.ser.close()

    # open the COM port
    def open_port(self):
        self.ser.open()

    # restart the COM port
    def restart_port(self):
        self.close_port()
        time.sleep(1)
        self.open_port()

    # connect to the wired device
    def write_device_name(self, device_name):
        self.start = False  # make sure logger has stopped

        device_name = device_name[0:8]  # dos 8.3 filename format for the SD card. 8 chars max
        self.device_name = device_name  # update new name
        write_device_name(self.ser, self.device_name)  # write to Serial

        self.ser.flushInput()  # allow device to update the name
        self.sensors = get_sensors(self.ser, self.device_name)  # update sensor names

    # log monitor sensors data to a tsv .txt
    def log_to_directory(self, path):
        self.path = path
        # check if directory exists
        directory = f"{path}/{self.device_name}"
        if not Path(directory).exists():
            mkdir(directory)  # make new directory

        # logging loop thread in the background
        def log_loop():
            ser = self.ser
            while self.start:
                # filename is the selected directory/device name/today's date
                if self.delimiter == 'csv':
                    separator = ','
                    filename = f"{directory}/{date.today()}.csv"
                else:
                    separator = '\t'
                    filename = f"{directory}/{date.today()}.txt"

                # write headers if a new file
                if not Path(filename).exists():
                    headers = separator.join(self.sensors)
                    # write headers as first line if a new file
                    file = open(filename, 'w')
                    file.write(f"timestamp{separator}")
                    file.write(f"{headers}\n")
                    file.close()

                line = ser.readline().decode().strip('\r\n')

                # checks for BLE events
                if self.ble:
                    if line.split()[0] == "Found":
                        # connection failed, stop logging
                        self.ble_connected = False
                        print("Could not connect")
                        self.stop_logger()
                        break
                    elif line == "Peripheral disconnected." or line == "Rescanning for UUID":
                        # disconnected, stop logging
                        self.ble_connected = False
                        print(f"Disconnected from {self.device_name}")
                        self.stop_logger()
                        break

                # make sure line read is the sensor data and not headers
                if self.device_name in line.split('\t')[0]:
                    line = ser.readline().decode().strip('\r\n')

                ser.readline()  # flush out extra line (sensor names)

                # sensors data
                self.timestamp = str(datetime.now())  # current timestamp
                sensors_data_list = line.split('\t')
                self.current_data = [float(sensor_data) for sensor_data in sensors_data_list]
                if self.delimiter == 'csv':
                    file_data = ','.join(sensors_data_list)
                else:
                    file_data = line
                # failsafe for an error when reading the data
                if 0.00 in self.current_data:
                    self.ble_disconnect()
                    self.stop_logger()
                    raise BLEDataError("0.00 found in ser.readline. Restart Device.")

                # write to the file
                file = open(filename, 'a+')
                file.write(f'{self.timestamp}{separator}')
                file.write(f"{file_data}\n")
                file.close()

                # print data to terminal
                print(f'\n{self.timestamp}')
                for i in range(len(self.sensors)):
                    print(f'{self.sensors[i]}: {sensors_data_list[i]}')

        # run loop as a thread in the background
        log_loop_thread = threading.Thread(name='log_loop', target=log_loop)
        log_loop_thread.start()

    # start logger
    def stop_logger(self):
        self.current_data = [None]  # logger stopped, no data available
        self.start = False
        print('logger stopped')

    # stop logger
    def start_logger(self):
        self.start = True
        print("\nlogger started\n")
        self.log_to_directory(self.path)

    # scan for ble peripherals advertising with monitor service
    def ble_scan(self, n_peripherals=5):
        if not self.ble:
            # device is not a BLE central, can't scan
            return None
        else:
            self.peripherals = ble_scan(self.ser, n_peripherals)
            return self.peripherals

    # connect to specific peripheral (use threading if using method for a GUI)
    def ble_connect(self, peripheral):
        self.device_name = peripheral
        print(f"Connecting to {peripheral} ...")
        ser = self.ser
        self.write_device_name(self.device_name)
        ser.readline().decode().strip('\r\n')
        ser.timeout = 20  # increase timeout to allow for device to connect

        # flush out the connected prints
        ser.readline()  # connected
        ser.readline()  # discovering attributes
        ser.readline()  # attributes discovered
        ser.readline()   # subscribed
        self.sensors = get_sensors(ser, peripheral)
        print("Connected.")
        self.ble_connected = True

    # restart the Serial port and device starts scanning for peripherals again
    def ble_disconnect(self):
        self.stop_logger()
        self.restart_port()
        self.ble_connected = False


"""
Module-Level Functions for class EnvironmentMonitor
======================       ========================================================================
Function                     Description
======================       ========================================================================
monitor_validate             Discovers which type of device the port is. Throws exception if Arduino
                             is not found. Returns the device name and if it is a ble device or not
                             
write_device_name            Write 8 byte ASCII to the connected Arduino representing device_name

get_sensors                  Get the names of the sensors corresponding to the logger connected via
                             serial or the sensors of the BLE peripheral connected to the BLE Central
                             by BLE that is connected to the script via serial.
                             
ble_scan                     Scan available peripherals and return a list of available BLE loggers.
                             Scans for n_peripherals. Default value is 5.
"""


# custom exception
class ArduinoNotFoundError(ValueError):
    pass


class BLEDataError(ValueError):
    pass


# get details on the device, check if device is an Arduino monitor
def monitor_validate(ser):
    # how long to wait on readline before throwing an error. if error keeps popping up, increase this value
    ser.timeout = 2
    line = ser.readline().decode().strip('\r\n')
    if line == 'BLE Central':
        # device is a central, set ble to True
        is_ble = True
        device_name = None  # need to connect ble first to get peripheral device name
    elif line == '':
        # ser.readline timeout, throw an error
        raise ArduinoNotFoundError(f"Arduino Environment Monitor not found on {ser.port} (serial read timeout).")
    else:
        # must be a wired Arduino
        is_ble = False
        device_name = line
    return is_ble, device_name


# write new device_name to connected Arduino
def write_device_name(ser, device_name):
    ser.write(device_name.encode())


# get the available sensors
def get_sensors(ser, device_name):
    ser.timeout = 3
    ser.readline()
    line = ser.readline().decode().strip('\r\n')
    # make sure headers are read
    if device_name not in line.split('\t')[0]:
        ser.readline()  # skip device_name
        line = ser.readline().decode().strip('\r\n')  # sensor names

    if len(line.split('\t')) == 1:
        line = ser.readline().decode().strip('\r\n')  # sensor names
    sensors = line.split('\t')
    # get list of sensors from line
    for i, sensor in enumerate(sensors):
        sensors[i] = sensor.replace('\t', '')
    return sensors


# scan for n peripherals
def ble_scan(ser, n):
    ser.readline()
    ser.timeout = 10  # change timeout to allow device to snap for peripheral
    peripheral_list = []
    for i in range(n):
        line = ser.readline().decode().strip('\r\n')
        peripheral_discovered = line.split()[-1]
        if peripheral_discovered not in peripheral_list:
            peripheral_list.append(peripheral_discovered)
    return peripheral_list
  
