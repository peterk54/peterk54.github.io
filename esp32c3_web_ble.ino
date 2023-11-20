// esp32c3_web_ble.ino
// klin, 20.11.2023
// test web ble functionality
//
// web app -> web-ble-bme280.html
// works fine with chrome browser web ble app on android and linux (raspberry pi)
// ble device name ESP32-TH
// read bme280 temperature and humidity, control rgb led
// on xiao esp32c3:
// -> connect bme280 to default i2c pins sda-gpio6 and scl-gpio7
// -> connect rgb to gpios red-gpio4, green-gpio3, blue-gpio2
// bme280 sends temperature and humidity each second
// commands are: 
// off|0 - all leds off, ron|1 - red on, rof|2 - red off, gon|3 - green on, gof|4 - green off, bon|5 - blue on, bof|6 - blue off
// 
// see super tutorial by sara and ruis santos
// -> https://randomnerdtutorials.com/esp32-web-bluetooth

/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-bluetooth/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

// uncomment when using nimble library
//#define USE_NIMBLE

#ifdef USE_NIMBLE
#include <NimBLEDevice.h>
#else
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// bme280 i2c object 
Adafruit_BME280 bme;

// ble device name
const char *BLEDeviceName = "ESP32-TH";

// ble server and characteristics
BLEServer *pServer = NULL;
BLECharacteristic *pSensorCharacteristic = NULL;
BLECharacteristic *pLedCharacteristic = NULL;

// connection flags
bool deviceConnected = false;
bool oldDeviceConnected = false;

// rgb led pins
const int ledPinR = 4; 
const int ledPinG = 3;
const int ledPinB = 2;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID               "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SENSOR_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define LED_CHARACTERISTIC_UUID    "19b10002-e8f2-537e-4f6c-d104768a1214"

// connect and disconnect callbacks
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

// write callback handler - read command from web ble app and set leds

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pLedCharacteristic) {
    std::string value = pLedCharacteristic->getValue();
    if(value.length() > 0) {
      char *s = (char *) value.c_str();
      int v = (int) *s;
      Serial.printf("characteristic event, written: [%s] [%d]\n", s, v);
      if( !strncmp(s, "off", 3) || v == 0) {
        digitalWrite(ledPinR, LOW);
        digitalWrite(ledPinG, LOW);
        digitalWrite(ledPinB, LOW);
      }
      else if( !strncmp(s, "ron", 3) || v == 1) {
        digitalWrite(ledPinR, HIGH);
      } 
      else if( !strncmp(s, "rof", 3) || v == 2) {
        digitalWrite(ledPinR, LOW);
      }
      else if( !strncmp(s, "gon", 3) || v == 3) {
        digitalWrite(ledPinG, HIGH);
      }
      else if( !strncmp(s, "gof", 3) || v == 4) {
        digitalWrite(ledPinG, LOW);
      }
      else if( !strncmp(s, "bon", 3) || v == 5) {
        digitalWrite(ledPinB, HIGH);
      }
      else if( !strncmp(s, "bof", 3) || v == 6) {
        digitalWrite(ledPinB, LOW);
      }            
    }
  }
};

// setup
void setup() {
  Serial.begin(115200);

  // create bme280 object
  if( !bme.begin()) {
    Serial.printf("no bme280 sensor found!\n");
    while(1);
  }

  // init rgb led pins
  pinMode(ledPinR, OUTPUT);
  pinMode(ledPinG, OUTPUT);
  pinMode(ledPinB, OUTPUT);
  
  // Create the BLE Device
  BLEDevice::init(BLEDeviceName);

  // create ble server and attach server callbacks
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // create ble service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // create ble sensor characteristic
  pSensorCharacteristic = pService->createCharacteristic(
    SENSOR_CHARACTERISTIC_UUID,
#ifdef USE_NIMBLE
    NIMBLE_PROPERTY::READ   |
    NIMBLE_PROPERTY::WRITE  |
    NIMBLE_PROPERTY::NOTIFY |
    NIMBLE_PROPERTY::INDICATE
#else
    BLECharacteristic::PROPERTY_READ   |
    BLECharacteristic::PROPERTY_WRITE  |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE
#endif    
  );

  // rgb buttons characteristic
  pLedCharacteristic = pService->createCharacteristic(
    LED_CHARACTERISTIC_UUID,
#ifdef USE_NIMBLE
    NIMBLE_PROPERTY::WRITE
#else    
    BLECharacteristic::PROPERTY_WRITE
#endif    
  );

  // register the callback for the rgb buttons characteristic
  pLedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

#ifndef USE_NIMBLE
  // create a ble descriptor
  // created automaticelly by nimble!
  pSensorCharacteristic->addDescriptor(new BLE2902());
  pLedCharacteristic->addDescriptor(new BLE2902());
#endif

  // start the service
  pService->start();

  // start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("waiting a client connection to notify...");
}

// loop
void loop() 
{
  // read bme280 temperature and humidity values and set/notify them
  if(deviceConnected) {
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    char s[64];
    sprintf(s, "%.2f °C - %.2f %%", t, h);
    pSensorCharacteristic->setValue(s);
    pSensorCharacteristic->notify();
    Serial.printf("temperature / humidity notified: [%s]\n", s);
    delay(1000);
  }
  
  // disconnecting - restart advertising
  if( !deviceConnected && oldDeviceConnected) {
    Serial.println("device disconnected.");
    delay(500);
    pServer->startAdvertising(); 
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  
  // connecting
  if(deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    Serial.println("device connected");
  }
}
