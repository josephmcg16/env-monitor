/*
 * --------------------------------------------------------------------------
  Environment Monitor BLE Peripheral

  Author: Joseph McGovern, 2021 Saltire Scholar Intern
  ------------------------------------------------------------------------------
  THis is the sketch BLE Peripheral which recieves data from sensors over 
  I2C communication bus

  Prints to an OLED Display and backups to SD card.
  SD card can be removed during runtime; but to avoid issues,
  shutdown the device before removing the SD card.

  SD card creates new file with headers for each sensor each day.
  Can change the delimiter from a comma (,) i.e. .csv 
  to a tab (/t) i.e. tsv (.txt).
  SD lib is limited to dos 8.3 filename format, 
  i.e. filename can only be 8 chars

  ------------------------------------------------------------------------------
  HYT 939 I2C Humidity/ Temperature Sensor
  ------------------------------------------------------------------------------
  Recieves data over I2C protocol.
  Slave address and command registers defined in the application note:
  https://www.servoflo.com/images/PDF/hyt-manual.pdf
  
  This datasheet is a bit fried to understand, but (using the Wire library) you 
  just write a command to the I2C slave address (0x28) saying you want data 
  (write a value of 0x80) then request 4 bytes of data the use some bit 
  manipulation and maths to get decimal numbers (floats). 
  Neat example on pages 13-14 of the application note.
  ------------------------------------------------------------------------------
  Barometer 3 Click (Infineon DPS368 I2C Barometric Pressure Sensor)
  ------------------------------------------------------------------------------
  Much easier to setup than the HYT939. Infineon offers a coding library for
  Arduino boards:
  https://github.com/Infineon/DPS368-Library-Arduino

  Just make an instance of the library class and call certain methods to get
  data (example codes available with the library).

  I had to comment off a line in the library or else I got an error.
  Since this code only uses the I2C bus for this sensor, comment off this line:
  in the DpsClass.cpp file (Dps368 library > src) 
  - this should be in the libraries folder next to where the sketches are saved. 
  Then ctrl-f search for "setDataMode".

  Comment off *m_spibus->setDataMode(SPI_MODE3;*

  (it should look like this)
  // Init bus
  m_spibus->begin();
  //m_spibus->setDataMode(SPI_MODE3);
  ------------------------------------------------------------------------------
  microSD Card for Adalogger Featherwing (SPI)
  ------------------------------------------------------------------------------
  Uses the 4 wire SPI protocol (MOSI, MISO, SCK and Chip Select).

  Quite easy to setup, just uses the Arduino SD library. Need to define the 
  Chip Select Pin (it's on D-10 for my setup).
  Docs for the library: https://www.arduino.cc/en/Reference/SD
  (Bunch of example codes available).
  
  SD card creates new file with headers for each sensor each day.
  Can change the delimiter from a comma (,) i.e. .csv 
  to a tab (/t) i.e. tsv (.txt).
  SD lib is limited to dos 8.3 filename format, 
  i.e. filename can only be 8 chars.

  I also call SD.begin() if there's an error with the SD card on each loop
  iteration. This means someone can remove the card then pop it back in and the 
  card can still be used without restarting the Arduino.
  ------------------------------------------------------------------------------
  Real Time Clock (RTC) on the Adalogger Featherwing (PCF8523)
  ------------------------------------------------------------------------------
  Again, this uses a coding library: 
  https://github.com/adafruit/RTClib
  (or you can just search RTClib in th Arduino library manager and it's the 
  Adafruit one).

  This...
  https://cdn-learn.adafruit.com/downloads/pdf/adafruit-adalogger-featherwing.pdf
  has some handy tutorials for the RTC and SD card.
  ------------------------------------------------------------------------------
  OLED Screen
  ------------------------------------------------------------------------------
  This was quite tricky to setup. I used an OLED with the SH1106 driver,
  but the SSD1306 is probably a better shout. Adafruit has a library for that:
  https://github.com/adafruit/Adafruit_SSD1306

  but I ended up using this library:
  https://github.com/lexus2k/lcdgfx#key-features
  as it was the only one I could find to work with my Nano33BLE.
  Might not work for other boards. It's just trial and error to get one that 
  works.
  ------------------------------------------------------------------------------
  BLE STUFF
  ------------------------------------------------------------------------------
  Writes data to a Characteristic on a custom Service of a GATT BLE Protocol

  Writes to Characteristic 23 bytes of data whenever the data is available
  (would be nice to change this to only write when there is a significant
  change in the value...)

  11 bytes represent device tag (peripheralLocalName variable) 
  in ASCII encoding.
  https://www.binaryhexconverter.com/hex-to-ascii-text-converter

  The other 3 x 4 bytes correspond to 32 bit floating points (little endian).
  https://www.scadacore.com/tools/programming-calculators/online-hex-converter/
  ______________________________________________________________________________
  BLE SensorsCharacteristic EXAMPLE:
  23 bytes  - hexadecimal (0xNumber) basing: 
  0x6C-61-62-5F-31-0-0-0-0-0-0--61-3B-48-42-F7-CE-CD-41-D2-AE-C7-47

  First 11 bytes:  0x6C-61-62-5F-31-0-0-0-0-0-0 -> ASCII: "lab_1"

  Next 3x4bytes: 0x61-3B-48-42 -> 50.0579872 (the humidity reading)
                 0xF7-CE-CD-41 -> 25.7260571 (the temperature reading)
                 0xD2-AE-C7-47 -> 102237.641 (the pressure reading)              
  ------------------------------------------------------------------------------
*/

