/*

	    Heating System Monitor  << ESP Now --Slave>> April 12, 2025 @ 17:26 EST
	
 	              ESP-Now code for Outside Temperature; BME280 sensor. 
		
		    Developed by Wlliam Lucid in Colaboration with Microsoft's Copilot,
		         OpenAI's ChstGPT, and Google's Gemini   

*/

 
#include <Wire.h>
#include <BME280I2C.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// WiFi credentials
const char *ssid = "R2D2";
const char *password = "sissy4357";

volatile bool alertFlag = false;

BME280I2C bme;

//master MAC address
uint8_t masterAddress[] = { 0x3C, 0xE9, 0x0E, 0x84, 0xEE, 0x80 };  // Replace with receiver's MAC address

//slaveMAC address
uint8_t slaveAddress[] = { 0xE4, 0x65, 0xB8, 0x25, 0x42, 0xF8 };  // Replace with your slave's MAC address

#define CHANNEL 4

String macAddr = WiFi.macAddress();

#define SEALEVELPRESSURE_HPA (1013.25)

float temp;
float hum;
float pres;

// Define variables to store incoming readings
float incomingTemp;
float incomingHum;
float incomingPres;

//Hold BME280 sensor readings
typedef struct struct_message1 {
  float temp;  // Temperature
  float hum;   // Humidity
  float pres;  // Pressure
};

struct_message1 BME280Readings;

//incming holding for sensor readings
typedef struct struct_message2 {
  float incomingTemp;  // Temperature
  float incomingHum;   // Humidity
  float incomingPres;  // Pressure
};

struct_message2 incomingReadings;

// Define alertFlag sending
struct struct_send {
  bool alertFlag;
};

struct struct_send sendAlert;

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

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // NTP client with UTC time

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
    if (len == sizeof(struct_send)) {
        struct_send tempSendAlert;
        memcpy(&tempSendAlert, incomingData, sizeof(struct_send));
        alertFlag = tempSendAlert.alertFlag;
        Serial.print("Received Alert Flag: ");
        Serial.println(alertFlag);
    } else {
        Serial.println("Error: Unexpected data size received for alertFlag.");
    }
}

void sendData() {
    esp_err_t result = esp_now_send(masterAddress, (uint8_t *)&BME280Readings, sizeof(BME280Readings));
    if (result == ESP_OK) {
        Serial.println("BME280 Data Sent Successfully!");
    } else {
        Serial.print("Error sending BME280 Data: ");
        Serial.println(esp_err_to_name(result));
    }
}


void setup() {
  Serial.begin(115200);
  while(!Serial){};

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

  // Initialize NTP client
  timeClient.begin();

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
  if(alertFlag == true){
    alertFlag = false;
    BME280();
    updateDisplay();
    delay(1000);
  }
}

void BME280() {
  //Declare variables for BME280sensor readings
  float temp = NAN, hum = NAN, pres = NAN;

  // Read data from BME280 sensor
  BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);

  bme.read(pres, temp, hum, tempUnit, presUnit);

  //BME280 Requires time to read data
  delay(500);


  if (temp == NAN || hum == NAN || pres == NAN) {
    Serial.println("Error reading BME280 data");
    return;  // Skip sending if sensor readings fail
  }

  // Set values to send
  BME280Readings.temp = temp;
  BME280Readings.hum = hum;
  BME280Readings.pres = pres;

  // Send message via ESP-NOW
  sendData();
}

//Display BME280 sensor readingd
void updateDisplay() {
  Serial.print("\n\nTemperature: ");
  Serial.print(BME280Readings.temp);
  Serial.println(" ºF");
  Serial.print("Humidity: ");
  Serial.print(BME280Readings.hum);
  Serial.println(" %");
  Serial.print("Pressure: ");
  Serial.print((BME280Readings.pres) * 0.02952998751);
  Serial.print(" inHg");
  Serial.println();
}