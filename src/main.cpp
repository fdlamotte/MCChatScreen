#define FIRMWARE_BUILD_DATE   "09 Mar 2025"

#include <Arduino.h>   // needed for PlatformIO
#include <BLEDevice.h>


#include <Mesh.h>
#include <helpers/BaseCompanionRadioMesh.h>

#include <RadioLib.h>
#include <helpers/CustomSX1262.h>
#include <modules/SX126x/SX1262.h>

#include <SPIFFS.h>

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/RadioLibWrappers.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/IdentityStore.h>
#include <helpers/BaseSerialInterface.h>
#include <RTClib.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SDA_OLED 17
#define SCL_OLED 18
#define Vext 36

#define USER_BTN 0

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     21 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#ifndef SCREEN_ON_TIME
#define SCREEN_ON_TIME 30000
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// 'meshcore', 128x13px
const unsigned char epd_bitmap_meshcore [] PROGMEM = {
    0x3c, 0x01, 0xe3, 0xff, 0xc7, 0xff, 0x8f, 0x03, 0x87, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 
    0x3c, 0x03, 0xe3, 0xff, 0xc7, 0xff, 0x8e, 0x03, 0x8f, 0xfe, 0x3f, 0xfe, 0x1f, 0xff, 0x1f, 0xfe, 
    0x3e, 0x03, 0xc3, 0xff, 0x8f, 0xff, 0x0e, 0x07, 0x8f, 0xfe, 0x7f, 0xfe, 0x1f, 0xff, 0x1f, 0xfc, 
    0x3e, 0x07, 0xc7, 0x80, 0x0e, 0x00, 0x0e, 0x07, 0x9e, 0x00, 0x78, 0x0e, 0x3c, 0x0f, 0x1c, 0x00, 
    0x3e, 0x0f, 0xc7, 0x80, 0x1e, 0x00, 0x0e, 0x07, 0x1e, 0x00, 0x70, 0x0e, 0x38, 0x0f, 0x3c, 0x00, 
    0x7f, 0x0f, 0xc7, 0xfe, 0x1f, 0xfc, 0x1f, 0xff, 0x1c, 0x00, 0x70, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x1f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x3f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x1e, 0x3f, 0xfe, 0x3f, 0xf0, 
    0x77, 0x3b, 0x87, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xfc, 0x38, 0x00, 
    0x77, 0xfb, 0x8f, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xf8, 0x38, 0x00, 
    0x73, 0xf3, 0x8f, 0xff, 0x0f, 0xff, 0x1c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x78, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfe, 0x3c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x3c, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfc, 0x3c, 0x0e, 0x1f, 0xf8, 0xff, 0xf8, 0x70, 0x3c, 0x7f, 0xf8, 
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 256)
const int epd_bitmap_allArray_LEN = 1;
const unsigned char* epd_bitmap_allArray[1] = {
    epd_bitmap_meshcore
};


#ifdef HAS_GPS

  #ifndef GPS_RX_PIN
  #define GPS_RX_PIN              (34)
  #endif

  #ifndef GPS_TX_PIN
  #define GPS_TX_PIN              (33)
  #endif

  #ifndef GPS_EN
  #define GPS_EN                  (47)
  #endif

  #ifndef GPS_RESET
  #define GPS_RESET               (48)
  #endif

  #ifndef GPS_BAUDRATE
  #define GPS_BAUDRATE            (9600)
  #endif

  #include "helpers/LocationProvider.h"
  #include "helpers/MicroNMEALocationProvider.h"
  HardwareSerial gps_serial(1);
  MicroNMEALocationProvider gps = MicroNMEALocationProvider (gps_serial);

#endif

/* ---------------------------------- CONFIGURATION ------------------------------------- */

// Believe it or not, this std C function is busted on some platforms!
static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

#ifndef TCP_PORT
#define TCP_PORT 5000
#endif

#ifdef WIFI_SSID
  #include <helpers/esp32/SerialWifiInterface.h>
  SerialWifiInterface serial_interface;