// libraries https://learn.adafruit.com/adafruit-all-about-arduino-libraries-install-use)
#include <ArduinoBLE.h>       // Bluetooth LE Library https://www.arduino.cc/en/Reference/ArduinoBLE
#include <Wire.h>             // I2C Library https://www.arduino.cc/en/Reference/Wire
#include <SD.h>               // SD Card Library https://www.arduino.cc/en/Reference/SD
#include <Dps368.h>           // Dps368 I2C Library https://github.com/Infineon/DPS368-Library-Arduino
#include <RTClib.h>           // RTC library https://github.com/adafruit/RTClib
#include "lcdgfx.h"           // OLED Library https://github.com/lexus2k/lcdgfx#key-features
#include "lcdgfx_gui.h"       // "
#include "Logo.h"             // Header file containing the Coherent Logo
// I followed the lcdgfx library examples and used this to get the array: https://javl.github.io/image2cpp/

// i2c slave addresses
#define hyt_Addr 0x28         // temp, humid sensor I2C slave address
#define dps_Addr 0x77         // pressure sensor I2C slave address

// SD card reader (SPI) chip select pin
#define CSPin 10

// Constructors (library classes)
Dps368 Dps368PressSensor = Dps368();  // dps368 (pressure) sensor
RTC_PCF8523 rtc;                      // Adalogger RTC
DisplaySH1106_128x64_I2C display(-1); // OLED SH1106 Display (I2C slave address 0x3C by default) (-1 means there's no reset pin).


// Bluetooth LE
#define monitorServiceUUID "19B10000-E8F2-537E-4F6C-D104768A1214"                           // custom 128-bit UUID for the GATT protocol
#define sensorsCharacteristicUUID "19B10001-E8F2-537E-4F6C-D104768A1214"
BLEService monitorService(serviceUUID);                                                     // monitor GATT service
BLECharacteristic sensorsCharacteristic(sensorsUUID, BLENotify , 23);                       // sensors characteristic
char peripheralLocalName[9] = "DEFAULT";                                                    // BLE device name ('DEFAULT' if SD card is not available on startup)
int connectedBLE = 0;                                                                       // BLE disconnected on startup

// Push button for OLED (see the Digital -> Button example for how this works)
int buttonStatus = 1;  // button initially held high
int screen = 0;        // first screen is the humidity display
#define buttonPin 2    // button I/O Pin

// Battery Voltage Pin
#define batPin A0

#define delim '\t'  // delimeter for files. ',' for .csv, '\t' for .tsv
// change filename ending to .csv in void sd_card_backup if desired.

