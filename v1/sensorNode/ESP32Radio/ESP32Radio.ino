
/*
Author: PaskKat
Date: 6/25/2026
Board in Arduino IDE: ESP32 S3 Dev Module

Purpose: Fetch sensor readings form the coproc when prompted.
          Receive TDMA scheduling and respond accordingly to the clusterhead.

Hardware:
    - Board: Heltec v3 ESP32 microcontroller on the UCSC Airwise project's v2 board
    - Sensors Used: N/A, this is cthe code for the radio microcontroller only.

*/ 

#include <Wire.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>

#define MAX_SENSOR_NODES 3
#define TDMA_SLOT_TIME 1000

#define DEBUG_PORT Serial
#define COPROC_PORT Serial1

// Pinouts -------------------------------------------------------------------
#define ESP_BAUD 9600
#define ESP_PIN_TX 43
#define ESP_PIN_RX 44

// Globals -------------------------------------------------------------------
unsigned long scheduledSlotTime = 0;

bool hasJoined = false;
bool clusterheadMACKnown = false;
bool scheduleReceived = false;
bool sentPacket = false;
bool sentJoinRequest = false;
int lastRoundCounter = 0;

uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t sinkMAC[6];
bool sinkMACKnown = false;

uint8_t clusterheadMAC[6];
uint8_t mySlotIndex = 255;

// Packet Defs ----------------------------------------------------------------

enum messageType : uint8_t {
  DISCOVERY = 1,
  JOIN_REQUEST,
  TDMA_SCHEDULE,
  SENSOR_DATA,
  AGGREGATE_DATA
};

struct discoveryPacket_t {
  uint8_t type;
  uint8_t hopCount;
  uint8_t roundCounter;
};

struct joinRequestPacket_t {
  uint8_t type;
};

struct tdmaSchedulePacket_t {
  uint8_t type;
  uint8_t macs[MAX_SENSOR_NODES][6];
};

struct sensorDataPacket_t {
  uint8_t type;
  float temperature;
  float humidity;
  uint16_t soilMoisture;
  unsigned long timestamp;
};

// Helpers -----------------------------------------------------------------

void sendJoinRequest() {
  if (hasJoined||sentJoinRequest) return;
  joinRequestPacket_t joinPacket = { JOIN_REQUEST };
  esp_now_send(clusterheadMAC, (uint8_t *)&joinPacket, sizeof(joinPacket));
  DEBUG_PORT.println("Sent join request!");
  sentJoinRequest = true;
}

void handleDiscoveryPacket(const uint8_t *senderMAC, const discoveryPacket_t *packet){
  DEBUG_PORT.println("Discovery packet received!");
  if (packet->hopCount == 0) return;

  if (!clusterheadMACKnown) {
    memcpy(clusterheadMAC, senderMAC, 6);
    clusterheadMACKnown = true;
    esp_now_peer_info_t  peerInfo = {};
    memcpy(peerInfo.peer_addr, clusterheadMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    DEBUG_PORT.println("Added clusterhead as a peer!");
  }
  if (packet->roundCounter != lastRoundCounter) {
    lastRoundCounter = packet->roundCounter;
    hasJoined = false;
    sentJoinRequest = false;
    scheduleReceived = false;
    sentPacket = false;
  }
  else{DEBUG_PORT.println("Clusterhead was already added.");}
  if (!hasJoined) {
    sendJoinRequest();
  }
}

void handleSchedulePacket(const uint8_t *senderMAC, const tdmaSchedulePacket_t *packet) {
  scheduleReceived = true;
  DEBUG_PORT.println("Received schedule packet!");
  uint8_t myMAC[6];
  WiFi.macAddress(myMAC);
  for (int i = 0; i < MAX_SENSOR_NODES; i++) {
    if (memcmp(packet->macs[i], myMAC, 6) == 0) {
      mySlotIndex = i;
      hasJoined = true;
      scheduleReceived = true;
      scheduledSlotTime = millis() + (mySlotIndex * TDMA_SLOT_TIME);
      return;
    }
  }
  hasJoined = false;
  scheduleReceived = false;
  mySlotIndex = 255;
}

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  const uint8_t *senderMac = recv_info->src_addr;
  uint8_t packetType = incomingData[0];
  switch (packetType) {
    case DISCOVERY:
      if (len >= sizeof(discoveryPacket_t))
        handleDiscoveryPacket(senderMac, (const discoveryPacket_t *)incomingData);
      break;
    case TDMA_SCHEDULE:
      if (len >= sizeof(tdmaSchedulePacket_t))
        handleSchedulePacket(senderMac, (const tdmaSchedulePacket_t *)incomingData);
      break;
    default:
      break;
  }
}

// MAIN --------------------------------------------------------------------

void setup(){
  DEBUG_PORT.begin(115200);
  while(!DEBUG_PORT);
  COPROC_PORT.begin(ESP_BAUD, SERIAL_8N1, ESP_PIN_RX, ESP_PIN_TX);
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    return;
  }
  esp_now_peer_info_t broadcastInfo = {};
  memcpy(broadcastInfo.peer_addr, broadcastAddress, 6);
  broadcastInfo.channel = 0;
  esp_now_add_peer(&broadcastInfo);
  DEBUG_PORT.println("Broadcasting address added as wifi esp-now peer");

  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  if (scheduleReceived && hasJoined && millis() >= scheduledSlotTime){
    COPROC_PORT.println("SENSOR_DATA");
    while(!(COPROC_PORT.available()));
    sensorDataPacket_t dataPacket;
    if (COPROC_PORT.available() && !sentPacket){
      String header = COPROC_PORT.readStringUntil('\n');
      header.trim();
      if (header == "SENSOR_DATA:") {
        dataPacket.type         = SENSOR_DATA;
        dataPacket.temperature  = COPROC_PORT.readStringUntil('\n').toFloat();
        dataPacket.humidity     = COPROC_PORT.readStringUntil('\n').toFloat();
        dataPacket.soilMoisture = COPROC_PORT.readStringUntil('\n').toInt();
        dataPacket.timestamp    = COPROC_PORT.readStringUntil('\n').toInt();
            
        DEBUG_PORT.println("Received data from coproc!");
        DEBUG_PORT.println("");
        }
        else{DEBUG_PORT.println("get Data called, SENSOR_DATA was not found");}
        }
      int timeout = millis() + 2000;
      if (millis() < timeout){
        DEBUG_PORT.println("Sending this data packet:");
        char buff[3000];
        snprintf(buff, sizeof(buff),
        "COPROC Temperature: %f\n"
        "COPROC humidity: %f\n"
        "COPROC soilMoisture: %u\n"
        "COPROC timestamp: %lu\n\n", 
        dataPacket.temperature, 
        dataPacket.humidity, 
        dataPacket.soilMoisture,
        dataPacket.timestamp);
        DEBUG_PORT.print(buff);
        
        esp_now_send(clusterheadMAC, (uint8_t *)&dataPacket, sizeof(dataPacket));
        DEBUG_PORT.println("Sent Data packet!");
        sentPacket = true;
    }
    scheduleReceived = false;
  }
}