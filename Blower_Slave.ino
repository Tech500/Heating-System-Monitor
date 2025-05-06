/*

	    Heating System Monitor  << ESP Now --Blower Slave>> May 6, 2025 @ 04:06 EDT
	
 	              ESP-Now code for Sound Detection. 

  		   Developed by Wlliam Lucid in Colaboration with Microsoft's Copilot,
		         OpenAI's ChstGPT, and Google's Gemini   

                Project uses 3- ESP32 DevKit V1 Boards, 1- BME280, 1- MCP9808, 1- MLX90614, 1- MAX9914
		
*/


#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

// WiFi credentials
const char *ssid = "Removed";
const char *password = "Removed";

#define CHANNEL 4

// Master MAC address (replace with actual address)
uint8_t masterAddress[] = { 0x3C, 0xE9, 0x0E, 0x84, 0xEE, 0x80 };

// Microphone settings
const int micPin = 34;
const int sampleWindowMs = 50;
const int thresholdOn = 2000;
const int thresholdOff = 900;
const unsigned long confirmDuration = 1000;

// Sound detection state
bool lastSoundState = false;
bool pendingState = false;
unsigned long lastStateChange = 0;
bool blowerOn = false;
bool blowerSentOn = false;
bool blowerSentOff = false;

enum MessageType : uint8_t {
  MSG_BME280 = 0,
  MSG_ALERT_FLAG = 1,
  MSG_BLOWER_STATE = 2
};

struct __attribute__((packed)) BlowerData {
  MessageType type;
  bool on;
};


esp_now_peer_info_t peerInfo;

// ESP-NOW send callback
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Sound detection logic
// Function that detects sound and sends the blower state
bool detectSound() {
  unsigned long startMillis = millis();
  int signalMin = 4095;
  int signalMax = 0;

  while (millis() - startMillis < sampleWindowMs) {
    int sample = analogRead(micPin);
    if (sample < signalMin) signalMin = sample;
    if (sample > signalMax) signalMax = sample;
  }

  int peakToPeak = signalMax - signalMin;
  Serial.print("PeakToPeak: ");
  Serial.println(peakToPeak);

  unsigned long now = millis();

  // Blower ON detection
  if (!blowerOn && peakToPeak >= thresholdOn) {
    if (!pendingState) {
      pendingState = true;
      lastStateChange = now;
    } else if (now - lastStateChange >= confirmDuration) {
      blowerOn = true;
      pendingState = false;
      Serial.println("Blower Detected: ON");
    }
  }

  // Blower OFF detection
  else if (blowerOn && peakToPeak <= thresholdOff) {
    if (!pendingState) {
      pendingState = true;
      lastStateChange = now;
    } else if (now - lastStateChange >= confirmDuration) {
      blowerOn = false;
      pendingState = false;
      sendData(blowerOn);
      Serial.println("Blower Detected: OFF");
      delay(500);
    }
  }

  return blowerOn;
}

// Function that sends the blower state to the master
void sendData(bool state) {
  BlowerData blower;
  blower.type = MSG_BLOWER_STATE;
  blower.on = state;  // Correctly set the blower state

  esp_err_t result = esp_now_send(masterAddress, (uint8_t *)&blower, sizeof(BlowerData));
  if (result == ESP_OK) {
    Serial.println("Blower data sent successfully");
  } else {
    Serial.print("Error sending blower data: ");
    Serial.println(result);
  }

  Serial.print("Sent blower state: ");
  Serial.println(state ? "ON" : "OFF");
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("Blower sound detector started...");

  WiFi.mode(WIFI_STA);  // Use AP+STA mode for better ESP-NOW stability
  Serial.print("Wi-Fi Mode: ");
  Serial.println(WiFi.getMode());  // Check current mode

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("Local IP: " + WiFi.localIP().toString());
  //Serial.println("MAC Address: " + WiFi.macAddress());
  // This is the mac address of the Master in Station Mode
  Serial.print("STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("STA CHANNEL ");
  Serial.println(WiFi.channel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  bool currentSoundState = detectSound();

  if (currentSoundState == true && lastSoundState == false) {
    if (!blowerSentOn) {
      sendData(true);  // Send ON state
      blowerSentOn = true;
    }
  }

  if (currentSoundState == false && lastSoundState == true) {
    if (!blowerSentOff) {
      sendData(false);  // Send OFF state
      blowerSentOff = true;
    }
  }

  if (currentSoundState != lastSoundState) {
    blowerSentOn = false;
    blowerSentOff = false;
  }

  lastSoundState = currentSoundState;

  delay(500);  // Small delay to prevent spamming the master
}
