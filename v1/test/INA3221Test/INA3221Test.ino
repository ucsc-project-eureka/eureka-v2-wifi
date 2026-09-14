// INA3221 Standalone Diagnostic Test for SAMD21
#include <Wire.h>
#include <Adafruit_INA3221.h>

#define DEBUG_PORT SerialUSB

Adafruit_INA3221 ina3221;

void setup() {
  // 1. Enable peripheral power rail (PA17 HIGH)
  PORT->Group[0].DIRSET.reg = PORT_PA17;
  PORT->Group[0].OUTSET.reg = PORT_PA17;

  DEBUG_PORT.begin(115200);
  while (!DEBUG_PORT && millis() < 5000); // Wait up to 5s for Serial Monitor

  DEBUG_PORT.println("\n--- INA3221 (0x40) Diagnostic Test ---");

  // 2. Start I2C Wire Bus
  Wire.begin();
  delay(100);

  // 3. I2C Bus Check specifically for 0x40
  Wire.beginTransmission(0x40);
  byte error = Wire.endTransmission();

  if (error != 0) {
    DEBUG_PORT.print("ERROR: Device not responding at 0x40! (I2C Error code: ");
    DEBUG_PORT.print(error);
    DEBUG_PORT.println(")");
    DEBUG_PORT.println("Check SDA/SCL wiring or PA17 power setting.");
    while (1); // Stop execution
  }

  DEBUG_PORT.println("SUCCESS: Hardware responding at 0x40.");

  // 4. Initialize Adafruit Library at 0x40
  if (!ina3221.begin(0x40, &Wire)) {
    DEBUG_PORT.println("ERROR: Adafruit_INA3221 library failed to initialize!");
    while (1);
  }

  // Set averaging mode & default shunt resistances
  ina3221.setAveragingMode(INA3221_AVG_16_SAMPLES);
  for (uint8_t i = 0; i < 3; i++) {
    ina3221.setShuntResistance(i, 0.05); // Standard Adafruit breakout default (0.05 ohm)
  }

  DEBUG_PORT.println("INA3221 Initialized Successfully. Starting loop...\n");
}

void loop() {
  DEBUG_PORT.println("----------------------------------------");
  
  // Read all 3 channels
  for (uint8_t ch = 0; ch < 3; ch++) {
    float busVoltage = ina3221.getBusVoltage(ch);              // Volts
    float current_mA = ina3221.getCurrentAmps(ch) * 1000.0;     // Amps to mA

    DEBUG_PORT.print("Channel ");
    DEBUG_PORT.print(ch + 1); // Display as 1, 2, 3
    DEBUG_PORT.print(" | Voltage: ");
    DEBUG_PORT.print(busVoltage, 2);
    DEBUG_PORT.print(" V | Current: ");
    DEBUG_PORT.print(current_mA, 2);
    DEBUG_PORT.println(" mA");
  }

  delay(2000); // Sample every 2 seconds
}