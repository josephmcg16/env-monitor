/*
  Environmental Monitor central sketch
  19/07/2021

  Author: Joseph McGovern

  Reads data from an environment monitor over BLE (peripheral) and prints to Serial.
  See the BLE examples for something similar:
  https://www.arduino.cc/en/Reference/ArduinoBLE

  Data received over GATT protocol.

  GATT protocol contains a GATT service with a characteristic.
  Characteristic receives 23 bytes of data.

  11 bytes represent device tag (peripheralLocalName)

  The other 3 x 4 bytes correspond to 32 bit floating points.


*/

#include <ArduinoBLE.h>       // BLE library https://www.arduino.cc/en/Reference/ArduinoBLE
#include "lcdgfx.h"           // OLED Library https://github.com/lexus2k/lcdgfx#key-features

#define deviceLocalName "Arduino_CENTRAL"

// define UUIDs here (defined in documentation, 'arduino_ble_uuids.doc')
#define serviceUUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define sensorsUUID "19B10001-E8F2-537E-4F6C-D104768A1214"

#define resetPin 7

#define delim '\t'

// Constructors
DisplaySH1106_128x64_I2C display(-1); // OLED SH1106 Display (I2C slave address 0x3C by default)

void setup() {
  // init reset
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, HIGH);

  // init OLED
  display.begin();
  display.clear();
  display.setFixedFont(ssd1306xled_font8x16);
  display.printFixed(20, 0, "BLE Central", STYLE_BOLD);
  display.printFixed(4, 16, deviceLocalName, STYLE_BOLD);
  display.printFixed(0, 48, "BLE Disconnected", STYLE_NORMAL);

  // init Serial
  Serial.begin(9600);
  while (!Serial); // wait for Serial port to open
  // init BLE
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  Serial.println("BLE Central");

  BLE.scanForUuid(serviceUUID);
}

void loop() {
  // if serial closes, reset
  if (!Serial) {
    digitalWrite(resetPin, LOW);
  }
  // check if peripheral with serviceUUID has been discovered
  BLEDevice peripheral = BLE.available();

  // found peripheral
  if (peripheral) {
    Serial.print("Found peripheral : ");
    Serial.println(peripheral.localName());
    // get desired peripheralName from Serial read
    char peripheralName[8];
    get_peripheralName(peripheralName);
    // if desired peripheral is available,
    // stop scan and get data
    if (peripheral.localName() == peripheralName) {
      BLE.stopScan();

      listen_for_monitor(peripheral);

      Serial.println("Rescanning for UUID");
      // peripheral has disconnected, start scanning again
      BLE.scanForUuid(serviceUUID);
    }
  }
}

void listen_for_monitor(BLEDevice peripheral) {
  // connect to peripheral
  Serial.println("Connecting ...");
  if (peripheral.connect()) {
    Serial.println("Connected.");
  }
  else {
    Serial.println("Failed to connect ...");
    return;
  }

  // discover attributes
  //int timeold = millis();
  Serial.println("Discovering attributes ...");
  if (peripheral.discoverAttributes()) {
    Serial.println("Attributes discovered");
  } else {
    // try again
    delay(10);
    if (peripheral.discoverAttributes()) {
      Serial.println("Attributes discovered");
    } else {
      Serial.println("Attribute discovery failed!");
      peripheral.disconnect();
      return;
    }
  }
  //Serial.print("Discovery Time : ");
  //Serial.println(String(millis() - timeold));

  BLECharacteristic sensorsCharacteristic = peripheral.characteristic(sensorsUUID);

  if (!sensorsCharacteristic.subscribe()) {
    Serial.println(F("subscription failed!"));
    peripheral.disconnect();
    return;
  } else {
    Serial.println(F("Subscribed."));
  }
  display.printFixed(0, 48, "  BLE Connected ", STYLE_NORMAL);
  // loop while connected to peripheral
  while (peripheral.connected()) {
    if (!Serial) {
      digitalWrite(resetPin, LOW);
    }
    if (sensorsCharacteristic.valueUpdated()) {
      // read the data from the sensors characteristic as 3xfloat32 for each sensor value
      float humidity, temperature, pressure;
      String tag = "";
      read_sensorsCharacteristic(sensorsCharacteristic, humidity, temperature, pressure, tag);

      // print to Serial
      String sensor_names = tag;  // string with the sensor names
      sensor_names += "_humidity";
      sensor_names += delim;
      sensor_names += tag;
      sensor_names += "_temperature";
      sensor_names += delim;
      sensor_names += tag;
      sensor_names += "_pressure";
      String data_csv = String(humidity);  // string with sensor CSVs
      data += delim;
      data += String(temperature);
      data += delim;
      data += String(pressure);
      Serial.println(peripheral.localName());
      Serial.println(sensor_names);
      Serial.println(data);
    }
  }
  display.printFixed(0, 48, "BLE Disconnected", STYLE_NORMAL);
  Serial.println("Peripheral disconnected.");
}


void read_sensorsCharacteristic(BLECharacteristic sensorsCharacteristic, float & humidity, float & temperature, float & pressure, String & tag) {
  /*
    Function which a characteristic containing a 12 byte array for 3 sensor values
    and converts the 12 byte array into 3 buffers of length 4 bytes.

    - Humidity     (byte 0-3)
    - Temperature  (byte 4-7)
    - Pressure     (byte 8-11)

    The buffers are then converted from the byte array
    to a human-readable 32-bit floating point.
  */
  // read 23 bytes (11 byte ASCII and 3 x float32) from the sensor characteristic into an array
  char sensors_data [23];
  sensorsCharacteristic.readValue(sensors_data, 23);

  // first 11 bytes for ASCII string for the tag
  // next 4 bytes for humidity, next 4 bytes for temperature
  // last 4 for pressure float32s
  char tag_buffer[11];
  char humid_buffer[4];
  char temp_buffer[4];
  char press_buffer[4];
  for (int i = 0; i < 11; ++i) {
    tag_buffer[i] = sensors_data[i];
  }
  for (int i = 0; i < 4; ++i) {
    humid_buffer[i] = sensors_data[i + 11];
    temp_buffer[i] = sensors_data[i + 15];
    press_buffer[i] = sensors_data[i + 19];
  }

  // convert from bytes to readable floats (%, degC, Pa)
  tag = String(tag_buffer);
  humidity = *(float *)&humid_buffer;
  temperature = *(float *)&temp_buffer;
  pressure = *(float *)&press_buffer;
}

void get_peripheralName(char peripheralName[8]) {
  byte buff [8] = "";
  int i = 0;
  while (Serial.available()) {
    if (i < 8) {
      buff[i] = Serial.read();
      i++;
    }
    else {
      Serial.read();
    }
  }
  for (byte i = 0; i < 8; i++) {
    peripheralName[i] = (char)buff[i];
    peripheralName[i + 1] = '\0'; // Add a NULL after each character
  }
}
