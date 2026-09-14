// Only runs on ESP32S3 Dev Module board for debug port access.
// onboard ESP32 sends message via wifi to another onboard esp32 every five seconds.
// You must enable CDC on boot in sketch settings.

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define WAIT_TIME 5000
#define SECOND_TIME 1000
#define DEBUG_PORT Serial
#define BUFFER_SIZE 1000


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

// MAIN --------------------------------------------------------------
int lastTime = 0;
int currentTime = 0;
int messageCount = 0;
// construct generic data.
  sensorDataPacket_t myData = {
    .type = SENSOR_DATA,
    .temperature = 10,
    .humidity = 20,
    .soilMoisture = 300,
    .timestamp = 0
  };

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Send OK" : "Send FAILED");
}

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

  esp_now_register_send_cb(onDataSent);

  // adding peer so that broadcast is sent
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  esp_now_add_peer(&peerInfo);

  DEBUG_PORT.println("ESP32 radio DEBUG_PORT ready");
}

void loop() {
  // ping an outside ESP32 with data packets. wait every five seconds.
  while (currentTime - lastTime < WAIT_TIME){
    currentTime = millis();
  }

  // execute sending task. broadcast for ease of reception.
  // send ptr to message.
  esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));

  // print check what you're sending
  DEBUG_PORT.println("Sent message:");
  char buffer[BUFFER_SIZE];
  snprintf(buffer, BUFFER_SIZE, "\
  Temperature: %f\
  Humidity: %f\
  Soil Moisture: %u\
  Time sent: %lu", myData.temperature, myData.humidity, myData.soilMoisture, myData.timestamp);
  DEBUG_PORT.println(buffer);

  // once they are equal, set to equal. then restart loop iteration.
  lastTime = currentTime;
}
