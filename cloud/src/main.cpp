#include <Arduino.h>

#include <K32.h> // https://github.com/KomplexKapharnaum/K32-lib
K32* k32 = nullptr;

#include <K32_wifi.h>
K32_wifi* wifi = nullptr;

#include <hardware/K32_buttons.h>
K32_buttons* buttons = nullptr;

#include "light.h"
#include "anim_cloudled.h"
#include "peer.h"
PeersPool* pool;

#include "nowlink.h"

#define CLOUD_VERSION 6

const uint32_t AUTO_SHUTDOWN = 60*60*7;  // 7h

//// Disable once flashed (EEPROM stored)
// #define K32_SET_NODEID 1      // board unique id  
// #define HW_REVISION 1     // 0 = DevC - 1 = Atom - 2 = OLIMEX
////

uint32_t chipID = 0;
uint32_t switchWifiAt = 0;   
bool soloMode = true;
int longPress = 0;

enum State { MACRO, LOOP=253, WIFI=254, OFF=255 };
byte state = LOOP;
byte pushes = 1;





////////////////////////////////
////////   BEACON       ////////
////////////////////////////////


// Send Macro
void sendMode() 
{
  uint8_t lMode = (state >= LOOP) ? state : activeMacroNumber(); // local mode

  char msg[10] = {0};
  sprintf(msg, "M=%03d,%03d", lMode, pushes);
  nowBroadcast(String(msg));
}

void sendTime() 
{
  nowBroadcast( "T="+String(nowMillis()) );
}

////////////////////////////////
////////    WIFI OTA    ////////
////////////////////////////////

void switchToWifi() {
  light->anim("flash")->push(1, 1000, 100)->play()->wait();
  
  state = WIFI;
  pushes++;
  LOG("STATE: WIFI");
  sendMode();
  sendMode();
  delay(10);

  nowStop();

  // start WIFI
  Serial.println("Starting WIFI");
  wifi = new K32_wifi(k32);
  wifi->setHostname("cloud-"+String(k32->system->id())+"-v"+String(CLOUD_VERSION));
  wifi->connect("hmsphr", "hemiproject");
}

////////////////////////////////
////////    RECV        ////////
////////////////////////////////


// Needed for painless library
void receivedCallback( const uint8_t *mac, const uint8_t *data, int len ) 
{
  if (state == WIFI) return;  // We are toggling wifi, ignore mesh

  // Parse
  uint32_t from = mac[5] | mac[4] << 8 | mac[3] << 16 | mac[2] << 24;
  String msg = String((char*)data).substring(0, len);

  // Serial.printf("-- Received from %u msg: %s\n", from, msg.c_str());

  // Time sync
  if (msg.startsWith("T=")) 
  {
    uint32_t remoteTime = strtoull(msg.substring(2).c_str(), NULL, 10);
    nowAdjust(remoteTime);
  }

  // Mode sync
  if (msg.startsWith("M=")) 
  {
    uint8_t rMode = msg.substring(2,5).toInt(); //msg.substring(2,3);
    uint8_t lMode = (state >= LOOP) ? state : activeMacroNumber();
    uint8_t rState = (rMode >= LOOP) ? rMode : MACRO;
    uint8_t rPushes = msg.substring(6,9).toInt();

    // Serial.printf("Mode rcv: %d - %d\n", rMode, rPushes);

    if (rPushes > pushes || (rPushes == pushes && rMode > lMode)) {
      pushes = rPushes;
      if (rState != WIFI) state = rState;
      
      if (rState == MACRO) setActiveMacro(rMode);
      else if (rState == WIFI) switchWifiAt = millis()+100;

      Serial.printf("Mode sync: %d - %d\n", rMode, pushes);
    }
  }
}





////////////////////////////////
////////   SETUP        ////////
////////////////////////////////

