#include "painlessMesh.h"

#define   MESH_CHANNEL    10
#define   MESH_PREFIX     "CloudLED"
#define   MESH_PASSWORD   "somethingSneaky!"


// prototypes
void receivedCallback( uint32_t from, String &msg );
void changedConnectionCallback();

painlessMesh  mesh;

void setup() {
  Serial.begin(115200);
  
  // START MESH
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
  mesh.init( MESH_PREFIX, MESH_PASSWORD, NULL, 5555, WIFI_AP_STA, MESH_CHANNEL, 1 );

  mesh.onReceive(&receivedCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  Serial.println("Setup done ;)");
}

void loop() {
  mesh.update();
}

void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("bridge:  Received from %u msg=%s\n", from, msg.c_str());
}

void changedConnectionCallback() 
{
  Serial.printf("Changed connections, node count = %d \n", mesh.getNodeList().size());
}