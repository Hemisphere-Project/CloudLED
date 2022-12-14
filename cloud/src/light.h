#include <K32_light.h>
K32_light* light = nullptr;

#include <fixtures/K32_ledstrip.h>
K32_fixture* strip = NULL;

/// ANIMATIONS & MACRO
int stripSIZE = 0;
K32_anim* anims[16] = {NULL};
int durations[16] = {0};
int loopLoop[16] = {0};
int macro = 0;
int macroCount = 0;
uint32_t macroTimeOffset = 0;
int macroChanged = false;

void lightSetup(K32* k32, int stripSize, int stripType, int stripPin) {
  light = new K32_light(k32);
  light->loadprefs();
  
  stripSIZE = stripSize;
  strip = new K32_ledstrip(0, stripPin, stripType, stripSIZE);    
  light->addFixture( strip );

  // INIT TEST STRIPS
  // light->anim( "test", new Anim_test_strip, 10 )
  //     ->drawTo(strip)
  //     ->push(300)
  //     ->play()
  //     ->wait();

  // INIT TEST STRIPS
  light->anim( "flash", new Anim_flash, stripSIZE )
      ->drawTo(strip)
      ->push(1, 50)
      ->play()
      ->wait();

  // OFF ANIM
  light->anim( "off", new Anim_off, stripSIZE )
      ->drawTo(strip);

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

void setActiveMacro(uint32_t now, int n=-1) {
  if (macro == n) return;
  if (n==-1) n = macro;
  if (n>=macroCount || n<0) return;
  macro = n;
  for (int i=0; i<macroCount; i++) {
    if (i==macro) light->anim("cloud_"+String(i))->play();
    else light->anim("cloud_"+String(i))->stop();
  }
  macroTimeOffset = now;
  LOG("Macro: "+String(macro));
  macroChanged = true;
}

void nextMacro(uint32_t now) {
  if (macroCount == 0) return;
  setActiveMacro(now, (macro+1) % macroCount);
}

void updateMacro(uint32_t now, int position, int peers, int autoNext=0, int solo=0)
{
  uint32_t animNow = now - macroTimeOffset;

  // ROUND / TURN - DURATION
  int duration = activeDuration();
  uint32_t roundDuration = duration * peers;

  // ROUND & TURN - CALC
  int turn = (animNow % roundDuration) / duration; 
  int round = animNow / roundDuration;
  int time = animNow % duration;

  //   LOG("=== Round: "+ String(round)+ " // Position: " + String(pool->position())+ " / Turn: " + String(turn) + " // Time: " + String(time) + " // Duration: " + String(duration) + " // Macro: " + String(activeMacro()->name()) );

  // AUTO-NEXT 
  if (autoNext && round >= loopLoop[macro]) nextMacro(now);

  K32_anim* anim = activeMacro();
  if (anim) 
    anim->push(duration, time, round, turn, position, peers, solo );
}

