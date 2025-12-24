#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// Access Point configuration
const char* AP_SSID = "ESP32-RFID-Controller";
const char* AP_PASSWORD = "laundry123";  // Minimum 8 characters

// Server configuration
const char* SERVER_URL = "http://192.168.4.4:5000/scan_card";

// RFID reader pins
#define RST_PIN 22
#define SS_PIN 5
#define SCK_PIN 18
#define MOSI_PIN 23
#define MISO_PIN 19

// Relay pin (changed to avoid conflict with RST_PIN)
#define RELAY_PIN 4

// Timing constants
const unsigned long CARD_COOLDOWN_MS = 3000;  // 3 seconds between same card scans
const unsigned long RELAY_PULSE_DURATION = 350;  // Relay pulse duration in ms
const float BACKOFF_BASE = 1.5;
const unsigned long BACKOFF_MAX_MS = 30000;

// Coin deduction settings
const int COINS_PER_SCAN = 1;  // Default coins to deduct per scan
const unsigned long RAPID_SCAN_WINDOW = 5000;  // 5 seconds for consecutive scans

// Global objects
MFRC522 rfid(SS_PIN, RST_PIN);

// State variables
String lastCard = "";
unsigned long lastCardTime = 0;
unsigned long lastSuccessfulScanTime = 0;
int rapidScanCount = 0;
bool relayActive = false;
unsigned long relayStartTime = 0;

// Function declarations
bool setupAccessPoint();
bool sendCardToServer(String cardId, int coins = COINS_PER_SCAN);
String readCardOnce();
void activateRelay();
void updateRelay();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32 RFID Laundry System ===");
  Serial.println("Mode: Access Point with Relay Control");
  
  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Start with relay OFF (active-low)
  Serial.println("Relay initialized (OFF)");
  
  // Initialize SPI with custom pins
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Initialize RFID reader
  rfid.PCD_Init();
  Serial.println("MFRC522 initialized");
  
  // Show RFID reader details
  rfid.PCD_DumpVersionToSerial();
  
  // Set up Access Point
  if (!setupAccessPoint()) {
    Serial.println("Failed to setup Access Point!");
    while(1) { delay(1000); }  // Halt if AP fails
  }
  
  Serial.println("\nSetup complete. Ready to scan cards.");
  Serial.println("Clients can connect to: " + String(AP_SSID));
  Serial.println("Server URL: " + String(SERVER_URL));
  Serial.println("Each scan deducts " + String(COINS_PER_SCAN) + " coin(s)");
}

void loop() {
  unsigned long now = millis();
  
  // Update relay state (for timed deactivation)
  updateRelay();
  
  // Try to read a card
  String card = readCardOnce();
  
  if (card.length() > 0) {
    // Check if it's a new card or cooldown has passed
    if (card != lastCard || (now - lastCardTime) >= CARD_COOLDOWN_MS) {
      
      Serial.println("\nCard detected: " + card);
      
      // Check for rapid consecutive scans
      if (card == lastCard && (now - lastSuccessfulScanTime) <= RAPID_SCAN_WINDOW) {
        rapidScanCount++;
        Serial.print("Rapid scan detected! Count: ");
        Serial.println(rapidScanCount);
      } else {
        rapidScanCount = 1;
      }
      
      // Calculate coins to deduct (could increase for rapid scans)
      int coinsToDeduct = COINS_PER_SCAN;
      if (rapidScanCount > 1) {
        coinsToDeduct = COINS_PER_SCAN * rapidScanCount;
        Serial.print("Deducting ");
        Serial.print(coinsToDeduct);
        Serial.println(" coin(s) for rapid usage");
      }
      
      // Send card to server with coin deduction
      if (sendCardToServer(card, coinsToDeduct)) {
        Serial.println("Transaction successful");
        lastCard = card;
        lastCardTime = now;
        lastSuccessfulScanTime = now;
      } else {
        Serial.println("Transaction failed");
        rapidScanCount = 0;  // Reset on failure
      }
    } else {
      Serial.println("Same card - waiting for cooldown period");
    }
  }
  
  delay(100);  // 100ms delay between scans
}

bool setupAccessPoint() {
  Serial.print("Setting up Access Point: ");
  Serial.println(AP_SSID);
  
  // Configure as Access Point
  WiFi.mode(WIFI_AP);
  
  // Set up the Access Point
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Failed to create Access Point!");
    return false;
  }
  
  delay(1000);
  
  Serial.println("Access Point created successfully!");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  return true;
}

