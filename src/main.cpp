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

int lastRound = -1;
int lastTurn = -1;
int lastPosition = -1;

uint32_t lastMeshMillis = 0;
uint32_t meshMillisOffset = 0;

// meshMillis (handle mesh µS overflow) -> will overflow after ~50 days
uint32_t meshMillis() 
{
  uint32_t meshMillis = mesh.getNodeTime()/1000 + meshMillisOffset;
  if (meshMillis < lastMeshMillis) {
    meshMillisOffset = lastMeshMillis;
    meshMillis = mesh.getNodeTime()/1000 + meshMillisOffset;
  }
  lastMeshMillis = meshMillis;
  return meshMillis;
}

// Send Info 
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
  // Serial.printf("-- Received from %u msg=%s\n", from, msg.c_str());

  // Receive channels list from Remote
  if (msg.startsWith("CL=")) 
  {
    // Parse remote pool list
    PeersPool* remotePool = new PeersPool(msg.substring(3), from);

    // I am missing from the list => inform remote
    if (remotePool->getChannel(pool->ownerID()) != pool->ownerChannel()) 
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
  // Serial.printf("Pool position: %d // size: %d\n", pool->position(), pool->size());
}

void changedConnectionCallback() 
{
  Serial.printf("Changed connections, node count = %d \n", mesh.getNodeList().size());
  pool->updatePeers(mesh.getNodeList());
  sendInfo();
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
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask( userLoopTask );
  userLoopTask.enable();

}

void loop() 
{ 

  mesh.update();

  // NOW
  uint32_t now = meshMillis();
  
  // ROUND / TURN - DURATION
  uint32_t animDuration = 1000;
  uint32_t roundDuration = animDuration * pool->count();
  
  // ROUND & TURN - CALC
  int turn = (now % roundDuration) / animDuration; 
  int round = now / roundDuration;

  // SITUATION CHANGED
  if (turn != lastTurn || round != lastRound || pool->position() != lastPosition)  
  {
    Serial.println("Round: "+ String(round)+ " // Turn: " + String(turn)+ " // My position: " + String(pool->position()) );
    lastRound = round;
    lastTurn = turn;
    lastPosition = pool->position();

    if (turn == pool->position()) 
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
