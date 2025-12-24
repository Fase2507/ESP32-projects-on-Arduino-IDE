#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// WiFi credentials
const char* SSID = "fase";
const char* PASSWORD = "12345678";

// Server configuration
const char* SERVER_URL = "http://172.16.3.50:5000/scan_card";

// RFID reader pins
#define RST_PIN 22
#define SS_PIN 5
#define SCK_PIN 18
#define MOSI_PIN 23
#define MISO_PIN 19
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
bool connectWiFi(int timeout = 20);
bool sendCardToServer(String cardId, int coins);  // Fixed declaration
String readCardOnce();
void activateRelay();
void updateRelay();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32 RFID Reader Starting ===");
  
  // Initialize SPI with custom pins
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Initialize RFID reader
  rfid.PCD_Init();
  Serial.println("MFRC522 initialized");
  
  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Start with relay OFF (active-low)
  Serial.println("Relay initialized (OFF)");
  
  // Show RFID reader details
  rfid.PCD_DumpVersionToSerial();
  
  // Connect to WiFi
  if (!connectWiFi()) {
    Serial.println("Initial WiFi connection failed, will retry in loop");
  }
  
  Serial.println("Setup complete. Ready to scan cards.");
}

void loop() {
  unsigned long now = millis();
  
  // Update relay state (for timed deactivation)
  updateRelay();
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi(10);
    delay(2000);
    return;
  }
  
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

bool connectWiFi(int timeout) {
  // Check if already connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected to WiFi");
    return true;
  }
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < timeout) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nWiFi connection failed!");
    return false;
  }
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

// Activate Relay
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

bool sendCardToServer(String cardId, int coins) {  // Fixed function name
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
        bool success = responseDoc["success"] | false;
        bool userExists = responseDoc["user_exists"] | false;
        const char* message = responseDoc["message"] | "No message";
        
        Serial.print("Transaction success: ");
        Serial.println(success ? "YES" : "NO");
        Serial.print("User exists: ");
        Serial.println(userExists ? "YES" : "NO");
        Serial.print("Message: ");
        Serial.println(message);
        
        if (success && userExists) {
          // Check if we should activate relay
          bool shouldActivate = responseDoc["activate_machine"] | false;
          int balance = responseDoc["balance"] | 0;
          int coinsUsed = responseDoc["coins_used"] | coins;
          
          Serial.print("Balance: ");
          Serial.println(balance);
          Serial.print("Coins used: ");
          Serial.println(coinsUsed);
          
          if (shouldActivate) {
            Serial.println("Activating laundry machine...");
            activateRelay();
          } else {
            Serial.println("Insufficient balance or not authorized");
          }
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

// Optional: Manual relay test function
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