/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-multiple-button
 */

#include <ezButton.h>

#define BUTTON_PIN_1 18  // The ESP32 pin GPIO25 connected to the button 1
#define BUTTON_PIN_2 19  // The ESP32 pin GPIO26 connected to the button 2
#define BUTTON_PIN_3 21  // The ESP32 pin GPIO27 connected to the button 3
  // The ESP32 pin GPIO14 connected to the button 4

ezButton button1(BUTTON_PIN_1);  // create ezButton object for button 1
ezButton button2(BUTTON_PIN_2);  // create ezButton object for button 2
ezButton button3(BUTTON_PIN_3);  // create ezButton object for button 3

void setup() {
  Serial.begin(115200);
  button1.setDebounceTime(100);  // set debounce time to 100 milliseconds
  button2.setDebounceTime(100);  // set debounce time to 100 milliseconds
  button3.setDebounceTime(100);  // set debounce time to 100 milliseconds
}

void loop() {
  button1.loop();  // MUST call the loop() function first
  button2.loop();  // MUST call the loop() function first
  button3.loop();  // MUST call the loop() function first

  // get button state after debounce
  int button1_state = button1.getState();  // the state after debounce
  int button2_state = button2.getState();  // the state after debounce
  int button3_state = button3.getState();  // the state after debounce

  /*
  Serial.print("The button 1 state: ");
  Serial.println(button1_state);
  Serial.print("The button 2 state: ");
  Serial.println(button2_state);
  Serial.print("The button 3 state: ");
  Serial.println(button3_state);
  Serial.print("The button 4 state: ");
  Serial.println(button4_state);
  */

  if (button1.isPressed())
    Serial.println("The button 1 is pressed");

  if (button1.isReleased())
    Serial.println("The button 1 is released");

  if (button2.isPressed())
    Serial.println("The button 2 is pressed");

  if (button2.isReleased())
    Serial.println("The button 2 is released");

  if (button3.isPressed())
    Serial.println("The button 3 is pressed");

  if (button3.isReleased())
    Serial.println("The button 3 is released");

}
