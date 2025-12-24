#define IO_USERNAME  "maqragor"
#define IO_KEY       "aio_TrSk04zc9l9YAxozG8G35OFUjPma"

// --- WIFI AYARLARI ---
#define WIFI_SSID "HUSO"
#define WIFI_PASS "galatasaray"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AdafruitIO_WiFi.h" // Adafruit IO Kütüphanesi

// Adafruit IO Bağlantısı
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Feed Tanımlama (Adafruit IO'daki feed adı ile aynı olmalı)
AdafruitIO_Feed *eggFeed = io.feed("yumurta-sayaci");

#define IR_ANALOG 34

// -------- OLED AYARLARI --------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------- SENSÖR AYARLARI --------
const int THRESHOLD = 50;
const int REQUIRED_COUNT = 5;
const unsigned long SAMPLE_INTERVAL = 30; 

int confirmCount = 0;
int eggCount = 0;
bool eggDetected = false;
unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);

  // I2C başlat
  Wire.begin(21, 22);

  // OLED başlat
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("OLED bulunamadi"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Connecting to IO...");
  display.display();

  // Adafruit IO Bağlantısını Başlat
  Serial.print("Adafruit IO Baglantisi Kuruluyor...");
  io.connect();

  // Bağlantı sağlanana kadar bekle
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nBaglantı Basarili!");
  
  updateDisplay();
}

void loop() {
  // Adafruit IO'yu canlı tutmak için her loop başında çağrılmalı
  io.run();

  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = now;
    int val = analogRead(IR_ANALOG);

    if (val < THRESHOLD) {
      if (!eggDetected) {
        confirmCount++;

        if (confirmCount >= REQUIRED_COUNT) {
          eggCount++;
          eggDetected = true;
          confirmCount = 0;

          Serial.print("Yumurta Algılandı! Toplam: ");
          Serial.println(eggCount);

          // Adafruit IO'ya veri gönder
          eggFeed->save(eggCount);

          updateDisplay();
        }
      }
    } else {
      confirmCount = 0;
      eggDetected = false;
    }
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("EGG COUNT");

  display.setTextSize(4);
  display.setCursor(0, 30);
  display.println(eggCount);
  display.display();
}