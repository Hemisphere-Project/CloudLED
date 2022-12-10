#include <Arduino.h>

#include <K32.h> // https://github.com/KomplexKapharnaum/K32-lib
K32* k32 = nullptr;

#include <K32_wifi.h>
K32_wifi* wifi = nullptr;

#include <hardware/K32_buttons.h>
K32_buttons* buttons = nullptr;

#include "light.h"
#include "anim_cloudled.h"
// #include "anim_dmx_strip.h"

#include "peer.h"
PeersPool* pool;

#include "painlessMesh.h"
painlessMesh  mesh;
SimpleList<uint32_t> activesNodes;

Scheduler userScheduler; // to control your personal task

#define CLOUD_VERSION 1

#define   MESH_CHANNEL    6
#define   MESH_PREFIX     "CloudLED"
#define   MESH_PASSWORD   "somethingSneaky!"

//// Disable once flashed (EEPROM stored)
// #define K32_SET_NODEID 3      // board unique id  
// #define HW_REVISION 0     // 0 = DevC - 1 = Atom
////

uint32_t lastMeshMillis = 0;
uint32_t meshMillisOffset = 0;
uint32_t switchWifiAt = 0;

int longPress = 0;

int notAlone = 0;

enum State { MACRO, LOOP, WIFI, OFF };
State state = MACRO;



// meshMillis (handle mesh µS overflow) -> will overflow after ~50 days
uint32_t meshMillis() 
{
  uint32_t meshMillis = mesh.getNodeTime()/1000 + meshMillisOffset;
  // if (meshMillis < lastMeshMillis) { // ERROR when mesh sync clocks (might sync backward)
  //   meshMillisOffset = lastMeshMillis;
  //   meshMillis = mesh.getNodeTime()/1000 + meshMillisOffset;
  // }
  lastMeshMillis = meshMillis;
  return meshMillis;
}

////////////////////////////////
////////   INFO         ////////
////////////////////////////////

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

// Send Macro
void sendMacro(int forced = 0) 
{
  // Master situation => send macro
  if (pool->isMaster()) {
    if (state == MACRO) mesh.sendBroadcast( "M="+String(activeMacroNumber())+String(",")+String(macroTimeOffset) );
    else if (state == LOOP) mesh.sendBroadcast( "L="+String(activeMacroNumber())+String(",")+String(macroTimeOffset) );
    else if (state == OFF) mesh.sendBroadcast( "OFF" );
  }

  // Btn pressed (forced) => inform Master
  else if (forced && pool->masterID() > 0) {
    if (state == MACRO) mesh.sendSingle(pool->masterID(), "M="+String(activeMacroNumber())+String(",")+String(macroTimeOffset) );
    else if (state == LOOP) mesh.sendSingle(pool->masterID(), "L="+String(activeMacroNumber())+String(",")+String(macroTimeOffset) );
  }
}

void sendMacroAuto() { 
  sendMacro(); 
}

Task userLoopTask1( TASK_MILLISECOND * 9100 , TASK_FOREVER, &sendInfo );
Task userLoopTask2( TASK_MILLISECOND * 5000 , TASK_FOREVER, &sendMacroAuto );


////////////////////////////////
////////   MESH         ////////
////////////////////////////////


// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) 
{
  if (switchWifiAt > 1) return;  // We are toggling wifi, ignore mesh
  
  Serial.printf("-- Received from %u msg=%s\n", from, msg.c_str());

  // Receive channels list from Remote
  if (msg.startsWith("CL=")) 
  {
    // Parse remote pool list
    PeersPool* remotePool = new PeersPool(msg.substring(3), from);

    // I am missing from the list => inform remote
    if (remotePool->getChannel(pool->ownerID()) != pool->ownerChannel()) 
    {
      LOGF3("%d %d %lu == ", remotePool->getChannel(pool->ownerID()), pool->ownerChannel(), pool->ownerID());
      Serial.println("Remote list doesnt know me => sending my channel");
      mesh.sendSingle(from, "C="+String(k32->system->channel()));
    
      remotePool->addPeer(mesh.getNodeId(), k32->system->channel());
    }

    // If remote is indeed master, update my pool
    if (remotePool->isMaster()) {
      Serial.println("Remote is master, update my pool");
      pool->import(remotePool);
    }

    delete(remotePool);
  }

  // Receive individual channel
  else if (msg.startsWith("C=")) 
  {
    Serial.println("Received channel from remote");
    int channel = msg.substring(2).toInt();
    pool->addPeer(from, channel);

    if (channel < k32->system->channel()) {
      Serial.println("Remote channel is lower => He should know me so he takes the lead");
      mesh.sendSingle(from, "C="+String(k32->system->channel()));
    }
  }

  // Receive macro from Master
  else if (msg.startsWith("M=") && state != OFF) 
  {
    Serial.println("Received macro from master");
    msg = msg.substring(2);
    int pos = msg.indexOf(",");
    int macro = msg.substring(0, pos).toInt();
    int offset = msg.substring(pos+1).toInt();
    state = MACRO;
    setActiveMacro(meshMillis(), macro);
    macroTimeOffset = offset;
    sendMacro();
  }

  // Receive macro LOOP from Master
  else if (msg.startsWith("L=") && state != OFF) 
  {
    Serial.println("Received macro LOOP from master");
    msg = msg.substring(2);
    int pos = msg.indexOf(",");
    int macro = msg.substring(0, pos).toInt();
    int offset = msg.substring(pos+1).toInt();
    state = LOOP;
    setActiveMacro(meshMillis(), macro);
    macroTimeOffset = offset;
    sendMacro();
  }

  // Go into WIFI
  else if (msg.startsWith("WIFI")) 
  {
    Serial.println("Received WIFI");
    switchWifiAt = millis()+5000;
    activeMacro()->stop();
    light->anim("flash")->push(6, 50, 100)->play();
  }

  else if (msg.startsWith("OFF")) 
  {
    state = OFF;
    LOG("STATE: OFF");
  }

  // else 
  // Serial.printf("Pool position: %d // size: %d\n", pool->position(), pool->size());
}

