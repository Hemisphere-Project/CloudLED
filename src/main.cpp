#include <Arduino.h>

#include <K32.h> // https://github.com/KomplexKapharnaum/K32-lib
K32* k32 = nullptr;

#include <hardware/K32_buttons.h>
K32_buttons* buttons = nullptr;

#include <K32_light.h>
K32_light* light = nullptr;

#include <fixtures/K32_ledstrip.h>
K32_fixture* strip = NULL;

#include "painlessMesh.h"
painlessMesh  mesh;
SimpleList<uint32_t> nodes;

Scheduler userScheduler; // to control your personal task

#define CLOUD_VERSION 1
#define PIN_STRIP 5
#define LULU_STRIP_SIZE 750

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky!"

//// Disable once flashed (EEPROM stored)
#define K32_SET_NODEID 3      // board unique id  
////

int position = 0;
int peerCount = 1;
uint32_t channels[16] = {0};

// CALCULATE MY POSITION
void calcPosition() {

  channels[k32->system->channel()] = mesh.getNodeId(); // make sure i am myself

  position = 0;
  for (int i = 0; i < k32->system->channel(); i++) 
    if (channels[i]) position++;

  peerCount = 0;
  for (int i = 0; i < 16; i++) 
    if (channels[i]) peerCount++;

  Serial.printf("Position: %d/%d\n", position, peerCount);
}

// USER LOOP TASK
void userLoop() 
{
  // Individual situation = send my channel
  if (peerCount == 1) {
    Serial.println("Solo... broadcast my channel !");
    mesh.sendBroadcast( "C="+String(k32->system->channel()) );
  }

  // Master situation = send channel list
  else if (position == 0) {
    String msg = "CL=";
    for (int i = 0; i < 16; i++) {
      msg += String(channels[i]);
      if (i < 15) msg += ",";
    }
    Serial.println("Master... broadcast channel list !");
    mesh.sendBroadcast( msg );
  }
  
}

Task userLoopTask( TASK_MILLISECOND * 1000 , TASK_FOREVER, &userLoop );

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) 
{
  // Receive channels list
  if (msg.startsWith("CL=")) 
  {
    // Channels list from Master
    uint32_t channelsMaster[16] = {0};
    String channelsStr = msg.substring(2);
    int i = 0;
    int pos = 0;
    while (i < channelsStr.length()) {
      int next = channelsStr.indexOf(",", i);
      if (next == -1) next = channelsStr.length();
      channels[pos] = channelsStr.substring(i, next).toInt();
      i = next + 1;
      pos++;
    }
    
    // I'm not in the list... re-broadcast my channel !
    if (channels[k32->system->channel()] == 0) {
      Serial.println("I'm not in the list... re-broadcast my channel !");
      mesh.sendBroadcast( "C="+String(k32->system->channel()) );
    }
    
    calcPosition();
  }

  // Receive individual channel
  if (msg.startsWith("C=")) {
    channels[msg.substring(2).toInt()] = from;
    calcPosition();
    if (position > 0) mesh.sendSingle(from, "C="+String(k32->system->channel()) );
  }


  // else 
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());

}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");

  nodes = mesh.getNodeList();

  SimpleList<uint32_t>::iterator node = nodes.begin();
  
  // remove channel if node is not connected anymore
  for (int i = 0; i < 16; i++) {
    if (channels[i]) {
      // search for node
      bool found = false;
      while (node != nodes.end()) {
        if (channels[i] == *node) {
          found = true;
          break;
        }
        node++;
      }
      if (!found) channels[i] = 0;
    }
  }
  calcPosition();


  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  node = nodes.begin();
  while (node != nodes.end()) {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();

}

void nodeTimeAdjustedCallback(int32_t offset) {
  // Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}


void setup() 
{
  k32 = new K32();

  // SET ID
  #ifdef K32_SET_NODEID
    k32->system->id(K32_SET_NODEID);
    k32->system->channel(K32_SET_NODEID);
  #endif

  k32->system->channel(k32->system->id());
  Serial.println("Channel: " + String(k32->system->channel()));

  buttons = new K32_buttons(k32);
  buttons->add(34, "BUT1");  

  light = new K32_light(k32);
  light->loadprefs();
  
  strip = new K32_ledstrip(0, PIN_STRIP, LED_WS2815_V1, LULU_STRIP_SIZE);    
  light->addFixture( strip );

  // INIT TEST STRIPS
  light->anim( "test-strip", new Anim_test_strip, LULU_STRIP_SIZE )
      ->drawTo(strip)
      ->push(300)
      ->master(100)
      ->play();

  // WAIT END
  light->anim("test-strip")->wait();

  // MESH
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  channels[k32->system->channel()] = mesh.getNodeId(); // my channel is active

  userScheduler.addTask( userLoopTask );
  userLoopTask.enable();

}

void loop() 
{ 

  mesh.update();

  int time = ( mesh.getNodeTime()/1000 ) % (1000*peerCount) ;

  if ( time > 1000*position && time < 1000*(position+1) ) {
    strip->all( CRGBW{255,255,255} );
  } else {
    strip->all( CRGBW{0,255,0} );
  }

  // int speed = 300;

  // strip->all( CRGBW{255,0,0} );
  // delay(speed);

  // strip->all( CRGBW{255,255,0} );
  // delay(speed);

  // strip->all( CRGBW{0,255,0} );
  // delay(speed);

  // strip->all( CRGBW{0,255,255} );
  // delay(speed);

  // strip->all( CRGBW{0,0,255} );
  // delay(speed);

  // strip->all( CRGBW{255,0,255} );
  // delay(speed);

  // strip->all( CRGBW{255,255,255} );
  // delay(speed);


}
