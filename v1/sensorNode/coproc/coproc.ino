// NoteL: Use an arduino zero (Native USB) config in arduino IDE for a working setup on the SAMD21.

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>

// MODS
#include <Adafruit_seesaw.h>
#include <Adafruit_INA3221.h>
#include "Adafruit_BME680.h"
#include "SparkFun_u-blox_GNSS_Arduino_Library.h" // for M10S GPS interfacing.

// Other setup pinouts --------------------------------------
// Use DEBUG_PORT for the Native Port
#define DEBUG_PORT SerialUSB
#define ESP_PORT Serial1

#define ESP_BAUD 9600

SFE_UBLOX_GNSS myGNSS;
#define SOIL_I2C 0x42

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

// From Airwise's ESP32 UART connections.
#define ESP_PIN_TX 43
#define ESP_PIN_RX 44

// Reference values for sensor data processing.
#define SEALEVELPRESSURE_HPA (1013.25)

// Reference all appropriate fields.
#define wirePort Wire               // I2C Bus port name.
Adafruit_BME680 bme(&wirePort);     // I2C
Adafruit_INA3221 ina3221;         // Power monitoring sensor.
Adafruit_seesaw ss;             // Soil sensor.

// Packet definitions
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

// N/A - checks with Debug_port statements are done in previous commits to this repo.

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
    // 1. Set PA17 to LOW immediately via direct register access
    // Ensure the pin is an output
    PORT->Group[0].DIRSET.reg = PORT_PA17; 
    // Clear the pin (Set to LOW)
    PORT->Group[0].OUTCLR.reg = PORT_PA17;

    DEBUG_PORT.begin(115200);

    DEBUG_PORT.println("INA3221 port found and initialized!");
    serialPrintINAData();
}

// Check power monitoring.
void checkBme680(void){
  // Check initialization of every sensor.
  if (!bme.begin()) {
    DEBUG_PORT.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }
  else{
    // Set up oversampling and filter initialization for BME
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms
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
    // Note: Capactative "touch" is the moisture level detected.
    uint16_t capread = ss.touchRead(0);
    
    DEBUG_PORT.print("Temperature: "); DEBUG_PORT.print(tempC); DEBUG_PORT.println("*C");
    DEBUG_PORT.print("Capacitive: "); DEBUG_PORT.println(capread);
  return;
}

// MAIN --------------------------------------------------------------------------

// Global variables: Mainly time keeping markers and flags.
int startTime;
int currentTime;
bool getDataFlag = false;

void setup(){
  // turn on the radio from the coproc
  PORT->Group[0].DIRSET.reg = PORT_PA17;
  PORT->Group[0].OUTCLR.reg = PORT_PA17;

  // DEBUG
  // DEBUG_PORT.begin(115200);
  // while(!DEBUG_PORT);

  // Get UART connecting coproc and esp32 online.
  ESP_PORT.begin(ESP_BAUD); // UART, coproc->esp32 and vice versa.
  while(!ESP_PORT);

  // initialize the sensors.
  startTime = millis();
  Wire.begin();
  ina3221.begin(0x40,&Wire);
  bme.begin(0x77,&Wire);
  ss.begin(SOIL_I2C);
  // give the sensors time to boot.
  // checkIna3221();
  // checkBme680();
  // checkSoilSensor();
  return;
}

void loop(){
  currentTime = millis() - startTime;
  if (ESP_PORT.available()){
    String input = ESP_PORT.readStringUntil('\n');
    input.trim();
    // NOTE: Assuming printed format for received "give data" message this way:
    if (input == "SENSOR_DATA"){
      getDataFlag = true;
    }
  }
  if (getDataFlag){
    sensorDataPacket_t myData;
    myData.type = SENSOR_DATA;

    // NOTE: this assumes parsing on the other side will pick up string data sent in this format.
    ESP_PORT.println("SENSOR_DATA:");

    // get latest bme data.
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