void changedConnectionCallback() 
{
  pool->ownerID(mesh.getNodeId());
  // Serial.printf("I am, ownerID = %lu %lu\n", pool->ownerID(), mesh.getNodeId());
  Serial.printf("Changed connections, node count = %d \n", mesh.getNodeList().size());
  pool->updatePeers(mesh.getNodeList());
  sendInfo();
  sendMacro();
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void switchToWifi() {
  light->anim("flash")->push(1, 1000, 100)->play()->wait();
  
  state = WIFI;
  LOG("STATE: WIFI");

  // stop MESH
  mesh.stop();

  // start WIFI
  wifi = new K32_wifi(k32);
  wifi->setHostname("cloud-"+String(k32->system->id())+"-v"+String(CLOUD_VERSION));
  wifi->connect("hmsphr", "hemiproject");
}

////////////////////////////////
////////   SETUP        ////////
////////////////////////////////

void setup() 
{
  pinMode(23, INPUT);
  digitalWrite(23, HIGH);

  k32 = new K32();

  // SET ID
  #ifdef K32_SET_NODEID
    k32->system->id(K32_SET_NODEID);
    k32->system->channel(K32_SET_NODEID);
  #endif

  // SET HARDWARE
  #ifdef HW_REVISION
    k32->system->hw(HW_REVISION);
  #endif

  k32->system->channel(k32->system->id());
  Serial.println("Channel: " + String(k32->system->channel()));

  buttons = new K32_buttons(k32);
  if (k32->system->hw() == 0) buttons->add(21, "PUSH");        // DevC
  else if (k32->system->hw() == 1) buttons->add(39, "PUSH");   // Atom

  k32->on("btn/PUSH-off", [](Orderz *order)
  { 
    // Ignore PUSH-off after long press
    if (longPress) {
      longPress = 0;
      return;
    }

    if (state == MACRO || state == LOOP) 
    {
      if (state != MACRO) {
        state = MACRO;
        LOG("STATE: MACRO");
      }
      activeMacro()->stop();
      light->anim("flash")->push(1, 50, 100)->play()->wait();
      LOG("NEXT");
      nextMacro( meshMillis() );
      sendMacro(true); 
    }

    // -> OFF again
    else if (state == WIFI) {
      state = OFF;
      LOG("STATE: OFF");
      light->anim("off")->push(1)->play();
      // k32->system->reset();
    }
    
  });

  k32->on("btn/PUSH-long", [](Orderz *order)
  { 
    longPress += 1;

    if (longPress == 1) 
    {
      // -> BLINK
      if (state == OFF) {
        light->anim("flash")->push(1, 50, 100)->play()->wait();
      }

      // -> LOOP
      else if (state == MACRO || state == LOOP) {
        activeMacro()->stop();
        light->anim("flash")->push(1, 1500, 100)->play()->wait();
        state = LOOP;
        LOG("STATE: LOOP");
        nextMacro( meshMillis() );
        sendMacro(true);
      } 

    }

    
    else if (longPress == 2) 
    {
      // -> WIFI
      if (state == OFF) {
        if (wifi) {
          light->anim("flash")->push(1, 1000, 100)->play()->wait();
          state = WIFI;
          LOG("STATE: WIFI");
        }
        else {
          switchWifiAt = 1;
          mesh.sendBroadcast("WIFI", true);
        }
      }

      // -> RESTART 
      else if (state == WIFI) {
        k32->system->reset();
      }

      // -> OFF
      else {
        mesh.sendBroadcast("OFF", true);
      }
    }

  });
  
  // LOAD LIGHT
  if (k32->system->hw() == 0) lightSetup(k32, 750, LED_WS2815_V1, 22);            // DevC
  else if (k32->system->hw() == 1) lightSetup(k32, 25, LED_WS2812B_V3, 27);       // Atom

  // START MESH
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, 5555, WIFI_AP_STA, MESH_CHANNEL, 1 );

  // POOL
  pool = new PeersPool(mesh.getNodeId(), k32->system->channel());
  
  // SET MESH
  mesh.onReceive(&receivedCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask( userLoopTask1 );
  userScheduler.addTask( userLoopTask2 );
  userLoopTask1.enable();
  userLoopTask2.enable();

  int master = 255;


  // CREATE ANIMATIONS
  addMacro(new Anim_cloud_wind,    3000, 1)->master(master);
  addMacro(new Anim_cloud_sparkle, 100,  1)->master(master);
  addMacro(new Anim_cloud_breath,  6000, 1)->master(master);
  addMacro(new Anim_cloud_flash,   150,  5)->master(master);
  addMacro(new Anim_cloud_rainbow, 3000, 2)->master(master);
  addMacro(new Anim_cloud_sparkle, 100,  1)->master(master);
  addMacro(new Anim_cloud_crawler, 1000, 5)->master(master);
  addMacro(new Anim_cloud_sparkle, 6000, 1)->master(master);


  setActiveMacro( meshMillis() );

  // Serial.printf("I am, ownerID = %lu %lu\n", pool->ownerID(), mesh.getNodeId());

  // Heap Memory log
  // k32->timer->every(1000, []() {
  //   static int lastheap = 0;
  //   int heap = ESP.getFreeHeap();
  //   LOGF2("Free memory: %d / %d\n", heap, heap - lastheap);
  //   lastheap = heap;
  //   if (heap < 50000) LOGF2("WARNING: free memory < 50K, new task might not properly start. %d / %d\n", heap, heap - lastheap);
  // });
}


////////////////////////////////
////////   LOOP        /////////
////////////////////////////////

void loop() 
{ 
  // GO TO WIFI
  if (switchWifiAt > 0 && state != WIFI) 
  {
    if( switchWifiAt > 1 && millis() > switchWifiAt ) {
      switchWifiAt = 0;
      switchToWifi();
    }
    else mesh.update();
    return;
  }

  // INFO
  if (macroChanged) {
    sendMacro();
    macroChanged = false;
  }

  // ANIMATE
  if (state == MACRO || state == LOOP)  
  {
    // Alone after being linked => reset
    if (!pool->isSolo()) notAlone = 1;
    else if (notAlone) k32->system->reset();

    // Update
    mesh.update();
    uint32_t now = meshMillis();

    // LOGF2("%d %d\n", pool->position(), pool->count());
    updateMacro(now, pool->position(), pool->count(), state == LOOP);    
  }

  else if (state == WIFI) 
  {
    uint32_t now = meshMillis();

    byte val = (now/12)%100 + 0;
    CRGBW color = CRGBW::DarkBlue;

    if (wifi && wifi->otaInProgress()) 
    {
      color = CRGBW::HotPink;
      delay(500);
    }
    else if (wifi && wifi->isConnected()) 
      color = CRGBW::Cyan;
    
    color %= val;
    strip->clear();
    for (int i=0; i<5; i++) strip->pix(i, color);
  }
  
  else if (state == OFF)
  {
    mesh.update();
    activeMacro()->stop();
    light->anim("off")->push(1)->play();
  }



}