/*
    Event handler functions, handle peripheral reaction when central connects/ disconnects
    Means I don't need a while loop and if statement to clunk up the code. I can just check
    each loop iteration using BLE.poll()
*/
void ConnectHandler(BLEDevice central) {
  // central connected event handler
  //Serial.print("Connected event, central: ");
  //Serial.println(central.address());
  // update BLE status
  connectedBLE = 1;
  BLE.advertise();
}

void DisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  //Serial.print("Disconnected event, central: ");
  //Serial.println(central.address());
  // update BLE status
  connectedBLE = 0;
  BLE.advertise();
}

void setup() {
  // initialize (init) serial
  Serial.begin(9600);  // 9600 baud rate
  // while (!Serial);  
  // uncomment this if you want the device to wait for serial port to open to start up

  // init i2c
  Wire.begin();
  Dps368PressSensor.begin(Wire);

  // init push button
  pinMode(buttonPin, INPUT_PULLUP);

  // init OLED and print logo for 3 seconds
  // see the library examples to see how this works :)
  display.begin();
  display.fill(0x00);
  display.setFixedFont(ssd1306xled_font8x16);
  display.drawBitmap1(0, 0, 128, 64, Logo);
  lcd_delay(3000);
  display.clear();


  // init RTC
  if (!rtc.begin()) {
    display.clear();
    display.printFixed(0, 0, "RTC init failure ...", STYLE_BOLD);
    while (!rtc.begin());
  }
  // reset the time for a new device
  if (! rtc.initialized() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.start();

  // init SD card
  if (!SD.begin(CSPin)) {
    display.clear();
    display.printFixed(12, 32, "SD Card Error", STYLE_NORMAL);

  }
  else {  // sd available, get the saved name
    // function that reads a text file saved to the SD card to get the last device name
    // if the SD card isn't available on startup the name is 'DEFAULT'.
    get_sd_peripheralLocalName(peripheralLocalName);
  }

  // init BLE
  if (!BLE.begin()) {
    display.clear();
    display.printFixed(0, 0, "BLE init failure ...", STYLE_BOLD);
    while (!BLE.begin());  // waits for BLE to startup if it fails on setup
  }
  // set the Event Handlers I defined earlier
  BLE.setEventHandler(BLEConnected, ConnectHandler);
  BLE.setEventHandler(BLEDisconnected, DisconnectHandler);

  // add the characteristics (sensor data to the service
  monitorService.addCharacteristic(sensorsCharacteristic);

  // add the service
  BLE.addService(monitorService);

  // set advertised local name and service
  BLE.setLocalName(peripheralLocalName);
  BLE.setAdvertisedService(monitorService);

  // start advertising the peripheral
  // advertising with the local name and monitorService UUID
  BLE.advertise();
}





// loop function
void loop() {
  BLE.poll();  // check if the BLE events took place

  /*
   * This is the infinite loop that runs over and over while the device is on.
   * 
   * Calls a bunch of functions I made:
   * ---------------------------------------------------------------------------
   * Function                   Description
   * ---------------------------------------------------------------------------
   * update_peripheralLocalName Checks the serial comms to see if any data is
   *                            available. Converts this data (bytes) into an
   *                            ASCII encoded character array.
   *                            Is a maximum of 8 characters long.
   *                            A null character ('\0') is added after each 
   *                            character othrwise some weird characters print 
   *                            to the OLED.
   *                            Make sure the written data has no newline 
   *                            character.
   * 
   * get_sensors_data           Gets the I2C data from the sensors using the
   *                            Wire library and Dps368 library class.
   *                            These values are saved as a 32-bit float.
   *
   * write_to_BLE               Updates the value of the sensorsCharacteristi.
   *                            If this code is being used for an IOT cloud 
   *                            application this function could be replaced 
   *                            with the WiFi stuff? 
   *                            e.g. void write_to_iot(data)
   *                            
   * OLED_screens_print         Converts all the floats into ASCII character 
   *                            arrays and prints to the OLED. Bunch of 
   *                            different screens available. The screen 
   *                            switches depending on a value which 
   *                            increases when a button state changes. 
   *                            This resets to zero once the final screen is 
   *                            reached.
   * 
   * sd_card_backup             Writes the data to a file on the SD card.
   *                            Filename is todays date 'DDMMYYY' 
   *                            (limited to 8 chars). Filename ending is 
   *                            .txt by default. Prints sensor data 
   *                            seperated by tabs ('\t'). Files are written 
   *                            to folders with the peripheralLocalName. 
   *                            If the file does not exist, a new fileis made 
   *                            with headers corresponding to timestamp and 
   *                            the sensor names.
   * ---------------------------------------------------------------------------
    */
    
  // check for new device name
  update_peripheralLocalName(peripheralLocalName);
  // sensor values (3xfloat32)
  float humidity, temperature, pressure;
  // get the i2c sensors data
  get_sensors_data(humidity, temperature, pressure);
  // write to the BLE characteristic
  write_to_BLE(sensorsCharacteristic, humidity, temperature, pressure);
  // print to the OLED
  OLED_screens_print(humidity, temperature, pressure);

  // and back up to SD card
  sd_card_backup(humidity, temperature, pressure);
}






/*
 * All the functions...
 */
 
void get_sensors_data(float & humidity, float & temperature, float & pressure) {
  // send normal mode command to hyt939 sensor
  Wire.beginTransmission(hyt_Addr);
  Wire.write(0x80);
  Wire.endTransmission();
  delay(50);

  // request 4 bytes of data from hyt939 sensor
  // throw error if 4 bytes unavailable
  unsigned int data[4];  // I2C buffer
  Wire.requestFrom(hyt_Addr, 4);
  if (Wire.available() >= 4) {
    for (int i = 0; i < 4; i++) {
      data[i] = Wire.read();
    }
  }
  else {
    display.clear();
    display.printFixed(0, 0, "HYT939 I2C Error", STYLE_BOLD);
    while (1);
  }

  // convert the 4 bytes of data to human-interpretable values
  humidity = (((data[0] & 0x3F) * 256.0) +  data[1]) * (100.0 / 16383.0);
  temperature = (((data[2] * 256.0) + (data[3] & 0xFC)) / 4) * (165.0 / 16383.0) - 40;

  // get pressure reading from dps368 sensor
  uint8_t oversampling = 7;     // dps368 sensor value accuracy, ranges from 0-7
  uint8_t ret = Dps368PressSensor.measurePressureOnce(pressure, oversampling);
  if (ret != 0) {
    // Something has went wrong
    display.clear();
    display.printFixed(0, 0, "DPS368 I2C Error", STYLE_BOLD);
    while (1);
  }
}

void write_to_BLE(BLECharacteristic sensorsCharacteristic, float humidity, float temperature, float pressure) {
  // combine the floats into one 12 byte character array
  byte sensors_data [23];  // 23 byte array for the BLE characteristic data
  byte tag [11];
  for (int i = 0; i < 8; ++i) {
    tag [i] = byte(peripheralLocalName[i]);  // the device name data (converts ascii to bytes)
  }
  for (int i = 8; i < 11; ++i) {
    tag[i] = 0;  // tag is limited to 8 characters, add null char for any other characters here.
  }
  
  // make the floats into 4 bytes
  byte * humid_bytes = (byte *) &humidity;
  byte * temp_bytes = (byte *) &temperature;
  byte * press_bytes = (byte *) &pressure;

  // append the device name to the BLE characteristic data buffer
  for (int i = 0; i < 11; ++i) {
    sensors_data[i] = tag[i];
  }
  // append the sensors data to the BLE characteristic data buffer
  for (int i = 0; i < 4; ++i) {
    sensors_data[i + 11] = humid_bytes[i];
    sensors_data[i + 15] = temp_bytes[i];
    sensors_data[i + 19] = press_bytes[i];
  }

  // print the raw values (for debugging)
  //  Serial.print(sensors_data[0], HEX);
  //  for (int i = 1; i < 22; ++i) {
  //    // print BLE sensor data for debugging
  //    Serial.print("-");
  //    Serial.print(sensors_data[i], HEX);
  //  }
  //  Serial.println();

  // write the data to the ble characteritic
  sensorsCharacteristic.writeValue(sensors_data, 23);
}

void OLED_screens_print(float humidity, float temperature, float pressure) {
  float batVolt = analogRead(batPin) * 5.0 / 1023; // get current battery voltage
  int x_coord;
  char humid_char[8], temp_char[8], press_char[10], batVolt_char[8];

  // make the floats into ascii char arrays for the OLED
  String(humidity).toCharArray(humid_char, 8);
  String(temperature).toCharArray(temp_char, 8);
  String(pressure).toCharArray(press_char, 10);
  String(batVolt).toCharArray(batVolt_char, 8);

  int pinValue = digitalRead(buttonPin);
  if (buttonStatus != pinValue) {
    screen++;
    display.clear();  // refresh the screen if the button is pressed
    if (screen > 4) {
      screen = 0;  // reaches final screen, go back to the start
    }
  }

  // check BLE status and print to the OLED
  if (connectedBLE) {
    display.printFixed(0, 48, "  BLE Connected ", STYLE_NORMAL);  // added spaces here as OLED will not wipe what is already there.
  }
  else {
    display.printFixed(0, 48, "BLE Disconnected", STYLE_NORMAL);
  }

  // Different OLED Screens to Display
  if (screen == 0) { // Show Device Name
    x_coord = (128 - strlen(peripheralLocalName) * 8) / 2;
    display.printFixed(20 , 0, "Device Name", STYLE_BOLD);
    display.printFixed(x_coord, 16, peripheralLocalName, STYLE_BOLD);
  }
  if (screen == 1) { // Show Battery Status
    display.printFixed(4 , 0, "Battery Voltage", STYLE_BOLD);
    display.setTextCursor(40, 16);
    display.write(batVolt_char);
    display.write(" V");
  }
  else if (screen == 2) {  // Show Humidity Reading
    display.printFixed(32, 0, "Humidity", STYLE_BOLD);
    display.setTextCursor(40, 16);
    display.write(humid_char);
    display.write(" %");
  }
  else if (screen == 3) {  // Show Temperature Reading
    display.printFixed(20, 0, "Temperature", STYLE_BOLD);
    display.setTextCursor(28, 16);
    display.write(temp_char);
    display.write(" degC");
  }
  else if (screen == 4) {  // Show Pressure Reading
    display.printFixed(32, 0, "Pressure", STYLE_BOLD);
    display.setTextCursor(20, 16);
    display.write(press_char);
    display.write(" Pa");
  }

  // print to Serial as well
  String sensor_names = peripheralLocalName;  // string with the sensor names
  sensor_names += "_humidity";
  sensor_names += delim;
  sensor_names += peripheralLocalName;
  sensor_names += "_temperature";
  sensor_names += delim;
  sensor_names += peripheralLocalName;
  sensor_names += "_pressure";
  String data = String(humidity);  // string with sensor CSVs
  data += delim;
  data += String(temperature);
  data += delim;
  data += String(pressure);
  Serial.println(peripheralLocalName);
  Serial.println(sensor_names);
  Serial.println(data);

  // delay(50);  // delay for next button press (might be needed?)
}

void sd_card_backup(float humidity, float temperature, float pressure) {
  File dataFile;
  // filename directory
  String directory = peripheralLocalName;
  // append the directory to the filename path
  String filename = directory;
  filename += "/";  // += is used to append character to an Arduino::String
  // append the current date as the filename
  // e.g: DDMMYYYY (8 chars)
  DateTime now = rtc.now();
  String day_now = String(now.day(), DEC);
  String month_now = String(now.month(), DEC);
  String year_now = String(now.year(), DEC);
  if (day_now.length() != 2) {
    filename += "0"; // check the comment below, same thing
  }
  filename += day_now;
  if (month_now.length() != 2) {
    filename += "0"; // makes it so e.g. Janurary is '01' not '1'.
  }
  filename += String(now.month(), DEC);
  filename += String(now.year(), DEC);
  // Serial.println(filename);  // print for debugging
  
  // filename ending
  filename += ".txt";
  // filename += ".csv";

  // string for the timestamp
  String timestampString = String(now.year(), DEC); timestampString += "/"; timestampString += String(now.month(), DEC);
  timestampString += "/"; timestampString += String(now.day(), DEC); timestampString += " ";
  timestampString += String(now.hour(), DEC); timestampString += ":"; timestampString += String(now.minute(), DEC);
  timestampString += ":"; timestampString += String(now.second(), DEC);
  // and sensors data
  String dataString = timestampString;
  dataString += delim; dataString += String(humidity);
  dataString += delim; dataString += String(temperature);
  dataString += delim; dataString += String(pressure);

  // write to the SD card file
  if (!SD.exists(filename)) {
    SD.mkdir(directory);
    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
      display.printFixed(12, 32, "             ", STYLE_NORMAL);  // remove error from the OLED
      // if the file does not yet exist, write the headers
      String headers = "timestamp";
      headers += delim;
      headers += peripheralLocalName;
      headers += "_humidity";
      headers += delim;
      headers += peripheralLocalName;
      headers += "_temperature";
      headers += delim;
      headers += peripheralLocalName;
      headers += "_pressure";
      dataFile.println(headers);
      // then write the data
      dataFile.println(dataString);
      dataFile.close();  // need to close file after doing anything!!!
    }
    else {
      // unable to open file
      display.printFixed(12, 32, "SD Card Error", STYLE_NORMAL);
      SD.begin(CSPin); // try to re-init the card
      // best just to restart the device, but this sometimes works
    }
  }
  else {
    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
      display.printFixed(12, 32, "             ", STYLE_NORMAL);  // remove error from the OLED
      // otherwise just write the data
      dataFile.println(dataString);
      dataFile.close();
    }
    else {
      display.printFixed(12, 32, "SD Card Error", STYLE_NORMAL);
      // unable to open file
      SD.begin(CSPin); // try to re-init the card
      // best just to restart the device, but this sometimes works
    }
  }
}

