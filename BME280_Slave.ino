    Heating System Monitor  << ESP Now --BME280 Slave>> May 6, 2025 @ 04:06 EDT
	
 	              ESP-Now code for Outside Temperature; BME280 sensor. 

  		   Developed by Wlliam Lucid in Colaboration with Microsoft's Copilot,
		         OpenAI's ChstGPT, and Google's Gemini   

                Project uses 3- ESP32 DevKit V1 Boards, 1- BME280, 1- MCP9808, 1- MLX90614, 1- MAX9914
		   
*/


#include <Wire.h>
#include <BME280I2C.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WiFiUdp.h>

// WiFi credentials
const char *ssid = "Removed";
const char *password = "Removed";

volatile bool alertFlag = false;

BME280I2C bme;

//master MAC address
uint8_t masterAddress[] = { 0x3C, 0xE9, 0x0E, 0x84, 0xEE, 0x80 };  // Replace with receiver's MAC address

//slaveMAC address
uint8_t slaveAddress[] = { 0xE4, 0x65, 0xB8, 0x25, 0x42, 0xF8 };  // Replace with your slave's MAC address

#define CHANNEL 4

String macAddr = WiFi.macAddress();

#define SEALEVELPRESSURE_HPA (1013.25)


enum MessageType : uint8_t {
  MSG_BME280 = 0,
  MSG_ALERT_FLAG,
};

// Unified struct for BME280 readings
struct BME280Data {
  MessageType type = MSG_BME280;
  float temp = 0.0;
  float hum = 0.0;
  float pres = 0.0;
};

// Create global instances for local and received sensor data
BME280Data localReadings;
BME280Data receivedReadings;

BME280Data sensorData;

// Define alertFlag sending
struct __attribute__((packed)) AlertFlag {
  MessageType type;
  bool alert;
};

//struct struct_send sendAlert;

//RTC Memory structure
struct RtcData {
  char timestamp[20];
  int cycleCount;
};

RtcData rtcData;

volatile bool interruptFlag = false;
const int interruptPin = 2;  // Example interrupt pin

// Global copy of slave
esp_now_peer_info_t peerInfo;

//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // NTP client with UTC time

// Callback when data is from Slave to Master
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("\nLast Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

//Call back when data is receivied
void OnRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len <= 0) return;

  // Peek at the message type
  MessageType msgType = *(MessageType *)incomingData;

  switch (msgType) {
    case MSG_ALERT_FLAG:
      if (len == sizeof(AlertFlag)) {
        AlertFlag receivedAlert;
        memcpy(&receivedAlert, incomingData, sizeof(AlertFlag));
        alertFlag = receivedAlert.alert;
        Serial.print("\nReceived Alert Flag: ");
        Serial.println(alertFlag);
      } else {
        Serial.print("Invalid AlertFlag size. Received: ");
        Serial.println(len);
      }
      break;

    case MSG_BME280:
      if (len == sizeof(BME280Data)) {
        BME280Data received;
        memcpy(&received, incomingData, sizeof(BME280Data));
        receivedReadings = received;
        Serial.print("Received BME280 -> Temp: ");
        Serial.print(received.temp);
        Serial.print(" °C, Hum: ");
        Serial.print(received.hum);
        Serial.print(" %, Pres: ");
        Serial.print(received.pres);
        Serial.println(" hPa");
      } else {
        Serial.print("Invalid BME280Data size. Received: ");
        Serial.println(len);
      }
      break;

  default:
      Serial.print("Unknown message type: ");
      Serial.println(msgType);
      break;
  }
}


void setup() {
  Serial.begin(115200);
  while (!Serial) {};

  Serial.println("\nHeating System Monitor\n\n");

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

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_err_t result = esp_now_init();
  if (result == ESP_OK) {
    Serial.println("ESP-NOW initialized successfully");
  } else {
    Serial.print("ESP-NOW initialization failed, error code: ");
    Serial.println(result);
  }

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  Serial.println("Peer added successfully");

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnRecv));

  Wire.begin();

  // Start the BME280 send
  while (!bme.begin()) {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }
}

void loop() {
  if (alertFlag == true) {
    BME280();
    updateDisplay();
    delay(1000);    
  }
  alertFlag = false;
}

void BME280() {
  float temp = NAN, hum = NAN, pres = NAN;

  // Read data from BME280 sensor
  BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);

  bme.read(pres, temp, hum, tempUnit, presUnit);
  delay(500);  // Allow time for sensor

  // Check for failed readings
  if (isnan(temp) || isnan(hum) || isnan(pres)) {
    Serial.println("Error reading BME280 data");
    return;
  }

  // Fill struct with valid sensor readings
  sensorData.type = MSG_BME280;
  sensorData.temp = temp;
  sensorData.hum = hum;
  sensorData.pres = pres;

  // Send data via ESP-NOW
  esp_now_send(masterAddress, (uint8_t *)&sensorData, sizeof(BME280Data));
}

//Display BME280 sensor readingd
void updateDisplay() {
  Serial.print("\n\nTemperature: ");
  Serial.print(sensorData.temp);
  Serial.println(" ºF");
  Serial.print("Humidity: ");
  Serial.print(sensorData.hum);
  Serial.println(" %");
  Serial.print("Pressure: ");
  Serial.print((sensorData.pres) * 0.02952998751);
  Serial.print(" inHg");
  Serial.println();
}
