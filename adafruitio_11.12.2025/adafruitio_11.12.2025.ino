#define IO_USERNAME "Fase2507"
#define IO_KEY      "aio_Sqgp87ZvCfVb7PwrtPkQxB0uvr0j"

#define WIFI_SSID   "TECNO CAMON 18"
#define WIFI_PASS   "qazwsx12"

#define LED_PIN LED_BUILTIN   // ESP32 built-in LED

#include "AdafruitIO_WiFi.h"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

AdafruitIO_Feed *digital = io.feed("digital");

unsigned long lastCheck = 0;
const unsigned long interval = 5000; // 5 seconds

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  Serial.print("Connecting to Adafruit IO");
  io.connect();

  digital->onMessage(handleMessage);

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nConnected!");

  digital->get();
}

void loop() {
  io.run();  // sürekli çalışması zorunlu

  unsigned long now = millis();
  if (now - lastCheck >= interval) {
    lastCheck = now;
    Serial.println("5 saniyelik IO kontrol döngüsü çalıştı.");
  }
}

void handleMessage(AdafruitIO_Data *data) {

  int level = data->toPinLevel();
  Serial.print("Received: ");
  Serial.println(level == HIGH ? "HIGH" : "LOW");

  digitalWrite(LED_PIN, level);
}
