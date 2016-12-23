

#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>


int EffectSelector = 1;

/*-------- NodeMCU pin translation----------*/
#define D0 16
#define D1 5 // I2C Bus SCL (clock)
#define D2 4 // I2C Bus SDA (data)
#define D3 0
#define D4 2 // Same as "LED_BUILTIN", but inverted logic
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO 
#define D7 13 // SPI Bus MOSI
#define D8 15 // SPI Bus SS (CS)
#define D9 3 // RX0 (Serial console)
#define D10 1 // TX0 (Serial console)


/*-------- IO definition----------*/

#define PIN_PirUpstairs D1
#define PIN_PirDownstairs D2
#define PIN_NeoPixel D0

/*-------- Enumeration definition----------*/
// Direction of the light pattern
#define UP   0
#define DOWN 1

// Color options
#define MULTI  0
#define RED    1
#define ORANGE 2
#define YELLOW 3
#define GREEN  4
#define BLUE   5
#define PURPLE 6



/*-------- User settings----------*/
#define NeoPixelInStripe 64
static bool BLYNK_ENABLED = true;



/*-------- General call----------*/
Adafruit_NeoPixel lights = Adafruit_NeoPixel(NeoPixelInStripe, PIN_NeoPixel, NEO_GRB + NEO_KHZ800);

// Storage
#define EEPROM_SALT 12663
typedef struct {
  int   salt = EEPROM_SALT;
  char  blynkToken[33]  = "";
  char  blynkServer[33] = "blynk-cloud.com";
  char  blynkPort[6]    = "8442";
} WMSettings;

WMSettings settings;

#include <ArduinoOTA.h>




//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}


//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void restart() {
  ESP.reset();
  delay(1000);
}

void reset() {
  //reset settings to defaults
  /*
    WMSettings defaults;
    settings = defaults;
    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  */
  //reset wifi credentials
  WiFi.disconnect();
  delay(1000);
  ESP.reset();
  delay(1000);
}


/*-------- void setup ----------*/
void setup()
{
  Serial.begin(115200);

  const char *hostname = "ambient";
 
  // Pin mode config
  pinMode(PIN_PirUpstairs, INPUT);
  pinMode(PIN_PirDownstairs, INPUT);

  WiFiManager wifiManager;
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //timeout - this will quit WiFiManager if it's not configured in 3 minutes, causing a restart
  wifiManager.setConfigPortalTimeout(180);

  //custom params
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.salt != EEPROM_SALT) {
    Serial.println("Invalid settings in EEPROM, trying with defaults");
    WMSettings defaults;
    settings = defaults;
  }

  Serial.println(settings.blynkToken);
  Serial.println(settings.blynkServer);
  Serial.println(settings.blynkPort);

  WiFiManagerParameter custom_blynk_text("Blynk config. <br/> No token to disable.");
  wifiManager.addParameter(&custom_blynk_text);

  WiFiManagerParameter custom_blynk_token("blynk-token", "blynk token", settings.blynkToken, 33);
  wifiManager.addParameter(&custom_blynk_token);

  WiFiManagerParameter custom_blynk_server("blynk-server", "blynk server", settings.blynkServer, 33);
  wifiManager.addParameter(&custom_blynk_server);

  WiFiManagerParameter custom_blynk_port("blynk-port", "blynk port", settings.blynkPort, 6);
  wifiManager.addParameter(&custom_blynk_port);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (!wifiManager.autoConnect(hostname)) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //Serial.println(custom_blynk_token.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("Saving config");

    strcpy(settings.blynkToken, custom_blynk_token.getValue());
    strcpy(settings.blynkServer, custom_blynk_server.getValue());
    strcpy(settings.blynkPort, custom_blynk_port.getValue());

    Serial.println(settings.blynkToken);
    Serial.println(settings.blynkServer);
    Serial.println(settings.blynkPort);

    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  }

  //config blynk
  if (strlen(settings.blynkToken) == 0) {
    BLYNK_ENABLED = false;
  }
  if (BLYNK_ENABLED) {
    Blynk.config(settings.blynkToken, settings.blynkServer, atoi(settings.blynkPort));
  }

  //OTA
  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();


   // NeoPixel (Strip)
   lights.begin();
   
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");


  Serial.println("done setup");
}


void loop()
{

  //ota loop
  ArduinoOTA.handle();
  //blynk connect and run loop
  if (BLYNK_ENABLED) {
    Blynk.run();
  }

    if(!digitalRead(PIN_PirUpstairs)) {
      show_stair_lights(DOWN,EffectSelector);
   } else if(!digitalRead(PIN_PirDownstairs)) {
      show_stair_lights(UP,EffectSelector);
   }

}






