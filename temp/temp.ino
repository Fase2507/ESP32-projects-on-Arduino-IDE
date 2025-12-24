
/* //Button and led 
#define BUTTON_PIN 21 // BUTTON
#define LED_PIN 18 // led 

int btn_state = 0;


void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  btn_state = digitalRead(BUTTON_PIN);
  if (btn_state == LOW)
    digitalWrite(LED_PIN, HIGH);
  else
    digitalWrite(LED_PIN, LOW);
}
*/