#include <SPI.h>
#include <MFRC522.h>

// -------PINS--------
#define RELAY_PIN 22

#define SS_PIN 5
#define RST_PIN 22
#define SCK_PIN 18
#define MOSI_PIN 23
#define MISO_PIN 19

#define RELAY_PULSE 350
#define CARD_COOLDOWN 3000

// Rfid read
MFRC522 rfid(SS_PIN, RST_PIN);
// ------- VARIABLES --------
String lastCardUID = "";
unsigned long lastCardTime = 0;

const String authorizedCards[] = {
  "A1B2C3D4",  // Replace with actual card UIDs
  "E5F67890",
  ""
}; 

void setup() {
  Serial.begin(115200);
  while (!Serial);  // For boards with native USB
  
  Serial.println("\n=== RFID Relay Controller ===");
  
  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Start with relay OFF (assuming active-low)
  
  Serial.println("Relay initialized (OFF)");
  
  // Initialize SPI with custom pins
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Initialize RFID reader
  rfid.PCD_Init();
  Serial.println("RFID Reader initialized");
  
  // Optional: Print RFID reader details
  Serial.print("MFRC522 firmware version: 0x");
  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(v, HEX);
  if (v == 0x91) {
    Serial.println(" (v1.0)");
  } else if (v == 0x92) {
    Serial.println(" (v2.0)");
  } else {
    Serial.println(" (unknown)");
  }
  
  Serial.println("\nReady to scan RFID cards...");
  Serial.println("Relay will activate for authorized cards");
}

void loop() {
  // Check for new RFID card
  String currentCard = readRFID();
  
  if (currentCard.length() > 0) {
    unsigned long now = millis();
    
    // Check if it's a new card or cooldown has passed
    if (currentCard != lastCardUID || (now - lastCardTime) >= CARD_COOLDOWN_MS) {
      
      Serial.print("Card detected: ");
      Serial.println(currentCard);
      
      // Check if card is authorized
      if (isCardAuthorized(currentCard)) {
        Serial.println("Card AUTHORIZED - Activating relay");
        activateRelay();
        lastCardUID = currentCard;
        lastCardTime = now;
      } else {
        Serial.println("Card NOT AUTHORIZED - No action");
        activateRelay()
      
        
        // Optional: Blink LED or sound buzzer for unauthorized card
      }
    } else {
      Serial.println("Same card detected - waiting for cooldown");
    }
  }
  
  // Small delay to prevent overwhelming the RFID reader
  delay(100);
}

String readRFID() {
  // Reset the loop if no new card present
  if (!rfid.PICC_IsNewCardPresent()) {
    return "";
  }
  
  // Verify if the NUID has been read
  if (!rfid.PICC_ReadCardSerial()) {
    return "";
  }
  
  // Read card UID
  String cardUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    // Add leading zero for values less than 0x10
    if (rfid.uid.uidByte[i] < 0x10) {
      cardUID += "0";
    }
    cardUID += String(rfid.uid.uidByte[i], HEX);
  }
  
  cardUID.toUpperCase();
  
  // Halt PICC
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
  
  return cardUID;
}

bool isCardAuthorized(String cardUID) {
  // Check if card is in the authorized list
  for (int i = 0; i < sizeof(authorizedCards) / sizeof(authorizedCards[0]); i++) {
    if (authorizedCards[i] == cardUID) {
      return true;
    }
  }
  
  // Optional: Always allow specific cards (for testing)
  // Uncomment the line below to allow ALL cards (for testing only!)
  // return true;
  
  return false;
}

void activateRelay() {
  Serial.println("Activating relay...");
  
  // Activate relay (pulse LOW for active-low relay)
  digitalWrite(RELAY_PIN, LOW);   // Relay ON
  Serial.print("Relay ON for ");
  Serial.print(RELAY_PULSE_DURATION);
  Serial.println(" ms");
  
  delay(RELAY_PULSE_DURATION);
  
  digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
  Serial.println("Relay OFF");
  
  Serial.println("Relay pulse completed");
}

// Optional: Manual relay test function
void testRelay() {
  Serial.println("\n=== Testing Relay ===");
  for (int i = 0; i < 3; i++) {
    Serial.print("Pulse ");
    Serial.println(i + 1);
    digitalWrite(RELAY_PIN, LOW);
    delay(RELAY_PULSE_DURATION);
    digitalWrite(RELAY_PIN, HIGH);
    delay(1000);
  }
  Serial.println("Test complete");
}