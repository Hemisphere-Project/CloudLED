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


#define   MESH_PREFIX     "CloudLED"
#define   MESH_PASSWORD   "somethingSneaky!"

//// Disable once flashed (EEPROM stored)
// #define K32_SET_NODEID 1      // board unique id  
////

int lastRound = -1;
int lastTurn = -1;
int lastPosition = -1;

uint32_t lastMeshMillis = 0;
uint32_t meshMillisOffset = 0;

bool longPress = false;

enum State { MESH, WIFI, OTA };
State state = MESH;


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
void sendMacro() 
{
  if (pool->isMaster()) 
    mesh.sendBroadcast( "M="+String(activeMacroNumber()) );
}


Task userLoopTask1( TASK_MILLISECOND * 5000 , TASK_FOREVER, &sendInfo );
Task userLoopTask2( TASK_MILLISECOND * 500 , TASK_FOREVER, &sendMacro );

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

  // Receive macro from Master
  if (msg.startsWith("M=")) 
  {
    Serial.println("Received macro from master");
    int macro = msg.substring(2).toInt();
    setActiveMacro(macro);
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

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
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
  buttons->add(39, "PUSH");  

  k32->on("btn/PUSH-off", [](Orderz *order)
  { 
    if (state == MESH) {
      nextMacro();
      sendMacro(); 
    }
    else if (!longPress) 
      k32->system->reset();
    
    longPress = false;
  });

  k32->on("btn/PUSH-long", [](Orderz *order)
  { 
    longPress = true;
    activeMacro()->stop();
    if (state == MESH) {
      state = WIFI;
      // stop MESH
      mesh.stop();

      // start WIFI
      wifi = new K32_wifi(k32);
      wifi->setHostname("cloud-"+String(k32->system->id())+"-v"+String(CLOUD_VERSION));
      wifi->connect("hmsphr", "hemiproject");
    } 
  });
  
  // LOAD LIGHT
  lightSetup(k32);

  // START MESH
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, 5555, WIFI_AP_STA, 6, 1 );

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


  // CREATE ANIMATIONS
  addMacro(new Anim_cloud_crawler, 1500)->master(100);
  addMacro(new Anim_cloud_wind, 3000)->master(100);
  addMacro(new Anim_cloud_breath, 3000)->master(100);
  addMacro(new Anim_cloud_sparkle, 3000)->master(100);

  // addMacro(new Anim_cloud_noel);
  // addMacro(new Anim_cloud_stars);
  // addMacro(new Anim_cloud_rainbow);
  int master = 100;

  //{master , r  , g  , b  , w  ,pix mod , pix long , pix_pos , str_mod , str_speed , r_fond , g_fond , b_fond , w_fond , mirror_mod , zoom }
  //{0      , 1  , 2  , 3  , 4  ,5       , 6        , 7       , 8       , 9         , 10     , 11     , 12     , 13     , 14         , 15   } adr + -1

  // addMacro(new Anim_dmx_strip, 3000)
  //   ->push( master, 0, 255, 255, 255, 0, 1, 127, 150, 100, 0, 0, 0, 0, 0,  255)
  //   ->mod("wind", new K32_mod_pulse)->absolute()->mini(150)->maxi(151)->at(8)->period(100)->play(); // 0 : Wind in tree 
  
  // addMacro(new Anim_dmx_strip, 3000)->push( master, 255, 0, 255, 255, 0, 1, 127, 21, 3000, 0, 0, 0, 0, 0,  255); // 1 : Breath 

  // addMacro(new Anim_dmx_strip, 3000)->push( master, 255, 0, 255, 255, 0, 1, 127, 21, 3000, 0, 0, 0, 0, 0,  255); // 1 : Breath 

  k32->timer->every(1000, []() {
    // NOW
    uint32_t now = meshMillis();
    int duration = activeDuration();
    uint32_t roundDuration = duration * pool->count();
    int turn = (now % roundDuration) / duration; 
    int round = now / roundDuration;
    int time = now % duration;
    LOG("=== Round: "+ String(round)+ " // Position: " + String(pool->position())+ " / Turn: " + String(turn) + " // Time: " + String(time) + " // Duration: " + String(duration) + " // Macro: " + String(activeMacro()->name()) );
  });

  setActiveMacro();
}

void loop() 
{ 
  // ANIMATE
  if (state == MESH)  
  {
    mesh.update();
    uint32_t now = meshMillis();

    K32_anim* anim = activeMacro();

    if (anim) 
    {      
      // ROUND / TURN - DURATION
      int duration = activeDuration();
      uint32_t roundDuration = duration * pool->count();
      
      // ROUND & TURN - CALC
      int turn = (now % roundDuration) / duration; 
      int round = now / roundDuration;
      int time = now % duration;
      
      anim->push(duration, time, round, turn, pool->position(), pool->count() );
    }
  }

  else if (state == WIFI) 
  {
    uint32_t now = meshMillis();

    byte val = (now/15)%70 + 30;
    CRGBW color = CRGBW::DarkBlue;

    if (wifi && wifi->otaInProgress()) 
    {
      color = CRGBW::HotPink;
      delay(500);
    }
    else if (wifi && wifi->isConnected()) 
      color = CRGBW::Cyan;
    
    color %= val;
    strip->all( color );
  }

  // 


    // //
    // // ANIMATE
    // //
    // int colorSel = round % 7; 
    // int pixSel = time * LULU_STRIP_SIZE / (animDuration/4);

    // if (turn == pool->position()) 
    // {
    //   int fade = 255;
    //   // if (time < animDuration/2) fade = time * 255 / (animDuration/2);
    //   // else fade = 255 - (time - animDuration/2) * 255 / (animDuration/2);

    //   CRGBW color;

    //   if (colorSel == 0) color = CRGBW(fade, fade, fade);
    //   else if (colorSel == 1) color = CRGBW(fade, 0, 0);
    //   else if (colorSel == 2) color = CRGBW(0, fade, 0);
    //   else if (colorSel == 3) color = CRGBW(0, 0, fade);
    //   else if (colorSel == 4) color = CRGBW(fade, fade, 0);
    //   else if (colorSel == 5) color = CRGBW(fade, 0, fade);
    //   else if (colorSel == 6) color = CRGBW(0, fade, fade);
      
    //   if (pixSel < 3) strip->all( CRGBW{0,0,0} );
    //   if (pixSel > 30) strip->pix(pixSel-30, CRGBW{0,0,0});

    //   if (pixSel > LULU_STRIP_SIZE) {
    //     strip->all( color );
    //     if (time> 7000 && time< 7500) strip->all( CRGBW{0,0,0} );
    //   }
    //   else strip->pix(pixSel, color);
    // }
    // else 
    // {
    //   strip->all( CRGBW{0,0,0} );
    // }

    //
    //
    //




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
