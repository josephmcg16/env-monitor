"""
Arduino Environment Monitor Project

Author: Joseph McGovern (2021 Saltire Scholar at Coherent Inc.)

Script for the Arduino Environment Data-logger when directly connected to a PC via USB.
=====================================================================================================
Class Environment Monitor
=====================================================================================================
Arduino with nrf52 BLE MCU

Device acts either as a BLE central or BLE peripheral.
BLE Central acts as a client to peripheral's i2c sensor data over a custom BLE GATT Characteristic.
serviceUUID (128-bit) - 19B10000-E8F2-537E-4F6C-D104768A1214
sensorsUUID (128 bit) - 19B10001-E8F2-537E-4F6C-D104768A1214

Central detects to first discovered peripheral advertising with the sensorsUUID.

Sensor data at each timestamp is logged to a specific directory each day.
Subdirectory is made when a new device is logged.
E.g. 'Arduino/01-08-2021.csv'

BLE Central Sketch available at 'L:\ ____.ino'
BLE Peripheral Sketch available at 'L:\ ____.ino'

======================       ========================================================================
Attribute                    Description
======================       ========================================================================
device_name                  BLE advertised local name, also used in making .csv <headers>.
                             8 chars max.

comport                      Selected port of the wired logger/ BLE central

start                        Keyword to represent if logger is active.

ser                          pyserial <serial.Serial> object for a given COM port

sensors                      Available sensors for the wired device/ connected BLE peripheral

ble_connected                True if a ble central is connected to the device, False otherwise
======================       ========================================================================
Method                       Description
======================       ========================================================================
restart                      Closes and re-opens the serial COM port

log_to_directory             Starts .csv data logging to user specified <path>. Makes a new directory
                             for a new device_name and a new file each day.
                             E.g. 'path/device_name/YYYY-MM-DD.csv'
                             loop runs in background thread <log_loop>

start_logger                 Sets start keyword to True and starts logging to specified path

stop_logger                  Sets start keyword to False and stops logging

ble_connect                  Connect to specified available BLE peripheral. Updates device_name to
                             become the same as the peripheral
"""
# import libraries
import serial
from pathlib import Path
from datetime import date, datetime
from os import mkdir
import threading


class EnvironmentMonitor:

    def __init__(self, comport, device_name='Arduino', baud_rate=9600):
        # check device name suitable for dos 8.3 filename format for the SD card
        if len(device_name) > 8:
            device_name = device_name[:8]
            print("attribute <device_name> must be 8 characters or less.")
        self.device_name = device_name
        self.comport = comport

        self.sensors = None  # sensors not defined until device is connected

        # logger start status
        self.start = False

        # init variables
        self.path = None
        self.peripherals = [None]  # empty list
        self.current_data = None

        # restart com port
        self.ser = serial.Serial(comport, baud_rate, timeout=3)
        self.ser.close()
        self.ser.open()

        # discover if device is a ble central or not
        self.ble = monitor_validate(self.ser)

        if self.ble:
            # ble connected status
            self.ble_connected = False
        else:
            self.ble_connected = None

    # close then re-open the COM port
    def restart(self):
        self.ser.close()
        self.ser.open()

    # connect to the wired device
    def wired_connect(self, wired_device_name):
        self.device_name = wired_device_name
        write_device_name(self.ser, wired_device_name)
        print(self.device_name)
        self.sensors = get_sensors(self.ser, self.device_name)
        print(self.sensors)

    # log monitor sensors data to a .csv
    def log_to_directory(self, path):
        self.path = path
        # check if directory exists
        directory = f"{path}/{self.device_name}"
        if not Path(directory).exists():
            mkdir(directory)  # make new directory

        # first line of new file is the headers
        headers = ",".join(self.sensors)

        # logging loop thread
        def log_loop():
            ser = self.ser
            while self.start:
                # filename is the selected directory/device name/today's date
                filename = f"{directory}/{date.today()}.csv"
                # Check if the logger file exists yet
                if not Path(filename).exists():
                    # write headers if a new file
                    file = open(filename, 'w')
                    file.write("timestamp,")
                    file.write(f"{headers}\n")
                    file.close()

                file = open(filename, 'a+')
                line = ser.readline().decode().strip('\r\n')
                # make sure line read is the sensor data and not headers
                if self.device_name in line.split(',')[0]:
                    line = ser.readline().decode().strip('\r\n')
                elif line == "Peripheral disconnected.":
                    # stop logging
                    self.ble_connected = False
                    print(f'\n{line}')
                    self.stop_logger()
                    break

                # write PC timestamp to file
                timestamp = str(datetime.now())
                file.write(timestamp)  # timestamp

                # write sensors data to file
                sensor_data = line
                self.current_data = sensor_data
                file.write(f",{sensor_data}\n")
                file.close()
                # print data to terminal
                sensors_data_list = line.split(',')
                print(f'\n{timestamp}')
                for i in range(len(self.sensors)):
                    print(f'{self.sensors[i]}: {sensors_data_list[i]}')

        # run loop as a thread in the background
        log_loop_thread = threading.Thread(name='log_loop', target=log_loop)
        log_loop_thread.start()

    # start/ stop logger
    def stop_logger(self):
        self.start = False
        print('logger stopped')

    def start_logger(self):
        self.start = True
        print("\nlogger started\n")
        self.log_to_directory(self.path)

    # scan for ble peripherals advertising with monitor service
    def ble_scan(self, n_peripherals=5):
        if not self.ble:
            return None
        else:
            self.peripherals = ble_scan(self.ser, n_peripherals)
            return self.peripherals

    # connect to specific peripheral
    def ble_connect(self, peripheral):
        self.device_name = peripheral
        print(f"Connecting to {peripheral} ...")
        ser = self.ser
        write_device_name(ser, peripheral)
        line = ser.readline().decode().strip('\r\n')
        while line != "Connecting ...":
            # empty buffer till device starts connecting
            line = ser.readline().decode().strip('\r\n')
        ser.timeout = 20  # increase timeout to allow for device to connect
        ser.readline()  # connected
        ser.readline()  # discovering attributes
        ser.readline()  # attributes discovered
        self.sensors = get_sensors(ser, peripheral)
        print("Connected.")
        self.ble_connected = True


"""
Module-Level Functions for class EnvironmentMonitor __init__
======================       ========================================================================
Function                     Description
======================       ========================================================================
monitor_validate             Discovers which type of device the port is. Throws exception if Arduino
                             is not found
                             
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


def monitor_validate(ser):
    # how long to wait on readline before throwing an error. if error keeps popping up, increase this value
    ser.timeout = 0.5
    serial_readline = ser.readline().decode().strip('\r\n')
    if serial_readline == 'BLE Central':
        # device is a central, set ble to True
        is_ble = True
    elif serial_readline == '':
        # ser.readline timeout, throw an error
        raise ArduinoNotFoundError(f"Arduino Environment Monitor not found on {ser.port} (serial read timeout).")
    else:
        # must be a wired Arduino
        is_ble = False
    return is_ble


# write new device_name to connected Arduino
def write_device_name(ser, device_name):
    ser.write(device_name.encode())


# get the available sensors
def get_sensors(ser, device_name):
    ser.timeout = 3
    line = ser.readline().decode().strip('\r\n')
    # make sure headers are read
    if device_name not in line.split(',')[0]:
        line = ser.readline().decode().strip('\r\n')
        print(line)
    sensors = line.split(',')
    # get list of sensors from line
    for i, sensor in enumerate(sensors):
        sensors[i] = sensor.replace(',', '')
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
