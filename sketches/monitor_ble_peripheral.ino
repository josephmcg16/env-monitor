/*
  Environment Monitor BLE Peripheral

  Author: Joseph McGovern

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

// UUIDs for the BLE services and characteristics (defined in documentation, 'arduino_ble_uuids.doc')
#define serviceUUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define sensorsUUID "19B10001-E8F2-537E-4F6C-D104768A1214"


// Bluetooth LE
#define serviceUUID "19B10000-E8F2-537E-4F6C-D104768A1214"                                  // custom 128-bit UUID
#define sensorsUUID "19B10001-E8F2-537E-4F6C-D104768A1214"
BLEService monitorService(serviceUUID);                                                     // monitor GATT service
BLECharacteristic sensorsCharacteristic(sensorsUUID, BLERead | BLEWrite | BLENotify , 23);  // sensors characteristic
#define peripheralLocalName "lab_test"                                                      // BLE device names
int connectedBLE = 0;

// Push button for OLED
int buttonStatus = 1;  // button initially held high
int screen = 0;        // first screen is the humidity display
#define buttonPin 2    // button I/O Pin
/*
    Event handler functions, handle peripheral reaction when central connects/ disconnects
*/
void ConnectHandler(BLEDevice central) {
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
  // update BLE status
  connectedBLE = 1;
  BLE.advertise();
}

void DisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
  // update BLE status
  connectedBLE = 0;
  BLE.advertise();
}

void setup() {
  // init serial
  Serial.begin(9600);

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
    lcd_delay(1000);
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
    display.printFixed(0, 0, "SD init failure ...", STYLE_BOLD);
    lcd_delay(1000);
    while (!SD.begin(CSPin));
  }

  // init BLE
  if (!BLE.begin()) {
    display.clear();
    display.printFixed(0, 0, "BLE init failure ...", STYLE_BOLD);
    lcd_delay(1000);
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
  delay(300);

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
  byte tag [11] = peripheralLocalName;
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
  for (int i = 0; i < 22; ++i) {
    Serial.print(sensors_data[i], HEX);
    Serial.print("-");
  }
  Serial.println(sensors_data[22]);
  sensorsCharacteristic.writeValue(sensors_data, 23);
}

void OLED_screens_print(float humidity, float temperature, float pressure) {

  char humid_char[8], temp_char[8], press_char[10];

  String(humidity).toCharArray(humid_char, 8);
  String(temperature).toCharArray(temp_char, 8);
  String(pressure).toCharArray(press_char, 10);

  int pinValue = digitalRead(buttonPin);
  if (buttonStatus != pinValue) {
    screen++;
    display.clear();  // refresh the screen
    if (screen > 2) {
      screen = 0;
    }
  }

  // check BLE status and print to the OLED
  if (connectedBLE) {
    display.printFixed(0, 48, " BLE Connected  ", STYLE_NORMAL);
  }
  else {
    display.printFixed(0, 48, "BLE Disconnected", STYLE_NORMAL);
  }

  if (screen == 0) {
    display.printFixed(32, 0, "Humidity", STYLE_BOLD);
    display.setTextCursor(40, 16);
    display.write(humid_char);
    display.write(" %");

  }
  else if (screen == 1) {
    display.printFixed(20, 0, "Temperature", STYLE_BOLD);
    display.setTextCursor(28, 16);
    display.write(temp_char);
    display.write(" degC");
  }
  // display humidity
  else if (screen == 2) {
    //display.setTextCursor(32, 16);
    //display.write("PRESSURE");
    display.printFixed(32, 0, "Pressure", STYLE_BOLD);
    display.setTextCursor(20, 16);
    display.write(press_char);
    display.write(" Pa");
  }

  delay(50);
}

void sd_card_backup(float humidity, float temperature, float pressure) {
  File dataFile;
  // filename directory
  String filename = peripheralLocalName;
  filename += "/";
  // append the current date as the filename
  // e.g: DDMMYYYY (8 chars)
  DateTime now = rtc.now();
  filename += String(now.day(), DEC);
  filename += String(now.month(), DEC);
  filename += String(now.year(), DEC);
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
    SD.mkdir(peripheralLocalName);
    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
      // if the file does not yet exist, write the headers
      Serial.println("Writing new file");
      dataFile.println("timestamp, humidity (%), temperature (degC), pressure (Pa)");
      // then write the data
      dataFile.println(dataString);
      dataFile.close();
      delay(1000);
    }
    else {
      // unable to open file
      Serial.println("Error opening file");
      SD.begin(CSPin); // try to re-init the card
      // best just to restart the device, but this sometimes works
    }
  }
  else {
    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
      // otherwise just write the data
      dataFile.println(dataString);
      dataFile.close();
    }
    else {
      // unable to open file
      Serial.println("Error opening file");
      SD.begin(CSPin); // try to re-init the card
      // best just to restart the device, but this sometimes works
    }

  }
}
