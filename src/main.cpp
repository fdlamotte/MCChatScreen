#define FIRMWARE_BUILD_DATE   "27 Feb 2025"

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

#include <helpers/HeltecV3Board.h>
#include <helpers/CustomSX1262Wrapper.h>

static HeltecV3Board board;

#define SDA_OLED 17
#define SCL_OLED 18
#define Vext 36

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     21 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

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

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
  #define LORA_FREQ   915.0
#endif
#ifndef LORA_BW
  #define LORA_BW     250
#endif
#ifndef LORA_SF
  #define LORA_SF     10
#endif
#ifndef LORA_CR
  #define LORA_CR      5
#endif
#ifndef LORA_TX_POWER
  #define LORA_TX_POWER  20
#endif

#ifndef MAX_LORA_TX_POWER
  #define MAX_LORA_TX_POWER  LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
  #define MAX_CONTACTS         100
#endif

#ifndef OFFLINE_QUEUE_SIZE
  #define OFFLINE_QUEUE_SIZE  16
#endif

#define  PUBLIC_GROUP_PSK  "izOH6cXN6mrJ5e26oRXNcg=="

// Believe it or not, this std C function is busted on some platforms!
static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

#ifdef BLE_PIN_CODE
  #include <helpers/esp32/SerialBLEInterface.h>
  SerialBLEInterface serial_interface;
#else
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface serial_interface;
#endif


#ifdef P_LORA_SCLK
SPIClass spi;
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);
#else
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY);
#endif
StdRNG fast_rng;
SimpleMeshTables tables;

class MyMesh : public BaseCompanionRadioMesh {

public:
  MyMesh(RADIO_CLASS& phy, RadioLibWrapper& rw, mesh::RNG& rng, mesh::RTCClock& rtc, SimpleMeshTables& tables)
     : BaseCompanionRadioMesh(phy, rw, rng, rtc, tables, board, PUBLIC_GROUP_PSK, LORA_FREQ, LORA_SF, LORA_BW, LORA_CR, LORA_TX_POWER) {
    }

  void begin(FILESYSTEM& fs, mesh::RNG& trng) {
    BaseCompanionRadioMesh::begin(fs, trng);

    display.setCursor(0,0);
    display.clearDisplay();
    display.drawBitmap(0,0,epd_bitmap_meshcore,128,13,SSD1306_WHITE);
    display.setTextSize(1);
    display.printf("\n\n\n%s\n", getNodeName());
    display.printf("Build: %s\n", FIRMWARE_BUILD_DATE);
    display.printf("freq : %03.2f sf %d\n", _prefs.freq, _prefs.sf);
    display.printf("bw   : %03.2f cr %d\n", _prefs.bw, _prefs.cr);
    display.display();  
  }


protected:
  char last_msg [MAX_FRAME_SIZE];
  char last_orig [80];

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
  }

  void onMessageRecv(const ContactInfo& from, uint8_t path_len, uint32_t sender_timestamp, const char *text) override {
    BaseCompanionRadioMesh::onMessageRecv(from, path_len, sender_timestamp, text);
    if (path_len == 0xFF) {
      sprintf(last_orig, "(D) %s", from.name);
    } else {
      sprintf(last_orig, "(%d) %s", path_len, from.name);
    }
    strncpy(last_msg, text, MAX_FRAME_SIZE);
    drawScreen();
  }
  void onChannelMessageRecv(const mesh::GroupChannel& channel, int in_path_len, uint32_t timestamp, const char *text) override {
    BaseCompanionRadioMesh::onChannelMessageRecv(channel, in_path_len, timestamp, text);
    if (in_path_len == 0xFF) {
      sprintf(last_orig, "(D) Chan");
    } else {
      sprintf(last_orig, "(%d) Chan");
    }
    strncpy(last_msg, text, MAX_FRAME_SIZE);
    drawScreen();
  }
  void onNextMsgSync() override {
    drawScreen();
  }

};


MyMesh the_mesh(radio, *new WRAPPER_CLASS(radio, board), fast_rng, *new VolatileRTCClock(), tables);

void halt() {
  while (1) ;
}

void setup() {
  Serial.begin(115200);

  board.begin();

#ifdef SX126X_DIO3_TCXO_VOLTAGE
  float tcxo = SX126X_DIO3_TCXO_VOLTAGE;
#else
  float tcxo = 1.6f;
#endif

#ifdef P_LORA_SCLK
  spi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
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

  int status = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POWER, 8, tcxo);
  if (status != RADIOLIB_ERR_NONE) {
    Serial.print("ERROR: radio init failed: ");
    Serial.println(status);
    halt();
  }

  radio.setCRC(0);

#ifdef SX126X_CURRENT_LIMIT
  radio.setCurrentLimit(SX126X_CURRENT_LIMIT);
#endif

#ifdef SX126X_DIO2_AS_RF_SWITCH
  radio.setDio2AsRfSwitch(SX126X_DIO2_AS_RF_SWITCH);
#endif

  fast_rng.begin(radio.random(0x7FFFFFFF));

  RadioNoiseListener trng(radio);

  SPIFFS.begin(true);
  the_mesh.begin(SPIFFS, trng);

#ifdef BLE_PIN_CODE
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
}
