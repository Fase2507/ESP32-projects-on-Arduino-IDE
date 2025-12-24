#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// WiFi credentials
const char* SSID = "fase";
const char* PASSWORD = "12345678";

// Server configuration
const char* SERVER_URL = "http://172.23.235.134:5000/scan_card"; 

// RFID reader pins
#define RST_PIN 22
#define SS_PIN 5
#define SCK_PIN 18
#define MOSI_PIN 23
#define MISO_PIN 19

// Relay pin (Assuming Active-LOW relay module)
#define RELAY_PIN 4

// Timing constants
const unsigned long SAME_CARD_DELAY_MS = 3000;       // Min time card must be removed before counting as a new "coin"
const unsigned long RAPID_SCAN_WINDOW_MS = 5000;    // Total time window for multi-coin counting
const unsigned long RELAY_PULSE_MS = 150;           // Relay pulse duration
const float BACKOFF_BASE = 1.5;
const unsigned long BACKOFF_MAX_MS = 10000;        // Max backoff for HTTP retries

// Coin settings
const int BASE_COINS = 1;          // Base coins per scan
const int RAPID_MULTIPLIER = 1;    // Additional coins for the second, third, etc., scans in the window

// Global objects
MFRC522 rfid(SS_PIN, RST_PIN);

// State variables
String lastCard = "";
unsigned long lastTransactionTime = 0; // Time of the last successful server transaction
unsigned long lastCardDetectedTime = 0; // Time card was last seen (for cooldown)
int currentCoinCount = 0; // The total number of coins requested in the current rapid scan window
bool relayActive = false;
unsigned long relayStartTime = 0;

// Function declarations
bool connectWiFi(int timeout = 20);
bool sendCardToServer(String cardId, int coins);
String readCardOnce();
void activateRelay();
void updateRelay();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32 RFID Multi-Coin System ===");
  
  // Initialize relay (Set HIGH for OFF if Active-LOW)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); 
  Serial.println("Relay initialized (OFF)");
  
  // Initialize SPI with custom pins
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Initialize RFID reader
  rfid.PCD_Init();
  Serial.println("MFRC522 initialized");
  rfid.PCD_DumpVersionToSerial();
  
  // Connect to WiFi
  if (!connectWiFi()) {
    Serial.println("Initial WiFi connection failed, will retry in loop");
  }
  
  Serial.println("\nSetup complete. Ready to scan cards.");
}

void loop() {
  unsigned long now = millis();
  
  // Must be called frequently to turn the relay OFF after the pulse
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
    
    // 1. Check if the card is different from the last card
    bool isNewCard = (card != lastCard);
    
    // 2. Check if the same card has been removed long enough to reset the count
    bool cooldownPassed = (now - lastCardDetectedTime) >= SAME_CARD_DELAY_MS;
    
    // 3. Check if the rapid scan window has expired
    bool windowExpired = (now - lastTransactionTime) > RAPID_SCAN_WINDOW_MS;
    
    // --- DETERMINE COIN COUNT ---
    if (isNewCard || cooldownPassed || windowExpired) {
      // This is a new transaction (new card, or old card with cooldown/timeout)
      currentCoinCount = BASE_COINS;
      lastCard = card;
      Serial.println("\n══════════════════════════════════");
      Serial.println("Card detected: " + card + " (New Transaction)");
    } else if (card == lastCard && (now - lastTransactionTime) < RAPID_SCAN_WINDOW_MS) {
      // This is a rapid scan of the same card within the window
      currentCoinCount += RAPID_MULTIPLIER;
      Serial.print("\n[RAPID SCAN] Current coins: ");
      Serial.println(currentCoinCount);
    } else {
      // Cooldown is still active
      Serial.println("Same card - waiting for cooldown period");
      lastCardDetectedTime = now; // Update time to extend cooldown if card is held
      return;
    }
    
    // --- PROCESS TRANSACTION ---
    Serial.print("Coins to deduct: ");
    Serial.println(currentCoinCount);
    
    if (sendCardToServer(card, currentCoinCount)) {
      Serial.println("✓ Transaction successful");
      lastTransactionTime = now;
    } else {
      Serial.println("✗ Transaction failed. Resetting coin count.");
      currentCoinCount = 0; // Reset count on failure
    }
    
    Serial.println("══════════════════════════════════\n");
    
  } else {
    // Card is no longer present, update the last detected time
    lastCardDetectedTime = now; 
  }
  
  delay(100); // Small delay between loops
}

