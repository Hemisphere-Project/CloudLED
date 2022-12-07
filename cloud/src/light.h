#include <K32_light.h>
K32_light* light = nullptr;

#include <fixtures/K32_ledstrip.h>
K32_fixture* strip = NULL;

#define PIN_STRIP 22              // OLIMEX 5 / ATOM 27 / DevC 22
#define LULU_STRIP_SIZE 750        // 735 - 
#define LULU_STRIP_TYPE LED_WS2815_V1   //LED_WS2815_V1 // LED_WS2812B_V3
#define PUSH_PIN  21 // Olimex 34 /  Atom 39 / DevC 21

/// ANIMATIONS & MACRO
K32_anim* anims[16] = {NULL};
int durations[16] = {0};
int macro = 0;
int macroCount = 0;

void lightSetup(K32* k32) {
  light = new K32_light(k32);
  light->loadprefs();
  
  strip = new K32_ledstrip(0, PIN_STRIP, LULU_STRIP_TYPE, LULU_STRIP_SIZE);    
  light->addFixture( strip );

  // INIT TEST STRIPS
  light->anim( "test-strip", new Anim_test_strip, LULU_STRIP_SIZE )
      ->drawTo(strip)
      ->push(200)
      ->master(50)
      ->play();

  // WAIT END
  light->anim("test-strip")->wait();
}

K32_anim* addMacro(K32_anim* anim, int duration) {
  if (macroCount == 16) return NULL;
  light->anim( "cloud_"+String(macroCount), anim, LULU_STRIP_SIZE )
      ->drawTo(strip)
      ->master(255);
  durations[macroCount] = duration;
  anims[macroCount++] = anim;
  return anims[macroCount-1];
}

K32_anim* getMacro(int n) {
  if (n>=macroCount || n<0) return NULL;
  return anims[n];
}

int getDuration(int n) {
  if (n>=macroCount || n<0) return 0;
  return durations[n];
}

K32_anim* activeMacro() {
  return getMacro(macro);
}

int activeMacroNumber() {
  return macro;
}

int activeDuration() {
  return getDuration(macro);
}

void setActiveMacro(int n=-1) {
  if (n==-1) n = macro;
  if (n>=macroCount || n<0) return;
  macro = n;
  for (int i=0; i<macroCount; i++) {
    if (i==macro) light->anim("cloud_"+String(i))->play();
    else light->anim("cloud_"+String(i))->stop();
  }
  LOG("Macro: "+String(macro));
}

void nextMacro() {
  if (macroCount == 0) return;
  macro = (macro+1) % macroCount;
  setActiveMacro(macro);
}

