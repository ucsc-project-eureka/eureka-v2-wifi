// Note: Set this with an ESP32 s3 dev module.
// Code as an esp32 radio to esp32 radio test receiving protocol.
// You must enable CDC on boot in sketch settings.

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define DEBUG_PORT Serial
#define BUFFER_SIZE 1000
#define SECOND_TIME 1000

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t sinkMAC[6];
bool sinkMACKnown = false;

// typedefs ---------------------------------------------------------------
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

// Helpers -----------------------------------------------------------------

void onDataRecv(const esp_now_recv_info* recv_info, const uint8_t* incomingData, int len){
  uint8_t packetType = incomingData[0];
  if (packetType == SENSOR_DATA){
    DEBUG_PORT.println("Recieved data packet!");
    sensorDataPacket_t pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    float temp = pkt.temperature;
    float h = pkt.humidity;
    uint16_t s = pkt.soilMoisture;
    unsigned long time = pkt.timestamp;
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "\
    Temperature: %f\
    Humidity: %f\
    Soil Moisture: %u\
    Time sent: %lu", temp, h, s, time);
    DEBUG_PORT.println(buffer);
  }
}

// MAIN --------------------------------------------------------------------
int lastTime = 0;
int currentTime = 0;

void setup(){
  DEBUG_PORT.begin(9600);
  while(!DEBUG_PORT){;}
  DEBUG_PORT.println("DEBUG_PORT connected!");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  if(esp_now_init() != ESP_OK){
    DEBUG_PORT.println("ESP-NOW init failed!");
    return;
  }
  DEBUG_PORT.println("ESP32 radio DEBUG_PORT ready");
  esp_now_register_recv_cb(onDataRecv);
  DEBUG_PORT.println("Receive interrupt set!");
}

void loop() {
  // Tick on every second.
  while (currentTime - lastTime < SECOND_TIME){
    currentTime = millis();
  }
  // if it hits the 1-second mark:
  DEBUG_PORT.print(".");
  // once they are equal, set to equal. then restart loop iteration.
  lastTime = currentTime;
}
