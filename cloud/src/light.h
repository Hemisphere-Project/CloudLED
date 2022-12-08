#include <K32_light.h>
K32_light* light = nullptr;

#include <fixtures/K32_ledstrip.h>
K32_fixture* strip = NULL;

/// ANIMATIONS & MACRO
int stripSIZE = 0;
K32_anim* anims[16] = {NULL};
int durations[16] = {0};
int loopLoop[16] = {0};
int loopCount[16] = {0};
int macro = 0;
int macroCount = 0;
int lastRound = -1;

void lightSetup(K32* k32, int stripSize, int stripType, int stripPin) {
  light = new K32_light(k32);
  light->loadprefs();
  
  stripSIZE = stripSize;
  strip = new K32_ledstrip(0, stripPin, stripType, stripSIZE);    
  light->addFixture( strip );

  // INIT TEST STRIPS
  light->anim( "test", new Anim_test_strip, 10 )
      ->drawTo(strip)
      ->push(300)
      ->play()
      ->wait();

  // INIT TEST STRIPS
  light->anim( "flash", new Anim_flash, 10 )
      ->drawTo(strip)
      ->push(1, 100)
      ->play()
      ->wait();


}

K32_anim* addMacro(K32_anim* anim, int duration, int loops=1) {
  if (macroCount == 16) return NULL;
  light->anim( "cloud_"+String(macroCount), anim, stripSIZE )
      ->drawTo(strip)
      ->master(255);
  durations[macroCount] = duration;
  loopLoop[macroCount] = loops;
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

int activeLoopCount() {
  return loopCount[macro];
}

void setActiveMacro(int n=-1) {
  if (macro == n) return;
  if (n==-1) n = macro;
  if (n>=macroCount || n<0) return;
  macro = n;
  for (int i=0; i<macroCount; i++) {
    if (i==macro) light->anim("cloud_"+String(i))->play();
    else light->anim("cloud_"+String(i))->stop();
  }
  lastRound = -1;
  LOG("Macro: "+String(macro));
}

void nextMacro() {
  if (macroCount == 0) return;
  setActiveMacro((macro+1) % macroCount);
}

void updateMacro(uint32_t now, int position, int peers, int autoNext=0)
{
  // AUTO-NEXT 
  if (autoNext && loopCount[macro] >= loopLoop[macro]) nextMacro();

  // ROUND / TURN - DURATION
  int duration = activeDuration();
  uint32_t roundDuration = duration * peers;
  
  // ROUND & TURN - CALC
  int turn = (now % roundDuration) / duration; 
  int round = now / roundDuration;
  int time = now % duration;

  // ANIM LOOP COUNT
  if (lastRound == -1) {
    lastRound = round;
    loopCount[macro] = 0;
  }
  if (lastRound != round) {
    loopCount[macro] += 1;
    lastRound = round;
    // LOG("Macro: "+String(macro)+" - Loop: "+String(loopCount[macro]));
  }

  K32_anim* anim = activeMacro();
  if (anim) 
    anim->push(duration, time, round, turn, position, peers );
}

