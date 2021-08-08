// BLE NETWORK DEVICE TEST

// libraries
#include <ArduinoBLE.h>       // Bluetooth LE Library
#include <Wire.h>             // I2C Library
#include <SD.h>               // SD Card Library
#include <Dps368.h>           // Dps368 I2C Library
#include <RTClib.h>           // RTC library
#include <LiquidCrystal.h>

// i2c slave addresses
#define hyt_Addr 0x28         // temp, humid sensor I2C slave address
#define dps_Addr 0x77         // pressure sensor I2C slave address

// SD card reader chip select pin
#define CSPin 10

// 16x2 lcd pins
const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
// lcd object
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// dps368 (pressure) sensor object
Dps368 Dps368PressSensor = Dps368();

// UUIDs for the BLE services and characteristics (defined in documentation, 'arduino_ble_uuids.doc')
#define serviceUUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define sensorsUUID "19B10001-E8F2-537E-4F6C-D104768A1214"

// BLE device names
#define deviceTag "lab_test"

void setup() {
  // init LCD
  lcd.begin(16, 2);
  lcd.clear();

  // init serial
  Serial.begin(9600);

  // init i2c
  Wire.begin();
  Dps368PressSensor.begin(Wire);

  // init RTC
  RTC_PCF8523 rtc;
  if (!rtc.begin()) {
    lcd.clear();
    lcd.print("starting rtc");
    lcd.setCursor(0, 1);
    lcd.print("failed ...");
    while (!rtc.begin());
  }

  // reset the time for new device
  if (! rtc.initialized() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.start();

  // init BLE
  if (!BLE.begin()) {
    lcd.clear();
    lcd.print("starting BLE");
    lcd.setCursor(0, 1);
    lcd.print("failed ...");
    while (!BLE.begin());
  }
  lcd.clear();
  lcd.print("Initilization");
  lcd.setCursor(0, 1);
  lcd.print("Successful!");
  delay(1000);
}

void loop() {
  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    // discovered a peripheral
    Serial.print("Found peripheral '");
    Serial.print(peripheral.localName());
    Serial.println("'");

    // found the service, stop scanning
    BLE.stopScan();

    // connect to the peripheral
    Serial.println("Connecting...");

    if (peripheral.connect()) {
      Serial.println("Connected!");
    } else {
      Serial.println("Failed to connect...");
      return;
    }

    // discover peripheral attributes
    lcd.clear();
    lcd.print("Discovering");
    lcd.setCursor(0, 1);
    lcd.print("Attributes ...");
    if (peripheral.discoverAttributes()) {
      Serial.println("Attributes Discovered!");
    } else {
      Serial.println("Attribute Discovery Failed...");
      peripheral.disconnect();
      BLE.scanForUuid(serviceUUID);
      return;
    }
    lcd.clear();
    lcd.print("BLE Connected");
    // retrive the characteristics
    BLECharacteristic sensorsCharacteristic = peripheral.characteristic(sensorsUUID);

    // DATA TRANSACTION
    // sensor values (3xfloat32)
    float humidity, temperature, pressure;
    // get the i2c sensors data
    get_sensors_data(humidity, temperature, pressure);
    // display on the lcd (delay of 2.5 seconds)
    lcd_print_data(humidity, temperature, pressure);
    // write to the BLE characteristic
    write_to_BLE(sensorsCharacteristic, humidity, temperature, pressure);

    peripheral.disconnect();

    // peripheral disconnected, start scanning again
    BLE.scanForUuid(serviceUUID);
  }

  // offline mode, can't find the peripheral, just log to SD card.
  // then start scanning again
  else {
    // sensor values (3xfloat32)
    float humidity, temperature, pressure;
    // get the i2c sensors data
    get_sensors_data(humidity, temperature, pressure);
    // display on the lcd (delay of 2.5 seconds)
    lcd_print_data(humidity, temperature, pressure);

    lcd.clear();
    lcd.print("BLE Scanning");
  }

}


// include pointers (&) as values are used outside of the scope of the function
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
    lcd.clear();
    lcd.print("hyt939 i2c");
    lcd.setCursor(0, 1);
    lcd.print("error ...");
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
    lcd.clear();
    lcd.print("dps368 i2c");
    lcd.setCursor(0, 1);
    lcd.print("error ...");
    while (1);
  }
}

void lcd_print_data(float humidity, float temperature, float pressure) {
  lcd.clear();
  lcd.print("Rel Humidity");
  lcd.setCursor(0, 1);
  lcd.print(humidity);
  lcd.print(" %");
  delay(1000);

  lcd.clear();
  lcd.print("Temperature");
  lcd.setCursor(0, 1);
  lcd.print(temperature);
  lcd.print(" degC");
  delay(1000);

  lcd.clear();
  lcd.print("Pressure");
  lcd.setCursor(0, 1);
  lcd.print(pressure);
  lcd.print(" Pa");
  delay(500);
}

void write_to_BLE(BLECharacteristic sensorsCharacteristic, float humidity, float temperature, float pressure) {
  // combine the floats into one 12 byte character array
  byte sensors_data [23];
  byte tag [11] = deviceTag;
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
