#include <K32_light.h>


// OUTILS
//

#define STROB_ON_MS  12

enum color_mode { 
  COLOR_BI,
  COLOR_TRI,
  COLOR_QUAD,      
  COLOR_PICKER,    
  COLOR_SD        
};

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


// Transform DMX range into simple value, I.E.  0-10 => 0, 11-20 => 1, 21-30 => 2, ....
inline int simplifyDmxRange(int value) {
  if (value > 0) value -= 1;
  return value/10;
}

// Return rounded value after dividing by 255
inline int roundDivInt(int a, int b) {
  return a/b + ((a%b)>(b/2));
}

// Scale a number based on a 8bit value
inline int scale255(int a, uint8_t value) {
  return roundDivInt( a*value, 255 );
}

// Check if int is min <= value <= max
inline bool btw(int value, int min, int max) {
  return (value >= min) && (value <= max);
}


// ANIM DMX
//

/* NOTES concernant la création d'une Animation:

Une animation est une classe (Anim_dmx par exemple) qui décrit la génération d'une image sur une portion de strip led.
La sortie LED, la portion de ruban et le décalage sont donnés à la création de l'animation (cf main.cpp)

Anim_dmx hérite de la classe K32_anim.

    - Une K32_anim est créée et attachée dans le programme principal en utilisant k32->light:
        
        k32->light->anim( 0, "artnet-strip", new Anim_dmx, LULU_STRIP_SIZE, 0);

            0               = numéro de la sortie (strip) sur laquelle opère l'animation
            "artnet-strip"        = alias de l'animation, permet de la rappeler par la suite en utilisant k32->light->anim("artnet-strip")
            new Anim_dmx    = l'animation en question, new permet de créer une nouvelle instance à partir de classe Anim_dmx
            LULU_STRIP_SIZE      = nombre de pixels utiles pour l'animation (optionnel -> defaut = full strip size)
            0               = décalage en pixel à partir du début du strip (optionnel -> defaut = 0)


    - Une anim existante peut être rappelée en utilisant    k32->light->anim("artnet-strip");


    - Dans le programme principal, une animation propose les fonctions publiques suivantes.
      Elles peuvent être chainnées ainsi:   k32->light->anim("artnet-strip")->master(100)->play()->wait(1000)->push(200);

        *  play()               lance l'animation
        *  play(1000)           lance l'animation pour une durée limitée (en millisecondes) puis s'arrête
        *  isPlaying()          savoir si l'animation joue
        *  stop()               arrête une animation
        *  wait()               bloque jusqu'à ce que l'animation s'arrête
        *  wait(1000)           bloque mais avec une durée maximum d'attente
        *  loop(true|false)     définit si l'animation joue en boucle
        *  loop()               savoir si l'animation joue en boucle
        
        *  master(255)          régle le MASTER de l'animation
        
        *  name()               alias de l'animation
        *  size()               nombre de pixels couverts par l'animation
        
        Afin d'alimenter les données internes permettant à l'animation de jouer:

        *  push(data[], size)   permet de passer un tableau de int à l'animation. size maximum = ANIM_DATA_SLOTS
        *  push(d0, d1, ...)    permet de passer les data sous forme d'une liste d'arguments. maximum = 8 arguments.
        *  push()               ne fourni aucune donnée, déclenche le rafraichissement de l'animation (comme si les data avaient changées)
        *  set(i, value)        enregistre data[i] à <value>, mais n'est pas considéré comme un changement de donnée. appeler push() pour appliquer ! 
    
        
    - Anim_dmx doit definir une fonction    void draw()  qui lui est propre.
      Cette fonction sera appelée à chaque fois que des nouvelles données sont disponibles afin de générer et dessiner
      une nouvelle image sur le strip.


    - Dans draw(), en plus des fonctions publiques listées ci-dessus,  
      il est possible d'accéder à un certain nombre de fonctions et variables internes propres à une K32_anim:

        *  data[]  est un tableau de int qui contient les dernières données fournies à l'anim via set(..) et push(..)
                    Ces données sont fournies par le programme principal qui utilise la fonction ->push() de l'animation
                    Dans KXKMSACIO c'est soit la portion utile de la trame ArtNet qui est fournie, soit une mémoire du fichier mem.h
                    L'animation peut donc utiliser ce tableau data[] comme base pour construire l'image.
                    Avant d'arriver à l'animation, data[] passe par les modulateurs qui peuvent les modifier.
                    Attention: il est inutile de modifier ce tableau, les modifs depuis l'animation seront écrasée lors de l'appel suivant.

        *  clear()                                      noir sur tous les pixels de l'animation
        *  all(CRGBW color)                             <color> sur tous les pixels de l'animation
        *  pixel(int i, CRGBW color)                    <color> sur le pixel <i>     
        *  pixel(int start, int count, CRGBW color)     <color> sur <count> pixels consécutifs à partir de <start>     

        *  startTime    time du lancement de l'animation, à l'appel de play()


    - il est également possible de définir une function    void init()   qui sera appelée à chaque lancement de l'animation via play()


    - Les functions et variables présentées ci-dessus sont communes à toutes les K32_anim.
      Il est cependant possible de créer des variables et functions internes spécifiques à une animation.
      Il est également possible de créer des alias sur les data[], par exemple:
        
            int& red = data[1];

      Permet ensuite dans l'animation d'utiliser la variable red à la place de data[1] ce qui clarifie l'écriture de la logique draw()
      Cette technique n'est pas utilisée ici mais dans anim_monitoring.h


    - Il est possible d'ajouter des modulateurs à une animation.
        Les modulateurs peuvent être nommés (créé une fois, puis accessible avec leur alias), ou anonyme (pour les modulateurs "jetables").

        * mod("sincity", new K3_mod_sinus)  modulateur nommé, pour être rappelé avec    mod("sincity")  
                                                n'est pas lancé automatiquement, nécessite l'appel à    mod("sincity")->play()
        
        * mod(new K3_mod_pulse)             modulateur jetable, ne pourra pas être rappelé. lancé automatiquement.
      
        * unmod()                           permet de supprimer tous les modulateurs anonymes d'une animation  (ajouter true pour supprimer TOUS les modulateurs)

        Les modulateurs peuvent être attaché à une ou plusieurs cases data de l'animation   mod("sincity")->at(1)->at(2)
        Lorsqu'il est lancé, il viendra moduler la case data en fonction de sa valeur propre 
        cette modulation est relative, comme application d'un "master".

        Les modulateurs non attachés peuvent être utilisés en appelant  mod("sincity")->value()  afin d'obtenir sa valeur
        et d'en faire bon usage.

        Lorsque un modulateur est lancé (qu'il soit attaché ou non), il va déclencher le rafraichissement de l'animation dès que sa valeur interne change. 
        Il est donc important d'arrêter un modulateur qui n'est plus utilisé avec    mod("sincity")->stop()
        Ou en supprimant tous les modulateurs avec la fonction    unmod()   de l'animation.

        Pour configurer les modulateur, voir K32_mods.h


La logique spécifique de Anim_dmx consiste à préparer un segment élémentaire et le dessiner ensuite (avec eventuel zoom & mirror) sur le strip
en utilisant les fonctions évoquées ci-dessus.

*/


