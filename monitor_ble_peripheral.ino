/*
  Enviromental Monitor
  Script which programs a Nano 33 BLE Sense as a BLE Peripheral Data Logger
  Sends data from I2C sensors over a GATT service to 4 characteristics.

  tagCharacteristic   - Represents the device label name. Read/ Write/ Notify. Written by the central to the peripheral. (different to the BLE name)
  tempCharacterstic   - Represents float value of the current temperature from the HYT 939 temp humidity sensor.
  humidCharacteristic - Represents float value of the current relative humidity from the HYT 939 temp humidity sensor.
  pressCharacteristic - Represents float value of the current air pressure from the HYT 939 temp humidity sensor.

  Also writes data as a .csv to an onboard FAT32 microSD card with an onboard RTC timestamp
*/

// libraries
#include <ArduinoBLE.h>       // Bluetooth LE Library
#include <Wire.h>             // I2C Library
#include <SD.h>               // SD Card Library
#include <Dps368.h>           // Dps368 I2C Library
#include <RTClib.h>           // RTC library

// global variables
#define hyt_Addr 0x28         // temp, humid sensor I2C slave address
#define dps_Addr 0x77         // pressure sensor I2C slave address
#define CSPin 4               // SD card reader chip select

// BLE Objects (MTU : 23 bytes)
BLEService monitorService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLECharacteristic tagCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite | BLENotify , 23);   // env_monitor label
BLEFloatCharacteristic tempCharacteristic("19B10002-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);             // hyt939 sensor temperature
BLEFloatCharacteristic humidCharacteristic("19B10003-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);            // hyt939 sensor humidity
BLEFloatCharacteristic pressCharacteristic("19B10004-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);            // dps368 sensor pressure

// dps368 i2c library objects
Dps368 Dps368PressSensor = Dps368();

// RTC object
RTC_PCF8523 rtc;



void setup() {
  // init LED
  pinMode(LED_BUILTIN, OUTPUT);


  // init Serial and I2C
  Serial.begin(9600);
  //while (!Serial);  // uncomment to wait for serial port for program to run

  Wire.begin();
  Dps368PressSensor.begin(Wire);


  // init BLE
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  // set advertised local name and service
  BLE.setLocalName("Arduino");
  BLE.setAdvertisedService(monitorService);

  // add the characteristics to the service
  monitorService.addCharacteristic(tagCharacteristic);
  monitorService.addCharacteristic(tempCharacteristic);
  monitorService.addCharacteristic(humidCharacteristic);
  monitorService.addCharacteristic(pressCharacteristic);

  // add service (with the characterstics)
  BLE.addService(monitorService);
  // set the initial value for the characeristic:
  //tagCharacteristic.writeValue(0);

  // start advertising the peripheral (Arduino)
  BLE.advertise();

  Serial.println(F("BLE Peripheral Service UUID: "));
  Serial.println(F("19B10000-E8F2-537E-4F6C-D104768A1214"));
  Serial.println(F("\nAdvertising as 'Arduino'...\n"));


  // init SD
  if (!SD.begin(CSPin)) {
    Serial.println("Card failed, or not present.");
    while (1);
  }

  // init RTC
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  // reset the time for new device
  if (! rtc.initialized() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.start();



}


// function which gets i2c data for the hyt939 sensor
// &temp and &humid are pointers
void hyt_ic2(float &humid, float &temp)
{
  // send normal mode command to hyt939
  Wire.beginTransmission(hyt_Addr);
  Wire.write(0x80);
  Wire.endTransmission();
  delay(300);

  // request 4 bytes of data from temp humid sensor
  // throw error if 4 bytes unavailable
  unsigned int data[4];  // I2C buffer
  Wire.requestFrom(hyt_Addr, 4);
  if (Wire.available() >= 4) {
    for (int i = 0; i < 4; i++) {
      data[i] = Wire.read();
    }
  }
  else {
    Serial.println(F("error... i2c failure."));
    while (1);
  }

  // convert the 4 bytes of data to human-interpretable values
  humid = (((data[0] & 0x3F) * 256.0) +  data[1]) * (100.0 / 16383.0);
  temp = (((data[2] * 256.0) + (data[3] & 0xFC)) / 4) * (165.0 / 16383.0) - 40;
}


void loop() {
  float humid;                  // hyt939 relative humidity
  float temp;                   // hyt939 temperature
  float pressure;               // dps368 pressure
  uint8_t oversampling = 7;     // config for dps368 sensor value accuracy, ranges from 0-7
  uint8_t ret;                  // used to check status of dps368 sensor when reading data
  String filename = "test.csv"; // dos 8.3 filename format (limitation of SD lib)
  String dataString;            // string for assembling data to a log (csv)
  String timestampString;       // string for the current RTC timestamp
  File dataFile;                // SD file object

  // listen for BLE centrals to connect to the Arduino:
  BLEDevice central = BLE.central();

  // if a central is connected to peripheral:
  if (central) {
    Serial.print(F("Connected to central: "));
    // print the central's MAC address:
    Serial.println(central.address());
    Serial.println();

    // while the central is still connected to peripheral:
    while (central.connected()) {
      // build a string of the current timestamp in YYYY/MM/DD HH:MM:SS format for the csv
      DateTime now = rtc.now();
      timestampString = String(now.year(), DEC); timestampString += "/"; timestampString += String(now.month(), DEC);
      timestampString += "/"; timestampString += String(now.day(), DEC); timestampString += " ";
      timestampString += String(now.hour(), DEC); timestampString += ":"; timestampString += String(now.minute(), DEC);
      timestampString += ":"; timestampString += String(now.second(), DEC);

      // if the sd card file does not yet exist, write the headers as the first line
      if (!SD.exists(filename)) {
        dataString = "timestamp,";
        dataString += "relhumid";    dataString += " (%),";
        dataString += "temperature"; dataString += " (degC),";
        dataString += "pressure";    dataString += " (Pa)";
        dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile) {
          dataFile.println(dataString);
          dataFile.close();
        }
        else {
          Serial.println(F("error opening file..."));
        }
      }

      // get i2c data from the sensors
      hyt_ic2(humid, temp);
      ret = Dps368PressSensor.measurePressureOnce(pressure, oversampling);
      if (ret != 0) {
        // Something has went wrong. Check example code and lib for more info.
        Serial.print("FAIL! ret = ");
        Serial.println(ret);
        while (1);
      }

      // write to the BLE characteristics
      tempCharacteristic.writeValue(temp);
      humidCharacteristic.writeValue(humid);
      pressCharacteristic.writeValue(pressure);

      // print to the serial monitor for debugging
      Serial.print(F("RTC Timestamp       : ")); Serial.println(timestampString);
      Serial.print(F("Humidity (%)        : ")); Serial.println(humid);
      Serial.print(F("Temperature (degC)  : ")); Serial.println(temp);
      Serial.print(F("Pressure (Pa)       : ")); Serial.println(pressure);
      Serial.println();


      // and write csv line to the SD card
      dataString = timestampString;
      dataString += ","; dataString += String(humid);
      dataString += ","; dataString += String(temp);
      dataString += ","; dataString += String(pressure);

      dataFile = SD.open(filename, FILE_WRITE);
      if (dataFile) {
        dataFile.println(dataString);
        dataFile.close();
      }
      else {
        Serial.println(F("error opening file..."));
      }

      // wait 2 seconds before repeating the loop
      delay(2000);

    }

    // when the central disconnects, print:
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
  }

  // otherwise
  else {
    // build a string of the current timestamp in YYYY/MM/DD HH:MM:SS format for the csv
    DateTime now = rtc.now();
    timestampString = String(now.year(), DEC); timestampString += "/"; timestampString += String(now.month(), DEC);
    timestampString += "/"; timestampString += String(now.day(), DEC); timestampString += " ";
    timestampString += String(now.hour(), DEC); timestampString += ":"; timestampString += String(now.minute(), DEC);
    timestampString += ":"; timestampString += String(now.second(), DEC);

    // if the sd card file does not yet exist, write the headers as the first line
    if (!SD.exists(filename)) {
      dataString = "timestamp,";
      dataString += "relhumid";    dataString += " (%),";
      dataString += "temperature"; dataString += " (degC),";
      dataString += "pressure";    dataString += " (Pa)";


      dataFile = SD.open(filename, FILE_WRITE);
      if (dataFile) {
        dataFile.println(dataString);
        dataFile.close();
      }
      else {
        Serial.println(F("error opening file..."));
      }
    }

    // get the i2c data
    hyt_ic2(humid, temp);  // updates value of temp and humid
    ret = Dps368PressSensor.measurePressureOnce(pressure, oversampling);
    if (ret != 0) {
      // Something has went wrong
      Serial.print("FAIL! ret = ");
      Serial.println(ret);
      while (1);
    }

    // print to the serial monitor for debugging
    Serial.print(F("RTC Timestamp       : ")); Serial.println(timestampString);
    Serial.print(F("Humidity (%)        : ")); Serial.println(humid);
    Serial.print(F("Temperature (degC)  : ")); Serial.println(temp);
    Serial.print(F("Pressure (Pa)       : ")); Serial.println(pressure);
    Serial.println();

    // and write csv line to the SD card
    dataString = timestampString;
    dataString += ","; dataString += String(humid);
    dataString += ","; dataString += String(temp);
    dataString += ","; dataString += String(pressure);

    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
    }
    else {
      Serial.println(F("error opening file..."));
    }
    delay(2000);
  }
}
