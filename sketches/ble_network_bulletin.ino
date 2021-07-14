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

#define deviceLocalName "Arduino"

// define UUIDs here (defined in documentation, arduino_ble_uuids.doc)
#define service_UUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define test_filename8_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"
#define test_sensors_UUID "19B10002-E8F2-537E-4F6C-D104768A1214"

// monitor GATT service
BLEService monitorService(service_UUID);

// device characteristics
BLECharacteristic test_filename8Characteristic(test_filename8_UUID, BLERead | BLEWrite | BLENotify , 8);
BLECharacteristic test_sensorsCharacteristic(test_sensors_UUID, BLERead | BLEWrite | BLENotify , 12);

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
  monitorService.addCharacteristic(test_filename8Characteristic);
  monitorService.addCharacteristic(test_sensorsCharacteristic);

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
    Serial.print(F("Connected to central : '"));
    Serial.print(central.localName());
    Serial.println(F("'"));
    while (central.connected()) {

      // check if new filename available from serial
      if (Serial.available()) {
        write_filename8Characteristic();
      }

      if (test_sensorsCharacteristic.written()) {
        // read the data from the sensors characteristic as 3xfloat32 for each sensor value
        float humidity, temperature, pressure;
        read_sensorCharacteristic(test_sensorsCharacteristic, humidity, temperature, pressure);

        // and print to serial
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
    Serial.print(F("central '"));
    Serial.print(central.localName());
    Serial.println(F("' disconnected."));
  }
}



/*
 * Function which a characteristic containing a 12 byte array for 3 sensor values
 * and converts the 12 byte array into 3 buffers of length 4 bytes.
 * 
 * - Humidity     (byte 0-3)
 * - Temperature  (byte 4-7)
 * - Pressure     (byte 8-11)
 * 
 * The buffers are then converted from the byte array 
 * to a human-readable 32-bit floating point.
 */
void read_sensorCharacteristic(BLECharacteristic test_sensorsCharacteristic, float & humidity, float & temperature, float & pressure) {
  // read 12 bytes (3 x float32) from the sensor characteristic into an array
  char sensors_data [12];
  test_sensorsCharacteristic.readValue(sensors_data, 12);

  // first 4 bytes for humidity, next 4 bytes for temperature and last 4 for pressure
  char humid_buffer[4];
  char temp_buffer[4];
  char press_buffer[4];

  for (int i = 0; i < 4; ++i) {
    humid_buffer[i] = sensors_data[i];
    temp_buffer[i] = sensors_data[i + 4];
    press_buffer[i] = sensors_data[i + 8];
  }

  // convert from bytes to readable floats (%, degC, Pa)
  humidity = *(float *)&humid_buffer;
  temperature = *(float *)&temp_buffer;
  pressure = *(float *)&press_buffer;
}


/*
 * Function which reads data from the serial buffer and writes to
 * a characteristic used for the filename of the csv file of the 
 * datalogger.
 */
void write_filename8Characteristic() {
  char filename8[8] = "";
  // read the first 8 bytes into the buffer
  Serial.readBytes(filename8, 8);
  // write value to the connected device filename characteristic
  test_filename8Characteristic.writeValue(filename8);
  // remove any buffered serial data
  while (Serial.available()) {
    Serial.read();
  }
}