#elif defined(BLE_PIN_CODE)
   #include <helpers/esp32/SerialBLEInterface.h>
   SerialBLEInterface serial_interface;
#else
   #include <helpers/ArduinoSerialInterface.h>
   ArduinoSerialInterface serial_interface;
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

class MyMesh : public BaseCompanionRadioMesh {
#ifdef HAS_GPS
  bool gps_time_sync_needed = true;
  LocationProvider* _gps;
#endif

protected:
  char last_msg [MAX_FRAME_SIZE];
  char last_orig [80];
  int nextBlanking;

  void blankScreen() {
    display.setCursor(0,0);
    display.clearDisplay();
    display.display();
    nextBlanking = 0;
  }

  void drawHome() {
    display.setCursor(0,0);
    display.clearDisplay();
    display.drawBitmap(0,0,epd_bitmap_meshcore,128,13,SSD1306_WHITE);
    display.setTextSize(1);
    display.printf("\n\n\n%s\n", getNodeName());
    display.printf("Build: %s\n", FIRMWARE_BUILD_DATE);
    display.printf("freq : %03.2f sf %d\n", _prefs.freq, _prefs.sf);
    display.printf("bw   : %03.2f cr %d\n", _prefs.bw, _prefs.cr);
  #ifdef WIFI_SSID
    display.printf("IP ");
    display.print(WiFi.localIP());
  #endif
    display.display();

    if (SCREEN_ON_TIME > 0) {
      nextBlanking = futureMillis(SCREEN_ON_TIME);
    }
  }

  void drawScreen() {
    int msgs = getUnreadMsgNb();
    display.clearDisplay();
    if (msgs) {
      display.setCursor(0,0);
      display.setTextSize(1); 
      display.printf("%s\n", getNodeName());
      display.printf("\n\n");
      display.printf("-%s\n", last_orig);
      display.printf("%s\n", last_msg);
      display.setCursor(100,9);
      display.setTextSize(2);
      display.printf("%d", msgs);
    }
    display.display();

    if (SCREEN_ON_TIME > 0) {
      nextBlanking = futureMillis(SCREEN_ON_TIME);
    }
  }

  void onCommandDataRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char *text) override {
    BaseCompanionRadioMesh::onCommandDataRecv(from, pkt, sender_timestamp, text);    
    sprintf(last_orig, "] %s", from.name);
    strncpy(last_msg, text, MAX_FRAME_SIZE);
    drawScreen();
  }

  void onSignedMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const uint8_t *sender_prefix, const char *text) override {
    BaseCompanionRadioMesh::onSignedMessageRecv(from, pkt, sender_timestamp, sender_prefix, text);
    sprintf(last_orig, "> %s/%02X", from.name, sender_prefix[0]);
    strncpy(last_msg, text, MAX_FRAME_SIZE);
    drawScreen();    
  }


  void onMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char *text) override {
    BaseCompanionRadioMesh::onMessageRecv(from, pkt, sender_timestamp, text);
    sprintf(last_orig, "> %s", from.name);
    strncpy(last_msg, text, MAX_FRAME_SIZE);
    drawScreen();
  }

  void onChannelMessageRecv(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t timestamp, const char *text) override {
    BaseCompanionRadioMesh::onChannelMessageRecv(channel, pkt, timestamp, text);
    sprintf(last_orig, "Channel msg");
    strncpy(last_msg, text, MAX_FRAME_SIZE);
    drawScreen();
  }

  void onNextMsgSync() override {
    blankScreen();
  }

  void gpsHandler() {
#ifdef HAS_GPS
    static long next_gps_update = 0;
    if (millisHasNowPassed(next_gps_update)) {
      if (_gps->isValid()) {
        _prefs.node_lat = ((double)_gps->getLatitude())/1000000.;
        _prefs.node_lon = ((double)_gps->getLongitude())/1000000.;
        if (gps_time_sync_needed) {
          getRTCClock()->setCurrentTime(_gps->getTimestamp());
          gps_time_sync_needed = false;
        }
      }
      next_gps_update = futureMillis(5000);
    }
#endif
  }

  void buttonHandler() {
    static int nextBtnCheck = 0;
    static int lastBtnState = 0;
    if (millisHasNowPassed(nextBtnCheck)) {
      int btnState = digitalRead(USER_BTN);
      bool btnChanged = (btnState != lastBtnState);
      if (btnChanged && (btnState == LOW)) {
        if (getUnreadMsgNb() > 0) {
          drawScreen();
        } else {
          drawHome();
        }
      }
    
      lastBtnState = btnState;
      nextBtnCheck = futureMillis(500);  
    }  
  }

  void screenHandler() {        
    if ((nextBlanking > 0) && millisHasNowPassed(nextBlanking)) {
      blankScreen();
    }
  }

