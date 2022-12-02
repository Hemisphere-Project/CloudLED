#include <K32_light.h>

// WIND
//
class Anim_cloud_wind : public K32_anim {
  public:
    int lastTime = 0;
    int nextTime = 0;

    void init() {}
    void draw (int data[ANIM_DATA_SLOTS])
    { 
      int duration = data[0];
      int time    = data[1];
      int round   = data[2];
      int turn    = data[3];
      int position = data[4];
      int count = data[5];
      
      // breath  = data[10];
      float progress = time*1.0f/duration;

      if (time < lastTime) nextTime = 0;
      lastTime = time;

      if (time > nextTime) 
      {
        for (int i=0; i<this->size(); i++) {
          byte rand = random(150, 250);
          CRGBW color = (count > 1) ? CRGBW{0,rand,rand-50} : CRGBW{rand/2,rand,0};
          this->pixel(i, color);
        }
        nextTime = time + random(50, 100);
      }
    }
};


// BREATH
//
class Anim_cloud_breath : public K32_anim {
  public:
    void init() {}
    void draw (int data[ANIM_DATA_SLOTS])
    { 
      int duration = data[0];
      int time    = data[1];
      int round   = data[2];
      int turn    = data[3];
      int position = data[4];
      int count = data[5];
      
      // breath  = data[10];
      float progress = time*1.0f/duration;
      int breath = (0.5f + 0.5f * sin(2 * PI * progress)) * 255;

      // DRAW ON STRIP
      /////////////////////////////////////////////////////////////////////////
      CRGBW color = (count > 1) ? CRGBW{breath,0,breath} : CRGBW{breath,0,0};
      this->all( color );
    }
};


