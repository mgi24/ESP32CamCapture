#include "esp_camera.h"
#include <WiFi.h>
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "driver/rtc_io.h"
int pictureNumber = 0;
#include <ESPAsyncWebServer.h>
AsyncWebServer server(8000);
String report;
//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15 
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "WIFI RUMAH";
const char* ssidAP = "ESP32Cam";
const char* password = "12345678";
bool isRecord = false;
bool isFPS = false;
float fps = 0;
unsigned long lastMillis = 0;
unsigned int frameCount=0;

void startCameraServer();
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    report = "SD CARD GADA";
    
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    report = "SD CARD GADA";
    
  }

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
  //START AP MODE (BACKUP)
  WiFi.softAP(ssidAP);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i<20)  {
    
    i++;
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  Serial.print("IP Address AP: ");
  Serial.println(WiFi.softAPIP());

  //SERVER WEB KEDUA
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><head><meta http-equiv='refresh' content='2'></head><body>";
    html += "<h1>ESP32 Async Web Server</h1>";
    html += "<h1>Frame now"+String(pictureNumber)+"</h1>";
    html += "<h1>FPS"+String(fps)+"</h1>";
    html += "<h1>REPORT"+report+"</h1>";
    html += "<form action='/start' method='get'><button type='submit'>Start Record</button></form>";
    html += "<form action='/stop' method='get'><button type='submit'>Stop Record</button></form>";
    html += "<form action='/fps' method='get'><button type='submit'>FPS</button></form>";
    html += "<form action='/stopfps' method='get'><button type='submit'>Stop FPS</button></form>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });
  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
    isRecord = true;
    request->redirect("/");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    isRecord = false;
    request->redirect("/");
  });
  server.on("/stopfps", HTTP_GET, [](AsyncWebServerRequest *request){
    isFPS = false;
    request->redirect("/");
  });
  server.on("/fps", HTTP_GET, [](AsyncWebServerRequest *request){
    isFPS = true;
    request->redirect("/");
  });

  server.begin();
}

void loop() {
  if (isRecord){
    record();
  }
  else{
    notrecord();
    if (isFPS){
      isFps();
    }else{
      fps = 0;
    }
  }

}

void isFps(){//TEST FPS TANPA LOAD (MAX FPS)
  unsigned long currentMillis = millis();
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
    fps = currentMillis - lastMillis;
    fps = 1000 / fps;
    Serial.printf("FPS: %.2f\n", fps);
    lastMillis = currentMillis;
  esp_camera_fb_return(fb);
}

void record(){//KALAU RECORD KODE INI JALAN
  //INI HANYA UNTUK CEK FPS
  unsigned long currentMillis = millis();
  //SETUP FRAMEBUFFER
  camera_fb_t * fb = NULL;
  //INJECT FRAMEBUFFER
  fb = esp_camera_fb_get();
  //MISAL FRAMEBUFFER GAGAL  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  //PICTURE HANDLER
  pictureNumber++;
  //NAMA FILE PIC
  String path = "/picture" + String(pictureNumber) +".jpg";
  //START SDMMC MELALUI METODE SPIFFS
  fs::FS &fs = SD_MMC; 
  //DEBUG
  Serial.printf("Picture file name: %s\n", path.c_str());
  //BIKIN FILE BARU
  File file = fs.open(path.c_str(), FILE_WRITE);
  //CEK BERHASIL ATAU GAGAL SAVE
  if(!file){
    Serial.println("Failed to open file in writing mode");
    report = "Failed to open file in writing mode";
  } 
  else { //KALAU BERHASIL SAVE FRAMEBUFFER KE JPG
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", path.c_str());
    report = ("Saved file to path: %s\n", path.c_str());
  }
  //TUTUP FILENYA
  file.close();
  //NGECEK FPS DOANG SIH
  fps = currentMillis - lastMillis;
  fps = 1000 / fps;
  //SHOW DAPET BERAPA FPS
  Serial.printf("FPS: %.2f\n", fps);
  lastMillis = currentMillis;
  //RETURN FRAME BUFFER AGAR BISA CAPTURE GAMBAR LAGI
  esp_camera_fb_return(fb); 
}
void notrecord(){
  report = "not recording";
}