public:
#ifdef HAS_GPS
  MyMesh(RADIO_CLASS& phy, RadioLibWrapper& rw, mesh::RNG& rng, mesh::RTCClock& rtc, SimpleMeshTables& tables, LocationProvider& gps)
     : BaseCompanionRadioMesh(phy, rw, rng, rtc, tables, board, PUBLIC_GROUP_PSK, LORA_FREQ, LORA_SF, LORA_BW, LORA_CR, LORA_TX_POWER),
     _gps(&gps) {
#else
  MyMesh(RADIO_CLASS& phy, RadioLibWrapper& rw, mesh::RNG& rng, mesh::RTCClock& rtc, SimpleMeshTables& tables)
     : BaseCompanionRadioMesh(phy, rw, rng, rtc, tables, board, PUBLIC_GROUP_PSK, LORA_FREQ, LORA_SF, LORA_BW, LORA_CR, LORA_TX_POWER) {
#endif
    }

  void begin(FILESYSTEM& fs, mesh::RNG& trng) {
    BaseCompanionRadioMesh::begin(fs, trng);
    drawHome();
  }

  void loop() {
    BaseCompanionRadioMesh::loop();

    buttonHandler();
    gpsHandler();
    screenHandler();

    #ifdef HAS_GPS
    _gps->loop();
    #endif
  
  }
};

#ifdef HAS_GPS
MyMesh the_mesh(radio, *new WRAPPER_CLASS(radio, board), fast_rng, *new VolatileRTCClock(), tables, gps);
#else
MyMesh the_mesh(radio, *new WRAPPER_CLASS(radio, board), fast_rng, *new VolatileRTCClock(), tables);
#endif

void halt() {
  while (1) ;
}

void setup() {
  Serial.begin(115200);

  board.begin();

  if (!radio_init()) { halt(); }

#ifdef ESP32_CPU_FREQ
  setCpuFrequencyMhz(ESP32_CPU_FREQ);
#endif

#ifdef HAS_GPS
  gps_serial.setPins(GPS_RX_PIN, GPS_TX_PIN);
  gps_serial.begin(GPS_BAUDRATE);
  gps.begin();
#endif

  Wire1.begin(SDA_OLED, SCL_OLED);

  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;) delay(100); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.display();

  //pinMode(USER_BTN, INPUT);

  fast_rng.begin(radio.random(0x7FFFFFFF));

  RadioNoiseListener trng(radio);

  SPIFFS.begin(true);
  the_mesh.begin(SPIFFS, trng);

#ifdef WIFI_SSID
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  serial_interface.begin(TCP_PORT);
#elif defined(BLE_PIN_CODE)
  char dev_name[32+10];
  sprintf(dev_name, "MeshCore-%s", the_mesh.getNodeName());
  serial_interface.begin(dev_name, BLE_PIN_CODE);
#else
  serial_interface.begin(Serial);
#endif

  the_mesh.startInterface(serial_interface);
 }

void loop() {
  the_mesh.loop();
  delay(1);
}
