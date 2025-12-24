#define IR_ANALOG 34

const int THRESHOLD = 50;
const int REQUIRED_COUNT = 5;
const unsigned long SAMPLE_INTERVAL = 30; // ms

int confirmCount = 0;
int eggCount = 0;
bool eggDetected = false;

unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = now;

    int val = analogRead(IR_ANALOG);

    // Serial.println(val);

    if (val < THRESHOLD) {
      if (!eggDetected) {
        confirmCount++;

        if (confirmCount >= REQUIRED_COUNT) {
          eggCount++;
          eggDetected = true;   // aynı yumurtayı kilitle
          confirmCount = 0;

          Serial.print("Egg detected! Total: ");
          Serial.println(eggCount);
        }
      }
    } else {
      // yumurta geçti, sistem resetlendi
      confirmCount = 0;
      eggDetected = false;
    }
  }
}

// #define IR_ANALOG 34

// void setup() {
//   Serial.begin(115200);
// }
// int count = 0;
// int eggCount = 0;
// void loop() {
//   int val = analogRead(IR_ANALOG);
//   // Serial.println(val);
//   if(val<50){
//     count++;
//     if(count%5==0){
//       eggCount++;
//     }
//   }
//   Serial.println("Egg count: ");
//   Serial.print(eggCount);
// }

