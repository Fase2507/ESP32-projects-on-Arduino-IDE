#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
const unsigned long SAMPLE_INTERVAL = 30; // ms

int confirmCount = 0;
int eggCount = 0;
bool eggDetected = false;

unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);

  // I2C başlat (ESP32 default)
  Wire.begin(21, 22);

  // OLED başlat
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("OLED bulunamadi"));
    while (true); // dur
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("EGG COUNT");
  display.display();
}

void loop() {
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

          Serial.print("Egg detected! Total: ");
          Serial.println(eggCount);

          // ---- OLED GÜNCELLE ----
          updateDisplay();
        }
      }
    } else {
      confirmCount = 0;
      eggDetected = false;
    }
  }
}

// -------- OLED GÖSTERİM FONKSİYONU --------
void updateDisplay() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("EGG");

  display.setTextSize(3);
  display.setCursor(0, 30);
  display.println(eggCount);

  display.display();
}
