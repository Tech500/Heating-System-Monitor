#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "Filters.h"

// WiFi credentials
const char* ssid = "Removed";
const char* password = "Removed";

// Master MAC address
uint8_t masterAddress[] = { 0x3C, 0xE9, 0x0E, 0x84, 0xEE, 0x80 };

// Microphone and thresholds
const int micPin = 34;
const int sampleWindowMs = 50;
//too high negeglected signal loss from filtering
//const float thresholdOn = 1.6;
//const float thresholdOff = 0.7;
//New thesholds
const float thresholdOn = 0.800;
const float thresholdOff = 0.600;

const unsigned long confirmDuration = 1000;

struct FilteredWindowResult {
  float signalMin;
  float signalMax;
  float peakToPeak;
  float lastFiltered;
};

// Filter parameters
const float lowerCutoff = 500.0;
const float upperCutoff = 12000.0;

// Filter objects
FilterTwoPole highPassFilter;
FilterTwoPole lowPassFilter;

// Blower state tracking
bool lastSoundState = false;
bool pendingState = false;
unsigned long lastStateChange = 0;
bool blowerOn = false;
bool blowerSentOn = false;
bool blowerSentOff = false;

// ESP-NOW Message
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

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void sendData(bool state) {
  BlowerData blower;
  blower.type = MSG_BLOWER_STATE;
  blower.on = state;

  esp_err_t result = esp_now_send(masterAddress, (uint8_t*)&blower, sizeof(BlowerData));
  if (result == ESP_OK) {
    Serial.println("Blower data sent successfully");
  } else {
    Serial.print("Error sending blower data: ");
    Serial.println(result);
  }

  Serial.print("Sent blower state: ");
  Serial.println(state ? "ON" : "OFF");
}

FilteredWindowResult sampleFilteredWindow(int analogPin, int durationMs) {
  FilteredWindowResult result;
  result.signalMin = 3.3;
  result.signalMax = 0.0;
  result.lastFiltered = 0.0;

  unsigned long startMillis = millis();
  while (millis() - startMillis < durationMs) {
    int raw = analogRead(analogPin);
    float voltage = raw * (3.3 / 4095.0);
    float highPass = highPassFilter.input(voltage);
    float filtered = lowPassFilter.input(highPass);
    result.lastFiltered = filtered;

    if (filtered < result.signalMin) result.signalMin = filtered;
    if (filtered > result.signalMax) result.signalMax = filtered;
  }

  result.peakToPeak = result.signalMax - result.signalMin;
  return result;
}

int consecutiveOnWindows = 0;
int consecutiveOffWindows = 0;
const int requiredWindows = 5;  // Change as needed

bool detectSound() {
  FilteredWindowResult res = sampleFilteredWindow(micPin, sampleWindowMs);
  float peakToPeak = res.peakToPeak;

  Serial.print("Filtered P2P: ");
  Serial.print(peakToPeak, 3);
  Serial.print(" | Min: ");
  Serial.print(res.signalMin, 3);
  Serial.print(" | Max: ");
  Serial.println(res.signalMax, 3);

  if (!blowerOn && peakToPeak >= thresholdOn) {
    consecutiveOnWindows++;
    consecutiveOffWindows = 0;  // reset opposite counter
    if (consecutiveOnWindows >= requiredWindows) {
      blowerOn = true;
      Serial.println("Blower Detected: ON");
      sendData(true);
      consecutiveOnWindows = 0;
    }
  } 
  else if (blowerOn && peakToPeak <= thresholdOff) {
    consecutiveOffWindows++;
    consecutiveOnWindows = 0;
    if (consecutiveOffWindows >= requiredWindows) {
      blowerOn = false;
      Serial.println("Blower Detected: OFF");
      sendData(false);
      consecutiveOffWindows = 0;
      delay(500);
    }
  } 
  else {
    // neither condition met, reset both counters
    consecutiveOnWindows = 0;
    consecutiveOffWindows = 0;
  }

  return blowerOn;
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("Blower Bandpass Detector Started");

  // Filter setup
  highPassFilter.setAsFilter(LOWPASS_BUTTERWORTH, lowerCutoff, 0);
  highPassFilter.IsHighpass = true;
  lowPassFilter.setAsFilter(LOWPASS_BUTTERWORTH, upperCutoff, 0);

  // WiFi setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("Local IP: " + WiFi.localIP().toString());

  // ESP-NOW init
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
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

  if (currentSoundState && !lastSoundState && !blowerSentOn) {
    sendData(true);
    blowerSentOn = true;
  }

  if (!currentSoundState && lastSoundState && !blowerSentOff) {
    sendData(false);
    blowerSentOff = true;
  }

  if (currentSoundState != lastSoundState) {
    blowerSentOn = false;
    blowerSentOff = false;
  }

  lastSoundState = currentSoundState;

  delay(500);
}
