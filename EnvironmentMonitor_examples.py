"""
EnvironmentMonitor Example Codes

Author: Joseph McGovern (2021 Saltire Scholar at Coherent Inc.)

wired BLE central:      wireless_example()
wired BLE peripheral:   wired_example()
"""

# import the environment monitor class
from EnvironmentMonitor import EnvironmentMonitor
import time


# Sample code for a data-logger (peripheral) connected to a BLE central which is connected via USB port 'COM10'
# Starts logging for 5 seconds. Stops for 3. Then starts logging indefinitely
def wireless_example():
    port = 'COM10'
    n = 1  # maximum number of peripherals with monitor service uuid to scan for
    monitor = EnvironmentMonitor(port)
    peripherals = monitor.ble_scan()
    print(f'Available peripheral(s)\n{peripherals}')
    monitor.ble_connect(peripherals[0])  # connect to first discovered peripheral
    sensor_names = monitor.sensors
    print(f'Available Sensors\n{sensor_names}')
    path = 'C:/Users/McGoverJ/OneDrive - Coherent, Inc/Environmental Monitoring Project' \
           '/Code/PythonStuff/bleApp/data'
    monitor.log_to_directory(path)
    monitor.start_logger()
    time.sleep(5)
    monitor.stop_logger()
    time.sleep(3)
    monitor.start_logger()


# Sample code for a data-logger (peripheral) connected directly to the PC via USB port 'COM4'
# Starts logging for 5 seconds. Stops for 3. Then starts logging indefinitely
def wired_example():
    name = 'Arduino'
    port = 'COM4'
    monitor = EnvironmentMonitor(port, name)
    monitor.wired_connect(monitor.device_name)
    sensor_names = monitor.sensors
    print(f'Available Sensors\n{sensor_names}')
    path = 'C:/Users/McGoverJ/OneDrive - Coherent, Inc/Environmental Monitoring Project' \
           '/Code/PythonStuff/bleApp/data'
    monitor.log_to_directory(path)
    monitor.start_logger()
    time.sleep(5)
    monitor.stop_logger()
    time.sleep(3)
    monitor.start_logger()


def main():
    wired_example()
    # wireless_example()


if __name__ == '__main__':
    main()
