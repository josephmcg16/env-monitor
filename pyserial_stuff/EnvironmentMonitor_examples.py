"""
EnvironmentMonitor Example Codes

Author: Joseph McGovern (2021 Saltire Scholar at Coherent Inc.)

wired BLE central:      wireless_example
wired BLE peripheral:   wired_example
"""

from EnvironmentMonitor import EnvironmentMonitor
import serial.tools.list_ports
import time


# Sample code for a data-logger (peripheral) connected to a BLE central which is connected via USB port 'COM10'
# Starts logging for 5 seconds. Stops for 3. Then starts logging indefinitely
def wireless_example(directory):
    central = EnvironmentMonitor('COM10')
    print("Scanning")
    peripherals = central.ble_scan(1)
    print(f'Available peripheral(s)\n{peripherals}')
    central.ble_connect(peripherals[0])  # connect to first discovered peripheral

    # print device details
    print(f'Wireless: {central.ble}')
    print(f'Name: {central.device_name}')
    print(f'Sensors: {central.sensors}')

    # start logging to the directory
    central.log_to_directory(directory)
    central.start_logger()

    # log for 10 seconds
    time.sleep(10)

    print("Disconnected")
    central.ble_disconnect()

    print("Scanning")
    peripherals = central.ble_scan(1)
    print(f'Available peripheral(s)\n{peripherals}')
    central.ble_connect(peripherals[0])  # connect to first discovered peripheral

    # print device details
    print(f'Wireless: {central.ble}')
    print(f'Name: {central.device_name}')
    print(f'Sensors: {central.sensors}')

    # start logging to the directory
    central.log_to_directory(directory)
    central.start_logger()

    # log for an hour
    time.sleep(3600)


# Sample code for a data-logger (peripheral) connected directly to the PC via USB port 'COM4'
# Starts logging for 10 seconds with name "Arduino"
# Device is renamed to "lab_test" and then logs for an hour.
def wired_example(directory):

    # first data-logger "Arduino"
    peripheral = EnvironmentMonitor('COM4')  # make the EnvironmentMonitor object at the correct port
    peripheral.write_device_name("Arduino")  # update the device name

    # print device details
    print(f'Wireless: {peripheral.ble}')
    print(f'Name: {peripheral.device_name}')
    print(f'Sensors: {peripheral.sensors}')

    print("\nnext device\n")

    # start logging to the directory
    peripheral.log_to_directory(directory)
    peripheral.start_logger()

    # log for 10 seconds
    time.sleep(10)

    # second data-logger "lab_test"
    peripheral.write_device_name("lab_test")  # update the device name
    # (method sets self.start to False, i.e. stops the logger)

    # print details
    print(f'Wireless: {peripheral.ble}')
    print(f'Name: {peripheral.device_name}')
    print(f'Sensors: {peripheral.sensors}')

    # start logging to the directory
    peripheral.log_to_directory(directory)
    peripheral.start_logger()
    # log for an hour
    time.sleep(3600)


def main():
    # path for the log directory location
    directory = 'C:/Users/McGoverJ/OneDrive - Coherent, Inc/' \
                'Environmental Monitoring Project/Code/PythonStuff/bleApp/data'

    # print the available COM ports
    ports = serial.tools.list_ports.comports()
    print('\n'.join([str(port) for port in ports]))

    wired_example(directory)
    # wireless_example(directory)


if __name__ == '__main__':
    main()
