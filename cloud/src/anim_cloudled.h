#include <K32_light.h>

CRGBW colorPreset[25] = {
  {CRGBW::Black},         // 0
  {CRGBW::Red},           // 1
  {CRGBW::Lime},          // 2
  {CRGBW::Blue},          // 3
  {CRGBW::White},         // 4
  {CRGBW::Yellow},        // 5
  {CRGBW::Magenta},       // 6
  {CRGBW::Cyan},          // 7
  {CRGBW::Orange}         // 8
};

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
        nextTime = time + random(30, 140);
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
      
      float progress = time*1.0f/duration;
      int breath = (0.5f + 0.5f * sin(2 * PI * progress)) * 255;

      // DRAW ON STRIP
      /////////////////////////////////////////////////////////////////////////
      this->all( CRGBW{breath,0,breath} );
    }
};


// SPARKLE
//
class Anim_cloud_sparkle : public K32_anim {
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
      
      float progress = time*1.0f/duration;

      if (time < lastTime) nextTime = 0;
      lastTime = time;

      if (time > nextTime) 
      {
        for (int i=0; i<this->size(); i++) {
          CRGBW color = CRGBW{0,0,0};
          if (random(0,15) == 0) {
            color = CRGBW{255,255,255}; 
          }
          this->pixel(i, color);
        }
        nextTime = time + random(5, 20);
      }
    }
};


// CRAWLER
//
class Anim_cloud_crawler : public K32_anim {
  public:
    CRGBW background;
    
    void init() {
      this->background = colorPreset[random(1,9)];
    }

    void draw (int data[ANIM_DATA_SLOTS])
    { 
      int duration = data[0];
      int time    = data[1];
      int round   = data[2];
      int turn    = data[3];
      int position = data[4];
      int count = data[5];
      
      float progress = time*1.0f/duration;

      // 2x slower breath
      float progx2 = (progress + (turn+round*count)%2)/2;

      int breath = (0.5f + 0.5f * sin(2 * PI * progx2 )) * 255;

      // BACKGROUND
      this->all( this->background % (127+breath/2) );

      // CRAWLER
      if (turn == position) 
      {
        int pos = (int)(progress * this->size());
        for (int i=pos; i>pos-3; i--) {
          if (i >= 0 && i < this->size()) {
            this->pixel(i, CRGBW{255,255,255});
          }
        }
      }

    }
};