void show_stair_lights(int direction, int effect) {
    switch(effect) {
      case 0:
//         basic_color(direction);
         break;
      case 1:
//         rainbow_fade(direction, 75);
         break;
      case 2:
//         rainbow_cycle(direction, 4, 20);
         break;
      case 3:
//         colorswirl(direction, 10000);
         break;
      case 4:
         trail(direction, 7, 10, 50);
         break;
      case 5:
  //       stack(direction, 50);
         break;
   }
}






void trail(int direction, int trail_len, int cycles, int delay_time) {
   unsigned char red   = 0;
   unsigned char green = 0;
   unsigned char blue  = 0;
   get_random_color(&red, &green, &blue);

   int start_pixel;
   int end_pixel;
   get_start_and_end_pixels(direction, &start_pixel, &end_pixel);

   // Generate the constants to multiply the brightness by for the trail
   float trail_constants[trail_len];
   trail_constants[0] = (float)1/trail_len;
   for(int i=2; i<=trail_len; i++) {
      trail_constants[i-1] = trail_constants[i-2] + (float)1/trail_len;
   }

   for(int i=0; i<cycles; i++) {
      // Pick a new random color on each cycle
      unsigned char red;
      unsigned char green;
      unsigned char blue;
      get_random_color(&red, &green, &blue);

      for(int j=start_pixel; (direction == UP ? j>=end_pixel-trail_len : j<=end_pixel+trail_len); (direction == DOWN ? j++ : j--)) {
         // Set all non-trail pixels to 0
         for(int k=0; k<lights.numPixels(); k++) {
            lights.setPixelColor(k, 0, 0, 0);
         }

         // Set the trail pixels to the appropriate color
         int k = j-trail_len;
         if(direction == UP) {
            k = j+trail_len;
         }
         for(int l=0; (direction == UP ? k>j-trail_len : k<j+trail_len); (direction == UP ? k-- : k++), l++) {
            lights.setPixelColor(k, red*trail_constants[l], green*trail_constants[l], blue*trail_constants[l]);
         }

         lights.show();

         delay(delay_time);
      }
   }

   clear_lights();
}




void get_random_color(unsigned char *red, unsigned char *green, unsigned char *blue) {
   int color = random(RED, PURPLE+1);

   *red   = 0;
   *green = 0;
   *blue  = 0;

   switch(color) {
      case RED:
         *red = 255;
         break;
      case ORANGE:
         *red   = 255;
         *green = 165;
         break;
      case YELLOW:
         *red   = 255;
         *green = 255;
         break;
      case GREEN:
         *green = 255;
         break;
      case BLUE:
         *blue = 255;
         break;
      case PURPLE:
         *red  = 128;
         *blue = 128;
         break;
   }
}


uint32_t get_random_color(void) {
   unsigned char red   = 0;
   unsigned char green = 0;
   unsigned char blue  = 0;

   get_random_color(&red, &green, &blue);
   return create_color(red, green, blue);
}




void get_start_and_end_pixels(int direction, int *start_pixel, int *end_pixel) {
   switch(direction) {
      case UP:
         *start_pixel = lights.numPixels() - 1;
         *end_pixel = 0;
         break;
      case DOWN:
         *start_pixel = 0;
         *end_pixel = lights.numPixels() - 1;
         break;
   }
}

void clear_lights(void) {
   // Turn off all lights
   for(int i=0; i<lights.numPixels(); i++) {
      lights.setPixelColor(i, 0);
      lights.show();
   }
}


uint32_t create_color(unsigned char red, unsigned char green, unsigned char blue) {
   // Create a 24 bit color value from R, G, B values
   // Bits 24-16: red
   // Bits 15-8:  green
   // Bits 7-0:   blue
   uint32_t color;

   color = red;
   color <<= 8;

   color |= green;
   color <<= 8;
   
   color |= blue;

   return color;
}


/*-------- Blynk calls ----------*/


//restart - button
BLYNK_WRITE(0) {
  int a = param.asInt();
  if (a != 0) {
    restart();
  }
}

//reset - button
BLYNK_WRITE(1) {
  int a = param.asInt();
  if (a != 0) {
    reset();
  }
}


//effect - selector
BLYNK_WRITE(2) {
  EffectSelector = param.asInt();

}