class Anim_dmx_strip : public K32_anim {
  public:


    // Setup
    void init() {

      // Strobe on data[0] (animMaster)
      this->mod("strobe", new K32_mod_pulse)->param(0, STROB_ON_MS)->at(0);

      // Smooth on data[0] (animMaster)
      this->mod("smooth", new K32_mod_sinus)->at(0);

    }


    // Loop
    void draw (int data[ANIM_DATA_SLOTS])
    {
      //
      // ONDMXFRAME
      //

      // LOGF("ANIM: %s frame ", name());
      // for (int i=0; i<LULU_PATCHSIZE; i++) {
      //   LOGINL(data[i]); LOGINL(" ");
      // }
      // LOG("");

      // animMaster @0 = nothing to draw
      if (data[0] == 0) {
        this->clear();
        return;
      }
      
      // Mirror & Zoom -> Segment size
      int mirrorMode  = simplifyDmxRange(data[14]);
      int zoomedSize  = max(1, scale255( size(), data[15]) );
      // int zoomedSize = size();

      int segmentSize = zoomedSize;
      if (mirrorMode == 1 || mirrorMode == 4)       segmentSize /= 2;
      else if (mirrorMode == 2 || mirrorMode == 5)  segmentSize /= 3;
      else if (mirrorMode == 3 || mirrorMode == 6)  segmentSize /= 4;
      if (segmentSize<1) segmentSize = 1;

      // Modes
      int pixMode     = simplifyDmxRange(data[5]);
      // LOGF("pixMode: %d\n", pixMode);

      int colorMode = COLOR_BI;
      if (pixMode == 23) colorMode = COLOR_TRI;
      else if (pixMode == 24) colorMode = COLOR_QUAD;
      else if (btw(pixMode, 6, 11)) colorMode = COLOR_PICKER; 
      // else if (btw(pixMode, 12, 17)) colorMode = COLOR_SD;


      // Segment Buffer
      CRGBW segment[segmentSize];


      // PRIMARY COLOR
      /////////////////////////////////////////////////////////////////////////

      // Color mode BI
      if (colorMode == COLOR_BI)
      {
        CRGBW color1 {data[1], data[2], data[3], data[4]};
        for(int i=0; i<segmentSize; i++) segment[i] = color1;
      }

      // Color mode PICKER
      else if (colorMode == COLOR_PICKER) {

        // Hue range
        int hueStart = data[10] + data[7];
        int hueEnd = data[11] + data[7];

        // Channel master
        CRGBW rgbwMaster {data[1], data[2], data[3], data[4]};
        
        // Color wheel
        CRGBW colorWheel;
        for(int i=0; i<segmentSize; i++) {
          segment[i] = colorWheel.setHue( (hueStart + ((hueEnd - hueStart) * i) / segmentSize) );
          segment[i] %= rgbwMaster;
        }

      }

      // Color mode SD
      else if (colorMode == COLOR_SD) {
        // TODO: load image from SD !


      }
      
      // Color mode TRI / QUADRI
      else if (colorMode == COLOR_TRI || colorMode == COLOR_QUAD) {
        
        // Colors
        CRGBW color[5] = {
          {data[10], data[11], data[12], data[13]}, // background     // color[0]
          colorPreset[ simplifyDmxRange(data[1]) ],                   // color[1]
          colorPreset[ simplifyDmxRange(data[2]) ],                   // color[2]
          colorPreset[ simplifyDmxRange(data[3]) ],                   // color[3]
          colorPreset[ simplifyDmxRange(data[4]) ]                    // color[4]
        };

        // Dash Length + Offset + Split 
        int dashLength  = max(3, scale255(2 * segmentSize, data[6]));          
        int dashOffset  = scale255((segmentSize-dashLength), data[7]);
        int dashSplit = (colorMode == COLOR_QUAD) ? 4 : 3;
        int dashPart  = 0;

        // Multi-color dash / color0 backgound
        for(int i=0; i<segmentSize; i++) 
        {
          dashPart = (i-dashOffset)*dashSplit/dashLength + 1;       // find in which part of the dash we are
          if (dashPart < 0 || dashPart > dashSplit) dashPart = 0;   // use 0 if outside of the dash (=> background)

          segment[i] = color[ dashPart ];
        }          

      }   


      // APPLY PIX MOD BACKGROUND
      /////////////////////////////////////////////////////////////////////////

      if (colorMode == COLOR_BI || colorMode == COLOR_PICKER) {

        // Background Color
        CRGBW backColor {CRGBW::Black};
        if (colorMode == COLOR_BI) backColor = CRGBW{data[10], data[11], data[12], data[13]};

        // Dash Length + Offset
        int dashLength  = max(1, scale255(segmentSize, data[6]) );          // pix_start
        int dashOffset  =  scale255(segmentSize+2*dashLength, data[7])-dashLength;                  // pix_pos

        // 2 colors = color1 one-dash / backColor background
        if (pixMode == 1 || pixMode == 7) 
        {
          for(int i=dashLength; i<segmentSize+dashLength; i++) 
            if (i < dashOffset || i >= dashOffset+dashLength) segment[i-dashLength] = backColor;
        }

        // 01:02 = color1 + backColor dash 
        if (pixMode == 2 || pixMode == 8) 
        {
          dashLength = max(1, data[6]);   // on this one, length is absolute and not relative to segementSize

          for(int i=0; i<segmentSize; i++) 
            if ( (i+dashOffset)/dashLength % 2 == 1 ) segment[i] = backColor;
        }

        // rusf>  = color1 one-dash fade R / backColor background
        else if (pixMode == 3 || pixMode == 9) 
        {
          for(int i=0; i<segmentSize; i++) 
          {
            if (i >= dashOffset && i < dashOffset+dashLength) 
            {
              int coef = (dashLength>1) ? ((dashOffset+dashLength-1-i) * 255 ) / (dashLength-1) : 0;

              segment[i] %= (uint8_t)coef;
              segment[i] += backColor % (uint8_t)(255-coef);
            }
            else segment[i] = backColor;
          }
        }

        // rusf<  = color1 one-dash fade L / color2 background
        else if (pixMode == 4 || pixMode == 10) 
        {
          // LOGF("dashLength: %d\n", dashLength);
          for(int i=0; i<segmentSize; i++) 
          {
            if (i >= dashOffset && i < dashOffset+dashLength) 
            {
              int coef = (dashLength>1) ? ((i-dashOffset) * 255 ) / (dashLength-1) : 0;

              segment[i] %= (uint8_t)coef;
              segment[i] += backColor % (uint8_t)(255-coef);
            }
            else segment[i] = backColor;
          }
          // LOG("dash done");
        }

        // rusf<>  = color1 one-dash fade LR / color2 background
        else if (pixMode == 5 || pixMode == 11) 
        {
          for(int i=0; i<segmentSize; i++) 
          { 
            if (i >= dashOffset && i < dashOffset+dashLength/2) 
            {
              int coef = (dashLength>1) ? ((i-dashOffset) * 255 ) / (dashLength/2) : 0;

              segment[i] %= (uint8_t)coef;
              segment[i] += backColor % (uint8_t)(255-coef);
            }
            else if (i >= dashOffset+dashLength/2 && i < dashOffset+dashLength) 
            {
              int coef = (dashLength>1) ? ((dashOffset+dashLength-1-i) * 255 ) / (dashLength/2) : 0;

              segment[i] %= (uint8_t)coef;
              segment[i] += backColor % (uint8_t)(255-coef);
            }
            else segment[i] = backColor;
          }
        }

      }


      // STROBE
      /////////////////////////////////////////////////////////////////////////
    
      int strobeMode  = simplifyDmxRange(data[8]);
      int strobePeriod = data[9];

      // strobe modulator
      if (strobeMode == 1 || btw(strobeMode, 3, 10)) 
        this->mod("strobe")->period( strobePeriod )->play();
      else 
        this->mod("strobe")->stop();

    
      // BLINK
      /////////////////////////////////////////////////////////////////////////

      // strobe blink (3xstrobe -> blind 1+s)
      if (strobeMode == 11 || btw(strobeMode, 12, 19)) 
      {
        // int count = this->strobe->periodCount() % 3;  // ERROR: periodCount is moving.. -> count is not linear !
        // // LOG(count);

        // if (count == 0)       this->strobe->period( strobePeriod*100/225 );
        // else if (count == 1)  this->strobe->period( strobePeriod/4 );
        // else if (count == 2)  this->strobe->period( strobePeriod*116/100 + 1000 );
        
        // // OFF
        // if (this->strobe->value() == 0) {
        //   this->clear();
        //   return;
        // }

        // TODO: make a special "multi-pulse" modulator
      }


      // SMOOTH
      /////////////////////////////////////////////////////////////////////////

      // smooth modulator
      if (strobeMode == 2) 
        this->mod("smooth")->period( strobePeriod )->play();
      else 
        this->mod("smooth")->stop();

      
      // RANDOM
      /////////////////////////////////////////////////////////////////////////

      // random w/ threshold
      if (btw(strobeMode, 3, 10) || btw(strobeMode, 12, 19) || btw(strobeMode, 20, 25)) 
      {
        // Seuil calculation depends of mode
        int strobeSeuil = 1000; 
        if (btw(strobeMode, 3, 10))       strobeSeuil = (data[8] - 31)*1000/69;         // 0->1000    strobeMode >= 3 && strobeMode <= 10
        else if (btw(strobeMode, 12, 19)) strobeSeuil = (data[8] - 121)*1000/79;        // 0->1000    strobeMode >= 12 && strobeMode <= 19
        else if (btw(strobeMode, 20, 25)) strobeSeuil = (data[8] - 201)*1000/54;        // 0->1000    strobeMode >= 20

        // apply random Seuil
        for(int i=0; i<segmentSize; i++) 
          if (random(1000) > strobeSeuil) segment[i] = {CRGBW::Black};
      }


      // ANIM-MASTER
      /////////////////////////////////////////////////////////////////////////

      for(int i=0; i<segmentSize; i++) segment[i] %= (uint8_t)data[0];



      // DRAW ON STRIP WITH ZOOM & MIRROR
      /////////////////////////////////////////////////////////////////////////

      // Clear
      this->clear();

      // Mirroring alternate (1 = copy, 2 = alternate)
      int mirrorAlternate = 1 + btw(mirrorMode, 1, 3); 

      // Zoom offset
      int zoomOffset = (size() - zoomedSize)/2;  

      // Copy pixels into strip
      for(int i=0; i<zoomedSize; i++) 
      {
        int pix  = i % segmentSize;               // pix cursor into segment
        int iter = i / segmentSize;               // count of mirror copy 
        
        if (iter && iter % mirrorAlternate)       // alternate: invert pix cursor
          pix = segmentSize - pix - 1;    

        this->pixel(i+zoomOffset, segment[pix]);  // draw on strip
      } 

    }

};