void setup() 
{
  // chipID
  for(int i=0; i<17; i=i+8)
    chipID |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  

  pinMode(23, INPUT);
  digitalWrite(23, HIGH);

  k32 = new K32(CLOUD_VERSION);

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
  else if (k32->system->hw() == 2) buttons->add(34, "PUSH");   // Olimex

  // SHORT PRESS
  k32->on("btn/PUSH-off", [](Orderz *order)
  { 
    // Ignore PUSH-off after long press
    if (longPress) {
      longPress = 0;
      return;
    }

    // MACRO/LOOP -> MACRO
    if (state == MACRO || state == LOOP) 
    {
      if (state != MACRO) {
        state = MACRO;
        LOG("STATE: MACRO");
      }
      activeMacro()->stop();
      light->anim("flash")->push(1, 50, 100)->play()->wait();
      LOG("NEXT");
      nextMacro();
      pushes++;
      sendMode();
    }

    // WIFI -> RESET
    else if (state == WIFI) {
      // state = OFF;
      // pushes++;
      // LOG("STATE: OFF");
      // light->anim("off")->push(1)->play();
      k32->system->reset();
    }
    
  });

  // LONG PRESS
  k32->on("btn/PUSH-long", [](Orderz *order)
  { 
    longPress += 1;
    LOG("LONG PRESS " + String(longPress));

    // LONG PRESS
    if (longPress == 1) 
    {
      // ALREADY OFF -> BLINK
      if (state == OFF) {
        light->anim("flash")->push(1, 50, 100)->play()->wait();
      }

      // MACRO/LOOP -> LOOP
      else if (state == MACRO || state == LOOP) {
        activeMacro()->stop();
        light->anim("flash")->push(1, 1500, 100)->play()->wait();
        state = LOOP;
        pushes++;
        LOG("STATE: LOOP");
        sendMode();
      } 

    }

    // LONGER PRESS
    else if (longPress == 2) 
    {
      // OFF -> WIFI
      if (state == OFF || k32->system->channel() == 0 ) {
        switchWifiAt = millis()+100;
        light->anim("flash")->push(1, 50, 100)->play()->wait();
      }

      // WIFI -> RESTART 
      else if (state == WIFI) {
        k32->system->reset();
      }

      // MACRO/LOOP -> OFF
      else {
        state = OFF;
        pushes++;
        LOG("STATE: OFF");
        sendMode();
      }
    }

  });
  
  // LOAD LIGHT
  if (k32->system->hw() == 0) lightSetup(k32, 750, LED_WS2815_V1, 22);            // DevC
  else if (k32->system->hw() == 1) lightSetup(k32, 25, LED_WS2812B_V3, 27);       // Atom
  else if (k32->system->hw() == 2) lightSetup(k32, 25, LED_WS2812B_V3, 5);        // OLIMEX
  
  // POOL
  // pool = new PeersPool( chipID, k32->system->channel() );

  int master = 255;
  if(k32->system->hw() == 1) master = 50;


  // CREATE ANIMATIONS
  addMacro(new Anim_cloud_wind,    2000, 1)->master(master);
  addMacro(new Anim_cloud_crawler, 1500, 5)->master(master);
  addMacro(new Anim_cloud_sparkle, 100,  1)->master(master);
  addMacro(new Anim_cloud_breath,  6000, 1)->master(master);
  addMacro(new Anim_cloud_flash,   150,  5)->master(master);
  addMacro(new Anim_cloud_rainbow, 3000, 2)->master(master);
  addMacro(new Anim_cloud_sparkle, 6000, 1)->master(master);

  setActiveMacro();

  // Serial.printf("I am, ownerID = %lu %lu\n", pool->ownerID(), chipID);

  // Heap Memory log
  // k32->timer->every(1000, []() {
  //   static int lastheap = 0;
  //   int heap = ESP.getFreeHeap();
  //   LOGF2("Free memory: %d / %d\n", heap, heap - lastheap);
  //   lastheap = heap;
  //   if (heap < 50000) LOGF2("WARNING: free memory < 50K, new task might not properly start. %d / %d\n", heap, heap - lastheap);
  // });

  // nowINIT
  nowInit(receivedCallback);

  // send MODE
  k32->timer->every(1000, []() {
    sendMode();
    delay( random(10) );  // random delay to avoid collision
    sendTime();
    delay( random(10) );  // random delay to avoid collision
  });

  
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
    return;
  }

  // // INFO
  // if (macroChanged) {
  //   sendMode();
  //   macroChanged = false;
  // }

  // ANIMATE
  if (state == LOOP || state == MACRO)  
  {
    // // Alone after being linked => reset
    // if (notAlone != 1 && !pool->isSolo()) notAlone = 1;                // Not Alone
    // else if (pool->isSolo() && notAlone == 1) notAlone = millis();     // Became ISolated
    // else if (notAlone > 1 && millis()-notAlone > 60000)                // Isolatd for 60s   //k32->system->reset();
    // {
    //   notAlone = 0;
    //   LOG("MESH RESTART: isolated");
    // }

    // AUTO-OFF
    if (nowMillis() > AUTO_SHUTDOWN*1000) {
      state = OFF;
      pushes++;
      sendMode();
    }
    
    // Update
    uint32_t now = nowMillis();

    // IF LOOP MODE / force count/position
    // int count = (state == LOOP) ? 10 : pool->count();
    // int position = (state == LOOP) ? k32->system->channel()-1 : pool->position();
    int count = 10;
    int position = k32->system->channel()-1;

    updateMacro(now, position, count, state == LOOP, soloMode );    
  }

  else if (state == WIFI) 
  {
    byte val = (nowMillis()/12)%100 + 0;
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
    activeMacro()->stop();
    light->anim("off")->push(1)->play();
  }



}