void update_peripheralLocalName(char peripheralLocalName[9]) {
  // this code is a bit fried, probably a better way to do this.
  // I made this work through trial and error.
  byte buff [8] = "";
  int i = 0;
  while (Serial.available()) {
    if (i < 8) {
      buff[i] = Serial.read();
      if (buff[i] == 0xa) {
        buff[i] = 0;
      }
      else if (buff[i] == 32) {
        buff[i] = 0x5F;  // if a space, make an underscore
      }
      i++;
    }
    else {
      Serial.read();
    }
  }
  if (buff[0] != 0) {
    // clear current device name
    for (int i = 0; i < 120; i = i + 8) {
      display.printFixed(i, 16, " ", STYLE_NORMAL);
    }
    for (byte i = 0; i < 8; i++) {
      peripheralLocalName[i] = (char)buff[i];
      peripheralLocalName[i + 1] = '\0'; // Add a NULL after each character
    }
    peripheralLocalName[9] = '/0';
    // set new name and advertise again
    BLE.setLocalName(peripheralLocalName);
    BLE.advertise();

    // update the current config
    SD.remove("config.txt");  // delete old config
    File configFile = SD.open("config.txt", FILE_WRITE);
    if (configFile) {
      // otherwise just write the data
      configFile.println(peripheralLocalName);
      configFile.close();
    }
    else {
      display.printFixed(12, 32, "SD Card Error", STYLE_NORMAL);
      // unable to open file
    }
  }
}

void get_sd_peripheralLocalName(char peripheralLocalName[9]) {
  byte buff [8] = "";
  File configFile = SD.open("config.txt", FILE_READ);
  if (configFile) {
    int i = 0;
    while (configFile.available()) {
      if (i < 8) {
        buff[i] = configFile.read();
        if (buff[i] == 0xa || buff[i] == 0xd) {
          buff[i] = 0;
        }
        else if (buff[i] == 32) {
          buff[i] = 0x5F;  // if a space, make an underscore
        }
        i++;
      }
      else {
        configFile.read();  // incase someone changes the file manually. Prevent SD card error later.
        // again, got this to work through trial and error.
      }
    }
    if (buff[0] != 0) {  // file wasn't blank
      for (byte i = 0; i < 8; i++) {
        peripheralLocalName[i] = (char)buff[i];
        peripheralLocalName[i + 1] = '\0'; // Add a NULL after each character
      }
      peripheralLocalName[9] = '/0';  // add null at the end too, needed for ascii c++-code.
    }
  }
  else {
    display.printFixed(12, 32, "SD Card Error", STYLE_NORMAL);
    // unable to open file
  }
}
