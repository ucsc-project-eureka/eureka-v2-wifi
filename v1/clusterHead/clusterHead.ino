
/*
NOTE: This should be compiled with the ESP32S3 Dev Module with CDC on Boot enabled.
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define MAX_SENSOR_NODES 3
#define TDMA_SLOT_TIME 1000
#define JOIN_REQUEST_TIMEOUT 3000
#define SENSOR_RESPONSE_TIMEOUT (TDMA_SLOT_TIME * MAX_SENSOR_NODES)
#define DEBUG_PORT Serial

uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t sinkMAC[6];
bool sinkMACKnown = false;

const unsigned long packetInterval = 3000;  // Time in milliseconds
unsigned long lastPacketSentTime = 0;

// Default C++ enum values are type int
enum messageType : uint8_t {
  DISCOVERY = 1,
  JOIN_REQUEST,
  TDMA_SCHEDULE,
  SENSOR_DATA,
  AGGREGATE_DATA
};

// Packet structures
struct discoveryPacket_t {
  uint8_t type;      // Packet type identifier
  uint8_t hopCount;  // Hop count away from the sink
  uint8_t roundCounter;
};

struct joinRequestPacket_t {
  uint8_t type;
};

struct tdmaSchedulePacket_t {
  uint8_t type;
  uint8_t macs[MAX_SENSOR_NODES][6];  // List of sensor node MACs
};

struct sensorDataPacket_t {
  uint8_t type;
  float temperature;
  float humidity;
  uint16_t soilMoisture;
  unsigned long timestamp;
};

struct aggregateDataPacket_t {
  uint8_t type;
  uint8_t macs[MAX_SENSOR_NODES][6];
  float temperatures[MAX_SENSOR_NODES];
  float humidities[MAX_SENSOR_NODES];
  uint16_t soilMoistures[MAX_SENSOR_NODES];
  unsigned long timestamps[MAX_SENSOR_NODES];
  uint8_t readingsCount;
};

// Global variables
unsigned long discoverySentTime = 0;
unsigned long scheduleSentTime = 0;

bool waitingForJoinRequests = false;
bool waitingForSensorData = false;

uint8_t sensorNodeMACs[MAX_SENSOR_NODES][6];
uint8_t sensorNodeCount = 0;

sensorDataPacket_t sensorData[MAX_SENSOR_NODES];
bool sensorDataReceived[MAX_SENSOR_NODES] = { false };
unsigned long joinCount = 0;
bool sentDiscovery = false;

aggregateDataPacket_t aggData;

unsigned long startTime;
unsigned long currentTime;
unsigned long sendTime;

esp_now_peer_info_t peerInfo;

// send a "give me data" ping. Assuming peer channel is the broadcast channel.
void sendDiscoveryPacket(discoveryPacket_t* sinkPkt){
  discoveryPacket_t recvPkt;
  memcpy(&recvPkt, sinkPkt, sizeof(discoveryPacket_t));
  discoveryPacket_t giveMeData;
      giveMeData.type = DISCOVERY;
      giveMeData.hopCount = recvPkt.hopCount + 1;
      giveMeData.roundCounter = recvPkt.roundCounter;
      sendTime = currentTime;
      esp_now_send(broadcastAddress,(uint8_t*)&giveMeData,sizeof(discoveryPacket_t));
      DEBUG_PORT.println("Sent GIVE DATA!");
      discoverySentTime = millis();
      waitingForJoinRequests = true;     
}

void handleDiscoveryPacket(const uint8_t* senderMAC, const discoveryPacket_t* packet){
  // Mark the clusterhead as assigned to that sender & register.
  if (!sinkMACKnown){
    memcpy(sinkMAC,senderMAC,6);
    sinkMACKnown = true;
    memcpy(peerInfo.peer_addr,sinkMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    DEBUG_PORT.println("Added sink as peer!");
  }
  if(!sentDiscovery && !waitingForJoinRequests && !waitingForSensorData){
    discoveryPacket_t sinkPkt;
    memcpy(&sinkPkt, packet, sizeof(discoveryPacket_t));
    sendDiscoveryPacket(&sinkPkt);
    sentDiscovery = true;
    DEBUG_PORT.println("Sent discovery packet!");
  }
  return;
}

void handleJoinRequest(const uint8_t* senderMAC) {

  // Check if MAC is already recorded
  for (uint8_t i = 0; i < sensorNodeCount; ++i) {
    if (memcmp(sensorNodeMACs[i], senderMAC, 6) == 0){
      DEBUG_PORT.println("Sensor node MAC address is already recorded");
      return;
      }
  }

  // Mark as joined.
  joinCount ++;
  // Store MAC address
  if (sensorNodeCount < MAX_SENSOR_NODES) {
    DEBUG_PORT.println("Storing sensor node MAC address");
    memcpy(sensorNodeMACs[sensorNodeCount], senderMAC, 6);
    // store for aggData packet as well.
    memcpy(aggData.macs[sensorNodeCount], senderMAC, 6);
    sensorNodeCount++;
  }
}

void handleSensorData(const uint8_t* mac, const uint8_t* incomingData, int len) {
  if (len != sizeof(sensorDataPacket_t)) {
    DEBUG_PORT.println("Invalid SENSOR_DATA packet size");
    return;
  }

  for (uint8_t i = 0; i < sensorNodeCount; i++) {
    if (memcmp(mac, sensorNodeMACs[i], 6) == 0) {
      memcpy(&sensorData[i], incomingData, sizeof(sensorDataPacket_t));
      sensorDataReceived[i] = true;

      // AND add this data to the agg_data pkt to be sent to the sink.
      DEBUG_PORT.printf("Stored SENSOR_DATA from node %d\n", i);
      // store all for aggData.
      aggData.temperatures[i] = sensorData[i].temperature;
      DEBUG_PORT.printf("  Temperature: %.2f °C\n", sensorData[i].temperature);

      aggData.humidities[i] = sensorData[i].humidity;
      DEBUG_PORT.printf("  Humidity: %.2f %%\n", sensorData[i].humidity);
      
      aggData.soilMoistures[i] = sensorData[i].soilMoisture;
      DEBUG_PORT.printf("  Soil Moisture: %u\n", sensorData[i].soilMoisture);

      aggData.timestamps[i] = sensorData[i].timestamp;
      DEBUG_PORT.println("Timestamp of packet recorded!");
      return;
    }
  }

  DEBUG_PORT.println("SENSOR_DATA received from unknown MAC.");
}

void sendTDMASchedule() {
  tdmaSchedulePacket_t tdmaSchedulePacket;
  tdmaSchedulePacket.type = TDMA_SCHEDULE;

  DEBUG_PORT.println("Preparing TDMA Schedule:");

  for (int i = 0; i < sensorNodeCount; i++) {
    memcpy(tdmaSchedulePacket.macs[i], sensorNodeMACs[i], 6);

    // Print each MAC with slot index
    DEBUG_PORT.printf("  Slot %d -> MAC: ", i);
    for (int j = 0; j < 6; j++) {
      DEBUG_PORT.printf("%02X", tdmaSchedulePacket.macs[i][j]);
      if (j < 5) DEBUG_PORT.print(":");
    }
    DEBUG_PORT.println();
  }

  esp_now_send(broadcastAddress, (uint8_t *)&tdmaSchedulePacket, sizeof(tdmaSchedulePacket));
  DEBUG_PORT.println("TDMA Schedule sent to all nodes.");

  // Prepare to receive data
  scheduleSentTime = millis();
  waitingForSensorData = true;

  // Reset sensor data received flags
  for (int i = 0; i < MAX_SENSOR_NODES; i++) {
    sensorDataReceived[i] = false;
  }
}

void sendAggregatePacket(){
  // send pkt back to sink.
  aggData.type = AGGREGATE_DATA;
  aggData.readingsCount = sensorNodeCount;
  esp_now_send(sinkMAC,(uint8_t*)&aggData,sizeof(aggregateDataPacket_t));
  DEBUG_PORT.println("Sent Aggregate Data packet!");
}

void readMacAddress(){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    DEBUG_PORT.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    DEBUG_PORT.println("Failed to read MAC address");
  }
}

void OnDataRecv(const esp_now_recv_info* recvInfo, const uint8_t* incomingData, int len) {
  // Determine type of packet received
  // get the source address.
  const uint8_t* senderMac = recvInfo->src_addr;
  uint8_t packetType = incomingData[0];

  switch (packetType) {
    case DISCOVERY:
      DEBUG_PORT.println("Recieved discovery packet!");
      handleDiscoveryPacket(senderMac, (const discoveryPacket_t*)incomingData);
      break;
    
    case JOIN_REQUEST:
      handleJoinRequest(senderMac);
      break;

    case SENSOR_DATA:
      handleSensorData(senderMac, incomingData, len);
      break;

    default:
      DEBUG_PORT.println("Unknown packet type received");
      break;
  }
}

// MAIN ---------------------------------------------------------------------------------

void setup(){
  DEBUG_PORT.begin(115200);
  while(!DEBUG_PORT);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  if(esp_now_init() != ESP_OK){
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  DEBUG_PORT.println("[DEFAULT] ESP32 Board MAC Address: ");
  readMacAddress();

  // adding peer so that broadcast is sent
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  DEBUG_PORT.println("Broadcasting address added as wifi esp-now peer");

  DEBUG_PORT.println("\nClusterhead ready.");
  startTime = millis();
}
 
void loop(){
  currentTime = millis();
  if (waitingForJoinRequests && (millis() - discoverySentTime > JOIN_REQUEST_TIMEOUT)) {
    DEBUG_PORT.println("Finished waiting for JOIN_REQUESTs.");
    waitingForJoinRequests = false;

    // Send TDMA schedule only if it had sensors join it.
    if (joinCount >= 1){
      sendTDMASchedule();
    }
    else{
      DEBUG_PORT.println("Didn't receive any join requests, stalling for 3 seconds.");
      delay(3000);
      sensorNodeCount = 0;
      joinCount = 0;
      sentDiscovery = false;
    }
  }
  if (waitingForSensorData && millis() - scheduleSentTime > SENSOR_RESPONSE_TIMEOUT) {
    DEBUG_PORT.println("Finished waiting for Sensor Data.");
    waitingForSensorData = false;
    // Once all sensors have reported data, send the agg data to the sink.
    sendAggregatePacket();
    // Reset all prior conditions, packages, and flags.
    sentDiscovery = false;
    sensorNodeCount = 0;
    joinCount = 0;
    memset(&aggData, 0, sizeof(aggData));
    memset(sensorDataReceived, 0, sizeof(sensorDataReceived));
  }
}