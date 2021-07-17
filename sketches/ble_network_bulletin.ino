/*
  Enviromental Monitor

*/

#include <ArduinoBLE.h>

#define deviceLocalName "Arduino_MASTER"

// define UUIDs here (defined in documentation, 'arduino_ble_uuids.doc')
#define serviceUUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define sensorsUUID "19B10001-E8F2-537E-4F6C-D104768A1214"

// monitor GATT service
BLEService monitorService(serviceUUID);

// device characteristics
BLECharacteristic sensorsCharacteristic(sensorsUUID, BLERead | BLEWrite | BLENotify , 23);

void setup() {
  // init Serial
  Serial.begin(9600);
  while (!Serial); // wait for Serial port to open

  // init BLE
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  // add the characteristics (sensor data to the service
  monitorService.addCharacteristic(sensorsCharacteristic);

  // add the service
  BLE.addService(monitorService);

  // set advertised local name and service
  BLE.setLocalName(deviceLocalName);
  BLE.setAdvertisedService(monitorService);

  // start advertising the peripheral
  // advertising with the local name and monitorService UUID
  BLE.advertise();

  Serial.print(F("Advertising as '"));
  Serial.print(deviceLocalName);
  Serial.println(F("'"));

}

void loop() {
  // listen for BLE centrals to connect to the Arduino:
  BLEDevice central = BLE.central();


  // if a central is connected
  if (central) {
    Serial.println(F("Connected to a central"));
    while (central.connected()) {
      if (sensorsCharacteristic.written()) {
        // read the data from the sensors characteristic as 3xfloat32 for each sensor value
        float humidity, temperature, pressure;
        String tag = "";
        read_sensorsCharacteristic(sensorsCharacteristic, humidity, temperature, pressure, tag);

        // and print to serial
        Serial.print(F("central tag   : '"));
        Serial.print(tag);
        Serial.println("'");
        
        Serial.print(F("humidity      : "));
        Serial.print(humidity);
        Serial.println(F(" %"));

        Serial.print(F("temperature   : "));
        Serial.print(temperature);
        Serial.println(F(" degC"));

        Serial.print(F("pressure      : "));
        Serial.print(pressure);
        Serial.println(F(" Pa"));
      }
    }
    Serial.print(F("central disconnected"));
  }
}



void read_sensorsCharacteristic(BLECharacteristic sensorsCharacteristic, float &humidity, float &temperature, float &pressure, String &tag) {
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
