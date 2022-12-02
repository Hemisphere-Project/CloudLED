#include "painlessMesh.h"

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define   STATION_SSID     "interweb"
#define   STATION_PASSWORD "superspeed37"
#define   STATION_PORT     5555
uint8_t   station_ip[4] =  {192,168,1,44}; // IP of the server

// prototypes
void receivedCallback( uint32_t from, String &msg );

painlessMesh  mesh;

void setup() {
  Serial.begin(115200);
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | DEBUG );
  mesh.setDebugMsgTypes( ERROR | DEBUG | STARTUP | CONNECTION );  // set before init() so that you can see startup messages


  // Channel set to 6. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6 );
  // Setup over the air update support
  mesh.initOTAReceive("bridge");

  mesh.stationManual(STATION_SSID, STATION_PASSWORD, STATION_PORT, station_ip);
  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);


  mesh.onReceive(&receivedCallback);
  Serial.println("Setup done ;)");
}

void loop() {
  mesh.update();
}

void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("bridge:  Received from %u msg=%s\n", from, msg.c_str());
}
