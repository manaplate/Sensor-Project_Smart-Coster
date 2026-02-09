#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ------------------- USER SETTINGS -------------------
//const char* ssid     = "TrueGigatexFiber_2.4G_vwv"; 
//const char* password = "nfSNu2xj"; 
const char* ssid     = "Goldfisb"; 
const char* password = "plasalmon";

const char* scriptURL = "https://script.google.com/macros/s/AKfycbxZP2MThwEjTbaNo6QTxhHTp52B6TZ5XIAF59nicunOujnwzWJamrCmdbfzkvQtQxKW9A/exec";

// GPIO 0 is usually the onboard BOOT button
const int buttonPin = 0;  
// -----------------------------------------------------

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;      
const int   daylightOffset_sec = 0; 

// Button Variables
int buttonState;             // Current stable state
int lastReading = HIGH;      // Previous raw reading
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 10; // Change sensitivity

// Timer Variables
unsigned long lastTime = 0;
unsigned long timerDelay = 60000/2; // Change update interval

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP); 

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){
    Serial.println("Syncing time...");
    delay(500);
  }
  Serial.println("Time Synced!");
  
  // Initialize button state
  buttonState = digitalRead(buttonPin);
}

void loop() {
  // Read button state 
  int reading = digitalRead(buttonPin);

  // If the switch changed, due to noise or pressing:
  if (reading != lastReading) {
    lastDebounceTime = millis(); // Reset the timer
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the reading has been there longer than the delay, take it as the actual state:
    if (reading != buttonState) {
      buttonState = reading;

      // Only toggle if the new state is LOW (Pressed)
      if (buttonState == LOW) {
        Serial.println("BUTTON PRESS! Resetting Data..."); // Debug message
        
        sendDataToGoogleSheet(true); // Send Reset
        
        lastTime = millis(); // Reset 60s timer
      }
    }
  }
  lastReading = reading;
  
  // Check Timer (Every 60 secs)
  if ((millis() - lastTime) > timerDelay) {
    Serial.println(String(timerDelay/60000) + " Minute Passed. Appending Data...");
    sendDataToGoogleSheet(false); // Send Append
    lastTime = millis(); 
  }
}

// Helper Function
String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Time Error";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// Main Send Function
void sendDataToGoogleSheet(bool reset) {
  if(WiFi.status() == WL_CONNECTED){
    WiFiClientSecure client;
    client.setInsecure(); 

    HTTPClient https;
    
    if (https.begin(client, scriptURL)) {
      https.addHeader("Content-Type", "application/json");

      String method = (reset) ? "reset" : "append";
      String currentTime = getFormattedTime();

      int placeholderReading = random(1, 100);
      
      String payload = "{\"method\": \"" + method + "\"," +
                        "\"timestamp\": \"" + currentTime + "\"," + 
                        "\"placeholderReading\": \"" + String(placeholderReading) + "\"}";

      Serial.print("Posting Payload: ");
      Serial.println(payload);

      int httpCode = https.POST(payload);

      if (httpCode == 302) {
        Serial.println("SUCCESS! (Data sent, ignoring redirect)");
      } 
      else if (httpCode == 200 || httpCode == 201) { 
        Serial.println("Success: " + https.getString());
      } 
      else {
        Serial.printf("Error Code: %d\n", httpCode);
      }
      https.end();
    } else {
      Serial.println("Unable to connect to Script URL");
    }
  } else {
    Serial.println("WiFi Disconnected");
  }
}