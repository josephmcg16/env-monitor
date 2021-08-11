"""
Arduino Environment Monitor GUI

Author: Joseph McGovern (2021 Saltire Scholar at Coherent Inc.)

This script can be ran as a standalone application to interface with an Arduino BLE Central connected to PC COM ports
and receive sensor data from a BLE peripheral Arduino.
Data is then displayed as a plot and written to a .csv spreadsheet at a user-defined PATH.

class MonitorProgram is used to store all attributes related to the GUI program.
Designing the script this way prevents
the necessity to sue global variables.

<Class::MonitorProgram>
======================       ========================================================================
Method                       Description
======================       ========================================================================
create_root_widgets          Starter window of the GUI. Tk.OptionMenu widget to select a COM port
                             and buttons to refresh available COM port and confirm current
                             selection.

refresh_ports                Command to refresh the drop menu to include current available COM ports

create_window_widgets        Creates main app window of the GUI.

close_window                 Closes main window and re-opens port selection (root window)

start_logger                 Starts writing .csv to chosen path given certain conditions are true

update_peripherals           Checks monitor object for attribute peripherals and updates the
                             peripherals drop menu selection when available

connect_to_peripheral        Starts connecting to the selected peripheral in the background once a
                             button is pressed
==========================   =======================================================================

AUTHOR: Joseph McGovern - 2021 Saltire Scholar Intern at Coherent Inc.
"""

# GUI Library
from tkinter import messagebox, filedialog, StringVar
import tkinter as tk

# PySerial Library
import serial.tools.list_ports  # method which lists available ports on the screen

# EnvironmentMonitor class
from EnvironmentMonitor import EnvironmentMonitor

# tkinter threading
import threading


