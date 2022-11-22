#include <Arduino.h>

#include <K32.h> // https://github.com/KomplexKapharnaum/K32-lib
K32* k32 = nullptr;

#include <hardware/K32_buttons.h>
K32_buttons* buttons = nullptr;

#include <K32_light.h>
K32_light* light = nullptr;

#include <fixtures/K32_ledstrip.h>
K32_fixture* strip = NULL;

#include "peer.h"
PeersPool* pool;

#include "painlessMesh.h"
painlessMesh  mesh;
SimpleList<uint32_t> activesNodes;

Scheduler userScheduler; // to control your personal task

#define CLOUD_VERSION 1
#define PIN_STRIP 5
#define LULU_STRIP_SIZE 750

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky!"

//// Disable once flashed (EEPROM stored)
// #define K32_SET_NODEID 4      // board unique id  
////

int lastPhase = -1;
int lastPosition = -1;


// USER LOOP TASK 
void sendInfo() 
{
  // I don't know others => send my channel
  //
  if (pool->isSolo()) 
  {
    Serial.println("Solo... broadcast my channel !");
    mesh.sendBroadcast( "C="+String(k32->system->channel()) );
  }

  // Master situation => send channel list periodically
  //
  if (pool->isMaster()) 
  {
    Serial.println("Master... broadcast channel list !");
    mesh.sendBroadcast( "CL="+pool->toString() );
  }
}
Task userLoopTask( TASK_MILLISECOND * 5000 , TASK_FOREVER, &sendInfo );

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) 
{
  Serial.printf("-- Received from %u msg=%s\n", from, msg.c_str());

  // Receive channels list from Remote
  if (msg.startsWith("CL=")) 
  {
    // Parse remote pool list
    PeersPool* remotePool = new PeersPool(msg.substring(3), from);

    // I am missing from the list => inform remote
    if (remotePool->getChannel(mesh.getNodeId()) == -1) 
    {
      Serial.println("Remote list doesnt know me => sending my channel");
      mesh.sendSingle(from, "C="+String(k32->system->channel()));
    
      remotePool->addPeer(mesh.getNodeId(), k32->system->channel());
    }

    // If remote is indeed master, update my pool
    if (remotePool->isMaster()) {
      Serial.println("Remote is master, update my pool");
      pool->import(remotePool);
    }
  }

  // Receive individual channel
  if (msg.startsWith("C=")) 
  {
    Serial.println("Received channel from remote");
    int channel = msg.substring(2).toInt();
    pool->addPeer(from, channel);

    if (channel < k32->system->channel()) {
      Serial.println("Remote channel is lower => He should know me so he takes the lead");
      mesh.sendSingle(from, "C="+String(k32->system->channel()));
    }
  }

  // else 
  Serial.printf("Pool position: %d // size: %d\n", pool->position(), pool->size());
}

void newConnectionCallback(uint32_t nodeId) {
    // Serial.printf("--> New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() 
{
  Serial.printf("Changed connections, node count = %d \n", mesh.getNodeList().size());

  pool->updatePeers(mesh.getNodeList());
  sendInfo();
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



  // START MESH
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler );

  // POOL
  pool = new PeersPool(mesh.getNodeId(), k32->system->channel());
  
  // SET MESH
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask( userLoopTask );
  userLoopTask.enable();

}

void loop() 
{ 

  mesh.update();

  int time = (mesh.getNodeTime()/1000) % (1000*pool->count()) ;
  int phase = time / 1000;

  if (phase != lastPhase || pool->position() != lastPosition)  
  {
    Serial.println("Phase: " + String(phase)+ " // Position: " + String(pool->position()) );
    lastPhase = phase;
    lastPosition = pool->position();

    if (phase == pool->position()) 
    {
      Serial.println("My turn !");
      strip->all( CRGBW{255,255,255} );
    }
    else 
    {
      Serial.println("Not my turn");
      strip->all( CRGBW{0,255,0} );
    }
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