// (connectWiFi function)
bool connectWiFi(int timeout) {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected to WiFi",WL_CONNECTED);
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


// (sendCardToServer function)
bool sendCardToServer(String cardId, int coins) {
  float backoff = BACKOFF_BASE;
  int maxRetries = 3; // Reduced retries for quicker user feedback
  int retryCount = 0;
  
  while (retryCount < maxRetries) {
    HTTPClient http;
    
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload with additional data for server logging/validation
    StaticJsonDocument<300> doc; // Increased buffer size
    doc["card_id"] = cardId;
    doc["coins_requested"] = coins;
    doc["machine_id"] = "laundry_machine_1"; // Static machine ID
    doc["is_rapid_scan"] = (coins > BASE_COINS);
        
    String jsonString;
    serializeJson(doc, jsonString);
    
    Serial.println("Sending to server: " + jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: "); Serial.println(httpResponseCode);
      
      StaticJsonDocument<512> responseDoc;
      DeserializationError error = deserializeJson(responseDoc, response);
      
      if (!error) {
        // bool success = responseDoc["success"] | false; //there is no success key in server-side
        bool shouldActivate = responseDoc["activate_machine"] | false;
        bool userExists = responseDoc["user_exists"] | false;
        Serial.print("Activate machine: "); Serial.println(shouldActivate ? "YES" : "NO");
            
        // Activate relay ONLY IF the server says so
        if (userExists) {
          activateRelay();
        }
        // Print all other details for debugging
        Serial.print("Message: "); Serial.println(responseDoc["message"] | "No message");
        if (responseDoc.containsKey("balance")) {
          Serial.print("Balance: "); Serial.println(responseDoc["balance"].as<int>());
        }
      } else {
        Serial.print("JSON parse error: "); Serial.println(error.c_str());
      }
      http.end();
      return true;
    } else {
      // (Error handling and exponential backoff)
      Serial.print("HTTP Error code: "); Serial.println(httpResponseCode);
      Serial.println("Error: " + http.errorToString(httpResponseCode));
      http.end();
      unsigned long sleepTime = min((unsigned long)(backoff * 1000), BACKOFF_MAX_MS);
      Serial.print("Retrying in "); Serial.print(sleepTime / 1000); Serial.println(" seconds...");
      delay(sleepTime);
      backoff = min(backoff * 2.0f, (float)BACKOFF_MAX_MS / 1000);
      retryCount++;
    }
  }
  Serial.println("Max retries reached, giving up");
  return false;
}

// (readCardOnce function)
String readCardOnce() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return "";
  }
  String cardId = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      cardId += "0";
    }
    cardId += String(rfid.uid.uidByte[i], HEX);
  }
  cardId.toUpperCase();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return cardId;
}

// (activateRelay function)
void activateRelay() {
  if (relayActive) {
    Serial.println("Relay already active, skipping");
    return;
  }
  Serial.println("=== ACTIVATING RELAY ===");
  digitalWrite(RELAY_PIN, LOW);   // Relay ON (active-low)
  relayActive = true;
  relayStartTime = millis();
  Serial.print("Relay ON for "); Serial.print(RELAY_PULSE_MS); Serial.println(" ms");
}

// (updateRelay function)
void updateRelay() {
  if (relayActive) {
    unsigned long now = millis();
    if (now - relayStartTime >= RELAY_PULSE_MS) {
      digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
      relayActive = false;
      Serial.println("Relay pulse completed");
    }
  }
}
