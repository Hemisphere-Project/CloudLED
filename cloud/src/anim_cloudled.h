#include <K32_light.h>

#define N_COLOR 7

CRGBW colorPreset[N_COLOR] = {
  {CRGBW::Red},           // 0
  {CRGBW::Lime},          // 1
  {CRGBW::Blue},          // 2
  {CRGBW::Yellow},        // 3
  {CRGBW::Magenta},       // 4
  {CRGBW::Cyan},          // 5
  {CRGBW::Orange}         // 6
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
    CRGBW background;

    void init() { this->background = colorPreset[random(0,N_COLOR)]; }
    void draw (int data[ANIM_DATA_SLOTS])
    { 
      int duration = data[0];
      int time    = data[1];
      int round   = data[2];
      int turn    = data[3];
      int position = data[4];
      int count = data[5];
      
      float progress = time*1.0f/duration;
      byte breath = (0.5f + 0.5f * sin(2 * PI * progress)) * 255;

      this->all( (CRGBW)(background%breath) );
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
      this->background = colorPreset[random(0,N_COLOR)];
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
      
      if (turn == position) 
      {

        // BACKGROUND
        this->all( this->background );

        // CRAWLER
        int pos = (int)(progress * this->size());
        for (int i=pos; i>pos-3; i--) {
          if (i >= 0 && i < this->size()) {
            this->pixel(i, CRGBW{255,255,255});
          }
        }
      }
      else
      {
        // float progx2 = (progress + (turn+round*count)%2)/2;

        int breath = (0.5f + 0.5f * cos(2 * PI * progress )) * 255;
        this->all( (CRGBW) (this->background % (127+breath/2)) );
      }


    }
};


// RAINBOW
//
class Anim_cloud_rainbow : public K32_anim {
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
      
      int offset = this->size() * time / duration;
      
      CRGBW colorWheel;
      for(int i=0; i<this->size(); i++)
        this->pixel(i, colorWheel.setHue( 255 * ((i+offset) % this->size()) / this->size() ) );

    }
};