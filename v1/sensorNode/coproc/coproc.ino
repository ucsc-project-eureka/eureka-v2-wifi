/*
Author: PaskKat
Date: 6/25/2026
Board in Arduino IDE: Arduino Zero (Native USB)

Purpose: Fetch and send sensor readings when prompted to the ESP32 radio.

Hardware:
    - Board: SAMD21 microprocessor, UCSC Airwise project's v2 board.
    - Sensors Used: BME680, INA3221, Adafruit Stemma Soil Sensor, M10s GPS (optional)

*/ 

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_seesaw.h>
#include <Adafruit_INA3221.h>
#include "Adafruit_BME680.h"
#include "SparkFun_u-blox_GNSS_Arduino_Library.h"

// Pinouts & Reference Values ---------------------------------------------------
#define DEBUG_PORT SerialUSB
#define ESP_PORT Serial1

#define ESP_BAUD 9600

SFE_UBLOX_GNSS myGNSS;
#define SOIL_I2C 0x42

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define ESP_PIN_TX 43
#define ESP_PIN_RX 44

#define SEALEVELPRESSURE_HPA (1013.25)

#define wirePort Wire               // I2C Bus port name.
Adafruit_BME680 bme(&wirePort);     // I2C
Adafruit_INA3221 ina3221;         // Power monitoring sensor.
Adafruit_seesaw ss;             // Soil sensor.

// Packet Defs ------------------------------------------------------------------
enum messageType : uint8_t {
  DISCOVERY = 1,
  JOIN_REQUEST,
  TDMA_SCHEDULE,
  SENSOR_DATA,
  AGGREGATE_DATA
};

struct sensorDataPacket_t {
  uint8_t type;
  float temperature;
  float humidity;
  uint16_t soilMoisture;
  unsigned long timestamp;
};

// Helpers ----------------------------------------------------------------------

void serialPrintBMEData(void){
  DEBUG_PORT.println(F("BME680 test"));

  DEBUG_PORT.print("Temperature = ");
  DEBUG_PORT.print(bme.temperature);
  DEBUG_PORT.println(" *C");

  DEBUG_PORT.print("Pressure = ");
  DEBUG_PORT.print(bme.pressure / 100.0);
  DEBUG_PORT.println(" hPa");

  DEBUG_PORT.print("Humidity = ");
  DEBUG_PORT.print(bme.humidity);
  DEBUG_PORT.println(" %");

  DEBUG_PORT.print("Gas = ");
  DEBUG_PORT.print(bme.gas_resistance / 1000.0);
  DEBUG_PORT.println(" KOhms");

  DEBUG_PORT.print("Approx. Altitude = ");
  DEBUG_PORT.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  DEBUG_PORT.println(" m");

  DEBUG_PORT.println();
  return;
}

void serialPrintINAData(void){
  DEBUG_PORT.println("\n--- Power Monitor Data ---");

  for (uint8_t i = 0; i < 3; i++) {
    float voltage_V = ina3221.getBusVoltage(i);
    float current_A = ina3221.getCurrentAmps(i);
    float current_mA = current_A * 1000.0;

    DEBUG_PORT.print("Channel ");
    DEBUG_PORT.print(i + 1); 
    DEBUG_PORT.print(": ");
    DEBUG_PORT.print(voltage_V, 3); 
    DEBUG_PORT.print(" V | ");
    DEBUG_PORT.print(current_mA, 2);
    DEBUG_PORT.println(" mA");
  }
}

void checkIna3221(void){
    PORT->Group[0].DIRSET.reg = PORT_PA17; 
    PORT->Group[0].OUTCLR.reg = PORT_PA17;

    DEBUG_PORT.begin(115200);

    DEBUG_PORT.println("INA3221 port found and initialized!");
    serialPrintINAData();
}

void checkBme680(void){
  if (!bme.begin()) {
    DEBUG_PORT.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }
  else{
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    if (! bme.performReading()) {
      DEBUG_PORT.println("Failed to perform reading :(");
      return;}
    else{
      serialPrintBMEData();
    }
  }
  return;
}

void checkSoilSensor(void){
  DEBUG_PORT.println("seesaw Soil Sensor check:");
    DEBUG_PORT.println(ss.getVersion());
    float tempC = ss.getTemp();
    uint16_t capread = ss.touchRead(0);
    DEBUG_PORT.print("Temperature: "); DEBUG_PORT.print(tempC); DEBUG_PORT.println("*C");
    DEBUG_PORT.print("Capacitive: "); DEBUG_PORT.println(capread);
  return;
}

// Globals -----------------------------------------------------------------------
int startTime;
int currentTime;
bool getDataFlag = false;

// MAIN --------------------------------------------------------------------------
void setup(){
  PORT->Group[0].DIRSET.reg = PORT_PA17;
  PORT->Group[0].OUTCLR.reg = PORT_PA17;
  ESP_PORT.begin(ESP_BAUD);
  while(!ESP_PORT);

  startTime = millis();
  Wire.begin();
  ina3221.begin(0x40,&Wire);
  bme.begin(0x77,&Wire);
  ss.begin(SOIL_I2C);
  return;
}

void loop(){
  currentTime = millis() - startTime;
  if (ESP_PORT.available()){
    String input = ESP_PORT.readStringUntil('\n');
    input.trim();
    if (input == "SENSOR_DATA"){
      getDataFlag = true;
    }
  }
  if (getDataFlag){
    sensorDataPacket_t myData;
    myData.type = SENSOR_DATA;
    ESP_PORT.println("SENSOR_DATA:");
    bme.performReading();
    myData.temperature = bme.temperature;
    myData.humidity = bme.humidity;
    
    // myData.soilMoisture = ss.touchRead(0); // Soil monitor not connected during testing, but if not hardcoded use this.
    // Soil moisture dummy value:
    myData.soilMoisture = 500;

    myData.timestamp = currentTime;
    ESP_PORT.println(myData.temperature);
    ESP_PORT.println(myData.humidity);
    ESP_PORT.println(myData.soilMoisture);
    ESP_PORT.println(myData.timestamp);
    getDataFlag = false;
  }
}