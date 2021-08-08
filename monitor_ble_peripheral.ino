/*
  Environment Monitor BLE Peripheral

  Author: Joseph McGovern, 2021 Saltire Scholar Intern

  BLE Peripheral which recieves data from sensors over I2C bus

  Prints to an LCD Display and backups to SD card.
  SD card can be removed during runtime, but to avoid issues;
  shutdown the device before removing the SD card.

  Writes data to a Characteristic on a custom Service of a GATT BLE Protocol

  Writes to Characteristic 23 bytes of data ~every 3 seconds.

  11 bytes represent device tag (peripheralLocalName var)

  The other 3 x 4 bytes correspond to 32 bit floating points.
*/

// libraries
#include <ArduinoBLE.h>       // Bluetooth LE Library
#include <Wire.h>             // I2C Library
#include <SD.h>               // SD Card Library
#include <Dps368.h>           // Dps368 I2C Library
#include <RTClib.h>           // RTC library
#include "lcdgfx.h"           // OLED Library
#include "lcdgfx_gui.h"       // "
#include "Logo.h"             // Header file containing the Coherent Logo

// i2c slave addresses
#define hyt_Addr 0x28         // temp, humid sensor I2C slave address
#define dps_Addr 0x77         // pressure sensor I2C slave address

// SD card reader chip select pin
#define CSPin 10

// Constructors
Dps368 Dps368PressSensor = Dps368();  // dps368 (pressure) sensor
RTC_PCF8523 rtc;                      // Adalogger RTC
DisplaySH1106_128x64_I2C display(-1); // OLED SH1106 Display (I2C slave address 0x3C by default)


// Bluetooth LE
#define serviceUUID "19B10000-E8F2-537E-4F6C-D104768A1214"                                  // custom 128-bit UUID
#define sensorsUUID "19B10001-E8F2-537E-4F6C-D104768A1214"
BLEService monitorService(serviceUUID);                                                     // monitor GATT service
BLECharacteristic sensorsCharacteristic(sensorsUUID, BLENotify , 23);                       // sensors characteristic
char peripheralLocalName[9] = "DEFAULT";                                                    // BLE device name
int connectedBLE = 0;;

// Push button for OLED
int buttonStatus = 1;  // button initially held high
int screen = 0;        // first screen is the humidity display
#define buttonPin 2    // button I/O Pin

