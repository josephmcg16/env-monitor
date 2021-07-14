/*
  Enviromental Monitor
  Script which programs a Nano 33 BLE Sense as a BLE Central Data Logger
  Recieves data from another Nano 33 BLE Sense over a GATT service with 4 characteristics.

  tagCharacteristic   - Represents the peripheral device label name. Read/ Write/ Notify. Written by the central to the peripheral. (different to the BLE name)
  tempCharacterstic   - Represents float value of the current temperature from the HYT 939 temp humidity sensor.
  humidCharacteristic - Represents float value of the current relative humidity from the HYT 939 temp humidity sensor.
  pressCharacteristic - Represents float value of the current air pressure from the HYT 939 temp humidity sensor.
*/

#include <ArduinoBLE.h>

#define service_UUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define tag_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"
#define temp_UUID "19B10002-E8F2-537E-4F6C-D104768A1214"
#define humid_UUID "19B10003-E8F2-537E-4F6C-D104768A1214"
#define press_UUID "19B10004-E8F2-537E-4F6C-D104768A1214"

void setup() {
  Serial.begin(9600);
  while (!Serial); // wait for ser port to open

  // init BLE
  BLE.begin();

  // scan for peripherals
  BLE.scanForUuid(service_UUID);
  Serial.print("Scanning for UUID: '");
  Serial.print(service_UUID);
  Serial.println("' ...");
}

void get_data_and_print(BLEDevice peripheral) {
  // connect to the peripheral
  Serial.println(F("Connecting ..."));
  if (peripheral.connect()) {
    Serial.println(F("Connected."));
  } else {
    Serial.println(F("Failed to Connect"));
    return;
  }

  // discover peripheral attributes
  Serial.println(F("Discovering service ... (this can take up to 2 mins)"));
  if (peripheral.discoverService(service_UUID)) {
    Serial.println(F("Service discovered."));
  } else {
    Serial.println(F("Attribute discovery failed."));
    peripheral.disconnect();
    return;
  }

  // retrive the service characteristics
  //BLECharacteristic tagCharacteristic = peripheral.characteristic("19B10001-E8F2-537E-4F6C-D104768A1214");
  BLECharacteristic tempCharacteristic = peripheral.characteristic("19B10002-E8F2-537E-4F6C-D104768A1214");
  BLECharacteristic humidCharacteristic = peripheral.characteristic("19B10003-E8F2-537E-4F6C-D104768A1214");
  BLECharacteristic pressCharacteristic = peripheral.characteristic("19B10004-E8F2-537E-4F6C-D104768A1214");

  // subscribe to the tempCharacteristic
  Serial.println(F("Subscribing to temperature characteristic ..."));
  if (!tempCharacteristic) {
    Serial.println(F("no temperature characteristic found!"));
    peripheral.disconnect();
    return;
  } else if (!tempCharacteristic.canSubscribe()) {
    Serial.println(F("temperature characteristic is not subscribable!"));
    peripheral.disconnect();
    return;
  } else if (!tempCharacteristic.subscribe()) {
    Serial.println(F("subscription failed!"));
    peripheral.disconnect();
    return;
  } else {
    Serial.println(F("Subscribed."));
    Serial.println();
  }

  // subscribe to the humidCharacteristic
  Serial.println(F("Subscribing to humidity characteristic ..."));
  if (!humidCharacteristic) {
    Serial.println(F("no humidity characteristic found!"));
    peripheral.disconnect();
    return;
  } else if (!humidCharacteristic.canSubscribe()) {
    Serial.println(F("humidity characteristic is not subscribable!"));
    peripheral.disconnect();
    return;
  } else if (!humidCharacteristic.subscribe()) {
    Serial.println(F("subscription failed!"));
    peripheral.disconnect();
    return;
  } else {
    Serial.println("Subscribed.");
    Serial.println();
  }

  // subscribe to the pressCharacteristic
  Serial.println(F("Subscribing to pressure characteristic ..."));
  if (!pressCharacteristic) {
    Serial.println(F("no pressure characteristic found!"));
    peripheral.disconnect();
    return;
  } else if (!pressCharacteristic.canSubscribe()) {
    Serial.println("pressure characteristic is not subscribable!");
    peripheral.disconnect();
    return;
  } else if (!pressCharacteristic.subscribe()) {
    Serial.println(F("subscription failed!"));
    peripheral.disconnect();
    return;
  } else {
    Serial.println(F("Subscribed."));
    Serial.println();
  }

  while (peripheral.connected()) {
    // wait for serial command
    while (Serial.available() == 0);
    String tag = Serial.readStringUntil('\n');
    delay(50);

    // check if sensor values are updated
    if (tempCharacteristic.valueUpdated()
        && humidCharacteristic.valueUpdated()
        && pressCharacteristic.valueUpdated()) {
      // read the values, MTU is 23 bytes
      char temp_buffer[4];    // 32 bit float (4 byte array)
      char humid_buffer[4];   // 32 bit float (4 byte array)
      char press_buffer[4];   // 32 bit float (4 byte array)

      tempCharacteristic.readValue(temp_buffer, 4);
      humidCharacteristic.readValue(humid_buffer, 4);
      pressCharacteristic.readValue(press_buffer, 4);

      // convert byte arrays to floats
      float temp = *(float *)&temp_buffer;
      float humid = *(float *)&humid_buffer;
      float pressure = *(float *)&press_buffer;

      // and print to serial
      Serial.println("timestamp");
      Serial.print(tag); Serial.println("_relhumid");
      Serial.println(humid);
      Serial.println("%");
      Serial.print(tag); Serial.println("_temp");
      Serial.println(temp);
      Serial.println("degC");
      Serial.print(tag); Serial.println("_pressure");
      Serial.println(pressure);
      Serial.println("Pa");
      Serial.println();
    }
  }
}

void loop() {

  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral)
  {
    // found a peripheral
    Serial.println(F("Device Found: "));
    Serial.print(F("Address - "));
    Serial.println(peripheral.address());
    Serial.print(F("Device name - "));
    Serial.print(F(" '"));
    Serial.print(peripheral.localName());
    Serial.print(F("' "));
    Serial.println();

    // stop scanning
    BLE.stopScan();

    // start monitoring
    get_data_and_print(peripheral);
    // periperal disconnected, start scanning again
    Serial.println("Peripheral disconnected");
    Serial.println();
    Serial.print("Re-Scanning for UUID: '");
    Serial.print(service_UUID);
    Serial.println("' ...");
    Serial.println();
  }
  // periperal disconnected, start scanning again
  BLE.scanForUuid("19B10000-E8F2-537E-4F6C-D104768A1214");

}