class MonitorGUI:

    def __init__(self, title, initial_dir):
        # init root
        self.root = tk.Tk()
        self.root.title(title)
        self.root.geometry(f"345x60+{30}+{30}")
        self.root.resizable(width=0, height=0)

        # init tkinter widgets
        self.window = None
        self.options_frame = None
        self.logger_frame = None

        # init ports
        self.ports_available = serial.tools.list_ports.comports()  # list of available ports
        self.port = tk.StringVar()  # selected port
        self.port.set('Select COM Port')

        # init status
        self.status_label = status_bar(self.root, "Ready")

        # init monitor object
        self.monitor = None

        # init monitor variables
        self.peripheral = tk.StringVar()  # selected peripheral
        self.peripherals = None  # list of available ble peripherals
        self.log_directory = None  # directory for the log files
        self.initial_dir = initial_dir  # initial directory for the file dialog

        # make the root
        self.create_root_widgets()

    def create_root_widgets(self):
        # drop down menu of available Arduino ports
        drop = tk.OptionMenu(self.root, self.port, *self.ports_available)
        drop.grid(row=0, column=0)
        drop.config(width=40)
        self.port.trace('r', self.refresh_ports)  # refresh ports each time drop menu is clicked

        # button to select the chosen option
        select_port_btn = tk.Button(self.root, text="Confirm", command=self.select_monitor)
        select_port_btn.grid(row=0, column=1)

    def refresh_ports(self, *args):
        self.ports_available = serial.tools.list_ports.comports()
        drop = tk.OptionMenu(self.root, self.port, *self.ports_available)
        drop.grid(row=0, column=0)
        drop.config(width=40)

    def select_monitor(self):
        if self.port.get() == "Select COM Port":
            # port has not been selected yet
            tk.messagebox.showwarning("Notice", "Please Select a COM Port")
            return

        comport = self.port.get().split()[0]

        try:
            self.monitor = EnvironmentMonitor(comport)
        except ValueError:
            # invalid port selection, throw an error
            messagebox.showerror("Port Selection Error", f"Arduino Not Found at {comport} !")
            return

        monitor = self.monitor

        if monitor.ble:
            if not messagebox.askokcancel("Port Selection", f"BLE Central found at {comport}. Select Log Directory."):
                return
            # device is a central, start scanning for BLE peripherals in the background
            ble_scan_thread = threading.Thread(name='scan_loop', target=monitor.ble_scan)
            ble_scan_thread.start()
        else:
            if not messagebox.askokcancel("Port Selection", f"Found Wired Arduino at {comport}. Select Log Directory."):
                return

        # then open window
        self.create_window_widgets()

    def create_window_widgets(self):
        # temporarily close the root window
        self.root.withdraw()

        # select the directory
        self.log_directory = filedialog.askdirectory(initialdir=self.initial_dir)
        if self.log_directory == '':
            self.root.deiconify()
            return

        # open new window
        self.window = tk.Toplevel()
        self.window.geometry(f"1280x720")
        self.window.title(f"Python Logger - {self.port.get()}")
        self.window.state('zoomed')  # opens full screen
        self.window.protocol("WM_DELETE_WINDOW", self.close_window)  # close the app if window is closed

        # update status bar
        self.status_label.destroy()
        self.status_label = status_bar(self.window, "Ready")

        # make the widgets
        tk.Label(self.window, text=f"Selected Path: '{self.log_directory}'")\
            .grid(row=0, column=0, columnspan=4, sticky='W')
        self.options_frame = tk.LabelFrame(self.window)
        self.options_frame.grid(row=1, column=0, sticky='W')
        tk.Label(self.options_frame, text="Logger Options:").grid(row=0, column=0, columnspan=2, sticky='W')
        start_btn = tk.Button(self.options_frame, text="Start Logger", command=self.start_logger)
        start_btn.grid(row=1, column=0)
        stop_btn = tk.Button(self.options_frame, text="Stop Logger", command=self.monitor.stop_logger)
        stop_btn.grid(row=1, column=1)

        # widgets for a BLE central
        if self.monitor.ble:
            lbl = tk.Label(self.options_frame, text="BLE Peripheral Selection")
            lbl.grid(row=0, column=2, sticky='W')
            # BLE peripheral selection drop menu
            self.peripheral.set("Select Peripheral")
            peripherals = self.update_peripherals()
            drop = tk.OptionMenu(self.options_frame, self.peripheral, *peripherals)
            drop.grid(row=1, column=2)
            drop.config(width=40)
            self.peripheral.trace('r', self.update_peripherals)  # refresh peripherals each time drop menu is clicked
            # BLE Connect to the Peripheral
            connect_btn = tk.Button(self.options_frame, text="Connect", command=self.connect_to_peripheral)
            connect_btn.grid(row=1, column=3)

        # widgets for a wired device
        else:
            tk.Label(self.options_frame, text="Peripheral Device Name").grid(row=0, column=2, sticky='W')
            device_name_input = tk.Entry(self.options_frame)
            device_name_input.grid(row=1, column=2)
            tk.Button(self.options_frame, text="Rename",
                      command=lambda: self.monitor.wired_connect(device_name_input.get()))\
                .grid(row=1, column=3)
        self.logger_frame = tk.LabelFrame(self.window)
        self.logger_frame.grid(row=1, column=3)

    def close_window(self):
        if self.monitor.ble:
            self.monitor.ble_disconnect()
        self.monitor.stop_logger()
        self.monitor.close_port()
        self.window.withdraw()
        self.root.deiconify()

    def start_logger(self):
        if self.monitor.ble:
            if self.peripheral.get() in ["No Peripherals Found, Scanning...", "Select Peripheral"]:
                messagebox.showwarning("Logger Warning", "Please Select a Peripheral")
                return
            if not self.monitor.ble_connected:
                messagebox.showwarning("Logger Warning", "BLE peripheral is not connected")
                return

        self.monitor.log_to_directory(self.log_directory)
        self.monitor.start_logger()

    def update_peripherals(self, *args):
        peripherals = self.monitor.peripherals
        if peripherals == [None]:
            # ble monitor central has no peripherals
            self.peripheral.set("No Peripherals Found, Scanning...")
            peripherals = ["No Peripherals Found, Scanning..."]
            return peripherals

        if self.peripheral.get() == "No Peripherals Found, Scanning...":
            # peripherals found, change default drop menu selection
            self.peripheral.set("Select Peripheral")

        # update the drop menu
        drop = tk.OptionMenu(self.options_frame, self.peripheral, *peripherals)
        drop.grid(row=1, column=2)
        drop.config(width=40)
        return peripherals

    def connect_to_peripheral(self):
        if self.peripheral.get() in ["No Peripherals Found, Scanning...", "Select Peripheral"]:
            # can't connect, no peripheral selected
            messagebox.showwarning("BLE Warning", "Please Select a Peripheral")
            return

        # start connecting to ble in the background
        self.status_label.destroy()
        self.status_label = status_bar(self.window, f"Connecting to {self.peripheral.get()}")
        ble_connect_thread = threading.Thread(name='connecting_loop',
                                              target=lambda: self.monitor.ble_connect(self.peripheral.get()))
        ble_connect_thread.start()

        def check_connected():
            while True:
                if self.monitor.ble_connected:
                    self.status_label.destroy()
                    self.status_label = status_bar(self.window, f"BLE Connected to {self.peripheral.get()}")
                    break
        check_connected_thread = threading.Thread(name='check_connection_loop', target=check_connected)
        check_connected_thread.start()


"""
Module Functions
======================       ========================================================================
Function                     Description
======================       ========================================================================
status_bar                   Generates a status label
"""


def status_bar(window, status):
    status_label = tk.Label(window, text=f'Status: {status}')  # init status again
    status_label.place(relx=0, rely=1.0, anchor='sw')
    return status_label


def main():
    # Create the GUI program
    program = MonitorGUI(title="Python Logger", initial_dir='C:/Users/McGoverJ/OneDrive - Coherent, Inc/'
                                                            'Environmental Monitoring Project/Code/PythonStuff/bleApp/'
                                                            'data')
    # Start the GUI event loop
    program.root.mainloop()


if __name__ == '__main__':
    main()