// Battery Voltage Pin
#define batPin A0
/*
    Event handler functions, handle peripheral reaction when central connects/ disconnects
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
  // init serial
  Serial.begin(9600);
  // while (!Serial);

  // init i2c
  Wire.begin();
  Dps368PressSensor.begin(Wire);

  // init push button
  pinMode(buttonPin, INPUT_PULLUP);

  // init OLED and print logo
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
    get_sd_peripheralLocalName(peripheralLocalName);
  }

  // init BLE
  if (!BLE.begin()) {
    display.clear();
    display.printFixed(0, 0, "BLE init failure ...", STYLE_BOLD);
    while (!BLE.begin());
  }

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

void loop() {
  BLE.poll();
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


// includes address pointers
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
    Serial.println("Hyt939 I2C Error");
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
    Serial.println("Dps 368 I2C error");
    while (1);
  }
}

void write_to_BLE(BLECharacteristic sensorsCharacteristic, float humidity, float temperature, float pressure) {
  // combine the floats into one 12 byte character array
  byte sensors_data [23];
  byte tag [11];
  for (int i = 0; i < 8; ++i) {
    tag [i] = byte(peripheralLocalName[i]);
  }
  for (int i = 8; i < 11; ++i) {
    tag[i] = 0;
  }
  byte * humid_bytes = (byte *) &humidity;
  byte * temp_bytes = (byte *) &temperature;
  byte * press_bytes = (byte *) &pressure;
  for (int i = 0; i < 11; ++i) {
    sensors_data[i] = tag[i];
  }
  for (int i = 0; i < 4; ++i) {
    sensors_data[i + 11] = humid_bytes[i];
    sensors_data[i + 15] = temp_bytes[i];
    sensors_data[i + 19] = press_bytes[i];
  }
  //  Serial.print(sensors_data[0], HEX);
  //  for (int i = 1; i < 22; ++i) {
  //    // print BLE sensor data for debugging
  //    Serial.print("-");
  //    Serial.print(sensors_data[i], HEX);
  //  }
  //  Serial.println();
  sensorsCharacteristic.writeValue(sensors_data, 23);
}

void OLED_screens_print(float humidity, float temperature, float pressure) {
  float batVolt = analogRead(batPin) * 5.0/1023;  // get current battery voltage
  int x_coord;
  char humid_char[8], temp_char[8], press_char[10], batVolt_char[8];

  String(humidity).toCharArray(humid_char, 8);
  String(temperature).toCharArray(temp_char, 8);
  String(pressure).toCharArray(press_char, 10);
  String(batVolt).toCharArray(batVolt_char, 8);

  int pinValue = digitalRead(buttonPin);
  if (buttonStatus != pinValue) {
    screen++;
    display.clear();  // refresh the screen
    if (screen > 4) {
      screen = 0;
    }
  }

  // check BLE status and print to the OLED
  if (connectedBLE) {
    display.printFixed(0, 48, "  BLE Connected ", STYLE_NORMAL);
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
  sensor_names += "_humidity,";
  sensor_names += peripheralLocalName;
  sensor_names += "_temperature,";
  sensor_names += peripheralLocalName;
  sensor_names += "_pressure";
  String data_csv = String(humidity);  // string with sensor CSVs
  data_csv += ",";
  data_csv += String(temperature);
  data_csv += ",";
  data_csv += String(pressure);
  Serial.println(sensor_names);
  Serial.println(data_csv);

  // delay(50);  // delay for next button press
}

void sd_card_backup(float humidity, float temperature, float pressure) {
  File dataFile;
  // filename directory
  String directory = peripheralLocalName;
  // append the directory to the filename path
  String filename = directory;
  filename += "/";
  // append the current date as the filename
  // e.g: DDMMYYYY (8 chars)
  DateTime now = rtc.now();
  String day_now = String(now.day(), DEC);
  String month_now = String(now.month(), DEC);
  String year_now = String(now.year(), DEC);
  if (day_now.length() != 2) {
    filename += "0";
  }
  filename += day_now;
  if (month_now.length() != 2) {
    filename += "0";
  }
  filename += String(now.month(), DEC);
  filename += String(now.year(), DEC);
  // Serial.println(filename);
  // filename ending
  filename += ".csv";

  // string for the timestamp
  String timestampString = String(now.year(), DEC); timestampString += "/"; timestampString += String(now.month(), DEC);
  timestampString += "/"; timestampString += String(now.day(), DEC); timestampString += " ";
  timestampString += String(now.hour(), DEC); timestampString += ":"; timestampString += String(now.minute(), DEC);
  timestampString += ":"; timestampString += String(now.second(), DEC);
  // and sensors data
  String dataString = timestampString;
  dataString += ","; dataString += String(humidity);
  dataString += ","; dataString += String(temperature);
  dataString += ","; dataString += String(pressure);

  // write to the SD card file
  if (!SD.exists(filename)) {
    SD.mkdir(directory);
    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
      display.printFixed(12, 32, "             ", STYLE_NORMAL);  // remove error from the OLED
      // if the file does not yet exist, write the headers
      String headers = "timestamp, ";
      headers += peripheralLocalName;
      headers += "_humidity, ";
      headers += peripheralLocalName;
      headers += "_temperature, ";
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
      }
    }
    if (buff[0] != 0) {  // file wasn't blank
      for (byte i = 0; i < 8; i++) {
        peripheralLocalName[i] = (char)buff[i];
        peripheralLocalName[i + 1] = '\0'; // Add a NULL after each character
      }
      peripheralLocalName[9] = '/0';
    }
  }
  else {
    display.printFixed(12, 32, "SD Card Error", STYLE_NORMAL);
    // unable to open file
  }
}
