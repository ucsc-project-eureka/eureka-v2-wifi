
/*
NOTE: This should be compiled with the ESP32S3 Dev Module with CDC on Boot enabled.
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define DATA_GET_INTERVAL 10000 // sending the "give me data" ping every 10 seconds.
#define MAX_SENSOR_NODES 3
#define DEBUG_PORT Serial
#define MAX_CLUSTERHEADS 1


// DEFS ---------------------------------------------------------------
uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t clusterHeadMACs[MAX_CLUSTERHEADS][6];
unsigned long clusterHeadCount = 0;

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

aggregateDataPacket_t aggregatePackets[MAX_CLUSTERHEADS];
bool sentDiscovery = false;

aggregateDataPacket_t aggData;
discoveryPacket_t discPkt;

unsigned long startTime;
unsigned long currentTime;
unsigned long sendTime;
unsigned long roundCount;

// Helpers ----------------------------------------------------------

void sendDiscoveryPacket(){
  discPkt.type = DISCOVERY;
  discPkt.hopCount = 0;
  discPkt.roundCounter = roundCount;

  esp_now_send(broadcastAddress, (uint8_t*)&discPkt, sizeof(discoveryPacket_t));
  sendTime = millis();
  sentDiscovery = true;
  DEBUG_PORT.println("Sent discovery packet!");
  return;
}

bool clusterHeadMACKnown(uint8_t* MAC){
  uint8_t testMAC[6];
  memcpy(testMAC, MAC, 6);
  bool flag = false;
  for (int i = 0; i<clusterHeadCount; i++){
    if (memcmp(testMAC, &(clusterHeadMACs[i]),sizeof(clusterHeadMACs[i])) == 0){
      flag = true;
    }
  }
  return flag;
}

// Unfinished - need to complete the second portion of reading out all the collected data.
void handleAggregatePacket(const uint8_t* CHMAC, aggregateDataPacket_t* aggPkt){
  uint8_t packetMAC[6];
  memcpy(packetMAC,CHMAC,6);
  if (!clusterHeadMACKnown(packetMAC)){
    memcpy(clusterHeadMACs[clusterHeadCount],CHMAC,6);
    // memcpy the agg packet into a larger array of all the collected data.
    memcpy(&aggregatePackets[clusterHeadCount], aggPkt, sizeof(aggregateDataPacket_t));
    clusterHeadCount++; 
    DEBUG_PORT.println("Added clusterhead into data set");
  }
  // print out the data recieved, copy the packet.
  DEBUG_PORT.println("Recieved Aggregate data packet from clusterhead!");

  for (int i = 0; i<clusterHeadCount; i++){
    char buff[1000];
    sprintf(buff, "Data collected for Clusterhead %d: ",i);
    DEBUG_PORT.println(buff);

    for (int j = 0; j<aggregatePackets[i].readingsCount;j++){
      DEBUG_PORT.println(j);
      DEBUG_PORT.println("Temperature: ");
      DEBUG_PORT.println(aggregatePackets[i].temperatures[j]);
      DEBUG_PORT.println("Humidity: ");
      DEBUG_PORT.println(aggregatePackets[i].humidities[j]);
      DEBUG_PORT.println("Soil Moisture: ");
      DEBUG_PORT.println(aggregatePackets[i].soilMoistures[j]);
      DEBUG_PORT.println("Time Stamp: ");
      DEBUG_PORT.println(aggregatePackets[i].timestamps[j]);
    }
  }
  return;
}

void onDataRecv(const esp_now_recv_info* recvInfo, const uint8_t* incomingData,int len){
  const uint8_t* senderMac = recvInfo->src_addr;
  uint8_t packetType = incomingData[0];

  if (packetType == AGGREGATE_DATA){
    handleAggregatePacket(senderMac, (aggregateDataPacket_t*) incomingData);
  }
  return;
}

// MAIN -------------------------------------------------------------

void setup() {
  DEBUG_PORT.begin(115200);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  esp_now_init();

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataRecv);

  DEBUG_PORT.println("ESP32 setup and broadcasting as sink");
  startTime = millis();
}

void loop() {
  currentTime = millis();
  if (currentTime - sendTime >= DATA_GET_INTERVAL){
    // Future: print out all aggData packets all at once. First, you need to define a timeout.
    // Once finished, reset all used conditions.
    clusterHeadCount = 0;
    memset(&aggregatePackets,0,sizeof(aggregatePackets));
    memset(&discPkt, 0, sizeof(discPkt));
    roundCount++;
    // sentDiscovery = false;
    sendDiscoveryPacket();
    sentDiscovery = true;
  }
}