bool sendCardToServer(String cardId, int coins) {
  float backoff = BACKOFF_BASE;
  int maxRetries = 3;
  int retryCount = 0;
  
  while (retryCount < maxRetries) {
    HTTPClient http;
    
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload with card ID and coins
    StaticJsonDocument<256> doc;
    doc["card_id"] = cardId;
    doc["coins"] = coins;
    doc["machine_id"] = "laundry_machine_1";  // Optional: Identify which machine
    // there is no rapid scan
    String jsonString;
    serializeJson(doc, jsonString);
    
    Serial.println("Sending to server: " + jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("Server response: ");
      Serial.println(response);
      
      // Parse JSON response
      StaticJsonDocument<1024> responseDoc;
      DeserializationError error = deserializeJson(responseDoc, response);
      
      if (!error) {
        // bool success = responseDoc["success"] | false; there is no success key in server-side
        bool userExists = responseDoc["user_exists"] | false;
        const char* message = responseDoc["message"] | "No message";
        
        // Serial.print("Transaction success: ");
        // Serial.println(success ? "YES" : "NO");
        Serial.print("User exists: ");
        Serial.println(userExists ? "YES" : "NO");
        Serial.print("Message: ");
        Serial.println(message);
        
        if (userExists) {//&& success ekle sonra
          // Check if we should activate relay
          bool shouldActivate = responseDoc["activate_machine"] | false;
          int balance = responseDoc["balance"] | 0;
          int coinsUsed = responseDoc["coins_used"] | coins;
          
          Serial.print("Balance: ");
          Serial.println(balance);
          Serial.print("Coins used: ");
          Serial.println(coinsUsed);
          activateRelay();
          // if (shouldActivate) {
          //   Serial.println("Activating laundry machine...");
          //   activateRelay();
          // } else {
          //   Serial.println("Insufficient balance or not authorized");
          // }
        } else {
          Serial.println("Transaction not successful - no relay activation");
        }
      } else {
        Serial.println("Failed to parse JSON response");
      }
      
      http.end();
      return true;  // Server responded, even if transaction failed
      
    } else {
      Serial.print("HTTP Error code: ");
      Serial.println(httpResponseCode);
      Serial.println("Error: " + http.errorToString(httpResponseCode));
      http.end();
      
      // Exponential backoff
      unsigned long sleepTime = min((unsigned long)(backoff * 1000), BACKOFF_MAX_MS);
      Serial.print("Retrying in ");
      Serial.print(sleepTime / 1000);
      Serial.println(" seconds...");
      
      delay(sleepTime);
      backoff = min(backoff * 2.0f, (float)BACKOFF_MAX_MS / 1000);
      retryCount++;
    }
  }
  
  Serial.println("Max retries reached, giving up");
  return false;
}

String readCardOnce() {
  // Look for new cards
  if (!rfid.PICC_IsNewCardPresent()) {
    return "";
  }
  
  // Select one of the cards
  if (!rfid.PICC_ReadCardSerial()) {
    return "";
  }
  
  // Read UID
  String cardId = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      cardId += "0";
    }
    cardId += String(rfid.uid.uidByte[i], HEX);
  }
  
  cardId.toUpperCase();
  
  // Halt PICC
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
  
  return cardId;
}

void activateRelay() {
  if (relayActive) {
    Serial.println("Relay already active");
    return;
  }
  
  Serial.println("\n=== ACTIVATING RELAY ===");
  digitalWrite(RELAY_PIN, LOW);   // Relay ON (active-low)
  relayActive = true;
  relayStartTime = millis();
  
  Serial.print("Relay ON for ");
  Serial.print(RELAY_PULSE_DURATION);
  Serial.println(" ms");
  Serial.println("Laundry machine should start now...");
}

void updateRelay() {
  if (relayActive) {
    unsigned long now = millis();
    if (now - relayStartTime >= RELAY_PULSE_DURATION) {
      digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
      relayActive = false;
      Serial.println("\nRelay OFF - Pulse completed");
      Serial.println("Laundry cycle activated");
    }
  }
}

// Optional: Manual relay test function (call from setup for testing)
void testRelay() {
  Serial.println("\n=== Testing Relay ===");
  for (int i = 0; i < 3; i++) {
    Serial.print("Test pulse ");
    Serial.println(i + 1);
    digitalWrite(RELAY_PIN, LOW);
    delay(RELAY_PULSE_DURATION);
    digitalWrite(RELAY_PIN, HIGH);
    delay(1000);
  }
  Serial.println("Relay test complete\n");
}