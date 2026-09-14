// Coproc sends message to the radio to be broadcast every five seconds.
// Works with the Arduino Zero (Native USB) arduino board config.

/*
 * - Mode: TEXTMSG
 * - esp Baud rate: 9600
 * - RX GPIO: 44
 * - TX GPIO: 43
 */

#define DEBUG_PORT SerialUSB
#define ESP_PORT  Serial1

constexpr uint32_t DEBUG_BAUD = 115200;
constexpr uint32_t ESP_BAUD = 9600;
constexpr uint32_t SEND_PERIOD_MS = 5000;

constexpr size_t MESSAGE_BUFFER_SIZE = 96;

uint32_t packetCounter = 0;
uint32_t lastSendMs = 0;

void setup() {
  PORT->Group[0].DIRSET.reg = PORT_PA17;
  PORT->Group[0].OUTCLR.reg = PORT_PA17;

  DEBUG_PORT.begin(DEBUG_BAUD);
  uint32_t start = millis();
  while (!DEBUG_PORT && (millis() - start < 3000));

  DEBUG_PORT.println();
  DEBUG_PORT.print("Debug serial on, baud rate: ");
  DEBUG_PORT.println(DEBUG_BAUD);

  ESP_PORT.begin(ESP_BAUD);
  DEBUG_PORT.println();
  DEBUG_PORT.print("ESP UART on, baud rate: ");
  DEBUG_PORT.println(ESP_BAUD);

  // just in case
  delay(5000);

  DEBUG_PORT.println();
  DEBUG_PORT.print("Sending test message every ");
  DEBUG_PORT.print(SEND_PERIOD_MS);
  DEBUG_PORT.println(" ms.");
}

void loop(){
  uint32_t current = millis();

  if (current - lastSendMs >= SEND_PERIOD_MS){
    lastSendMs = current;
    packetCounter+=1;

    char message[MESSAGE_BUFFER_SIZE];

    // formats message 
    // snprintf(destination, maxSize, formatString, values)
    snprintf(
      message,
      sizeof(message),
      "COPROC_TEST packet=%lu",
      (unsigned long)packetCounter
    );

    ESP_PORT.println(message);
    DEBUG_PORT.print("Sent to ESP32 (Serial1): ");
    DEBUG_PORT.println(message);
  }
}
