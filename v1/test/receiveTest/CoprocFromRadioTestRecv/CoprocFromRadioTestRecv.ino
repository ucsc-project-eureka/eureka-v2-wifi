// Coproc recieves message to the radio and repeats what it recieved to its debug serial.
// Works on the arduino zero (Native USB) board config.
/*
 * - Mode: TEXTMSG
 * - Baud rate: 38400
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
}

void loop() {
  while (ESP_PORT.available()) {
    String message = ESP_PORT.readStringUntil('\n');
    DEBUG_PORT.println("Recieved from radio: ");
    DEBUG_PORT.println(message);
  }
}
