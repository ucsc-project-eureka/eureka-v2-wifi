# eureka-v2-wifi
EUREKA hierarchical data collection rewritten to interface with the AIRWISE V2 board.
***
### Documentation included for each .ino file
```cpp
/*
Author: 
Date: 
Board in Arduino IDE: 

Purpose: 

Hardware:
    - Board:
    - Sensors Used:

*/ 
```
***
### Version Synopsis: v1
V1 was completed and tested in July 2026. Its purpose is to transition the UCSC Eureka project's esp8266 protocols over to esp32-wifi, interfacing with the UCSC Airwise project's sensor boards with some modifications for soil sensing. Range testing concluded this esp-now protocol was too short-range for large-scale sensor networks.
Reference below, the architecture of V1 using wifi via esp-now.

Sink State Machine:

![v1 Cluster Head State Machine](miscellaneous/v1Sink.png)

Cluster Head State Machine:

![v1 Sink State Machine](miscellaneous/v1ClusterHead.png)

Sensor Node State Machine:

![v1 Sensor Node State Machine](miscellaneous/v1SensorNode.png)
***
