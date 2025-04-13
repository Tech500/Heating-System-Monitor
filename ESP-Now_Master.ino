/*

	    Heating System Monitor  << ESP Now --Master>> April 12, 2025 @ 17:26 EST
	
 	              ESP-Now code for Outside Temperature; BME280 sensor. 
		
		    Developed by Wlliam Lucid in Colaboration with Microsoft's Copilot,
		         OpenAI's ChstGPT, and Google's Gemini   

                Project uses 2- ESP32 DevKit V1 Boards, 1- BME280, 1- MCP9808, 1- MLX90614

*/


#include <Wire.h>
#include "mcp9808.h"
#include "Adafruit_MLX90614.h"
#include <esp_now.h>
#include <WiFi.h> 
#include <esp_wifi.h>
#include <WiFiUdp.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <FTPServer.h>
#include <time.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <HTTPClient.h>

Ticker oneMinuteTicker;

FTPServer ftpSrv(LittleFS);

volatile unsigned long lastISRTime = 0;  // For interrupts

unsigned long lastRequestTime = 0;   // For timers
bool dataReadyFlag = false;          // Timer flag
float dailyEventTotalMinutes = 0.0;  // Daily runtime
int dailyEventCount = 0;             // Daily event count

String getDateTime();
float readSensorTemperature();
void enabledaily();
void enableCooling();
void disabledailyCooling();
void resetDailyStats();
void processRequest();
void calculateAverageRuntime();
void sendGoogleSheetsData();
void simulateTemperatureChange();
void checkdailyStatus();


// WiFi credentials
const char *ssid = "R2D2";
const char *password = "sissy4357";


WiFiUDP udp;
// local port to listen for UDP packets
//const int udpPort = 1337;
char incomingPacket[255];
char replyPacket[] = "Hi there! Got the message :-)";
const char *udpAddress1 = "pool.ntp.org";
const char *udpAddress2 = "time.nist.gov";

String GOOGLE_SCRIPT_ID = "AKfycbzHopYwRo0G4NAiIIMmiWVUlUJPBycbyDzkx2AaVSJTN8691DIfSjqd4h4I4UAxhMkv";  //Deployment ID: Double

//master MAC address
uint8_t masterAddress[] = { 0x3C, 0xE9, 0x0E, 0x84, 0xEE, 0x80 };  // Replace with receiver's MAC address

//slaveMAC address
uint8_t slaveAddress[] = { 0xE4, 0x65, 0xB8, 0x25, 0x42, 0xF8 };  // Replace with your slave's MAC address

WiFiClient client;

///Are we currently connected?
boolean connected = false;

//WiFiUDP udp;
// Define NTP client to get time
WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP, "us.pool.ntp.org", -18000, 0);

//#define TZ "EST5EDT,M3.2.0/2,M11.1.0/2"

int DOW, MONTH, DATE, YEAR, HOUR, MINUTE, SECOND;
//String = (Sunday, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday};
String weekDay;

int lc = 0;
time_t tnow;

char strftime_buf[64];

String dtStamp(strftime_buf);

String lastUpdate;  //Store dtStamp for String data

HTTPClient http;


//const char *serverURL = "http://httpbin.org/post";  //Server for testing

//Interrupt routine
volatile bool alertFlag = false;


float temp;
float hum;
float pres;

// Define variables to store incoming readings



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
  boolean alertFlag;
};

struct struct_send sendAlert;

//RTC Memory structure
struct RtcData {
  char timestamp[20];
  int cycleCount;
};

RtcData rtcData;


String data;      //String of data sent to Google Sheets
String urlFinal;  //url for Googlesheets

void onSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

esp_now_peer_info_t peerInfo;

//Callback when sensordata is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("\n\nLast Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last packet sent Status: ");  //status???
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

/// Callback when sensordata is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message1)) {                              // Check for BME280 sensor readings
    memcpy(&BME280Readings, incomingData, sizeof(struct_message1));  // Update global struct

    // Debug: Print BME280 readings
    Serial.println("Updated BME280Readings:");
    Serial.print("Temperature: ");
    Serial.println(BME280Readings.temp);
    Serial.print("Humidity: ");
    Serial.println(BME280Readings.hum);
    Serial.print("Pressure: ");
    Serial.println(BME280Readings.pres * 0.02952998751);
  } else if (len == sizeof(struct_send)) {                  // Check for alertFlag data
    memcpy(&sendAlert, incomingData, sizeof(struct_send));  // Update global alertFlag

    // Debug: Print the alert flag
    Serial.print("Received alertFlag: ");
    Serial.println(sendAlert.alertFlag);
  } else {  // Catch-all for mismatched data sizes
    Serial.println("Received data size mismatch.");
  }
}

void sendAlertFlag() {
  sendAlert.alertFlag = alertFlag;
  esp_err_t result = esp_now_send(slaveAddress, (uint8_t *)&sendAlert, sizeof(sendAlert));
  if (result == ESP_OK) {
    Serial.println("\nAlert Flag Sent Successfully!");
  } else {
    Serial.print("\nError sending Alert Flag: ");
    Serial.println(esp_err_to_name(result));
  }
}

MCP9808 ts(24);

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Define the alert pin, interrupt number, and relay pin
#define ALERT_PIN 2
#define ALERT_PIN_INTERRUPT 2

String getDateTime() {
  struct tm *ti;

  tnow = time(nullptr) + 1;
  ti = localtime(&tnow);
  DOW = ti->tm_wday;
  YEAR = ti->tm_year + 1900;
  MONTH = ti->tm_mon + 1;
  DATE = ti->tm_mday;
  HOUR = ti->tm_hour;
  MINUTE = ti->tm_min;
  SECOND = ti->tm_sec;
  strftime(strftime_buf, sizeof(strftime_buf), "%a %m %d %Y  %H:%M:%S", localtime(&tnow));  //Removed %Z
  dtStamp = strftime_buf;
  dtStamp.replace(" ", "-");
  return (dtStamp);
}

struct heatingData {
  float outsideTemp;
  float insideTemp;
  float registerTemp;
  float extraThermoTemp;
  float elapsedMinutes;
  float dailyTotalMinutes;
  float lastRecordedMinute;
};

heatingData sensordata;

bool checkTemp = false;
;

volatile bool heatAlert = false;

void IRAM_ATTR heatISR() {
  heatAlert = true;
}


void configTime() {

  configTime(0, 0, udpAddress1, udpAddress2);
  setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 2);  // this sets TZ to Indianapolis, Indiana
  tzset();


  Serial.print("wait for first valid timestamp\n");

  while (time(nullptr) < 100000ul) {
    Serial.print(".");
    delay(5000);
  }

  //Serial.println("\nSystem Time set\n");

  getDateTime();

  Serial.println(dtStamp);
}


// Thermostat setpoint (update as needed)
float thermostatSetpoint = 70.0;  // 70°F in Celsius

float elapsedMinutes = 0;
float dailyTotalMinutes = 0;  // Declare dailyTotalMinutes globally
float heatStartTime = 0;      // Declare dailyStartTime globally
bool heatActive = false;      // Declare dailyActive globally

//float registerTemp = sensordata.registerTemp;  // Read register temperature
float LOW_TEMP_F = 65.0;   // Example in °F
float HIGH_TEMP_F = 74.0;  // Example in °F

// Convert to Celsius
float LOW_TEMP_C = (LOW_TEMP_F - 32) / 1.8;
float HIGH_TEMP_C = (HIGH_TEMP_F - 32) / 1.8;

float registerTemp = 65;           // Initialize to a default temperature
float simulatedroomRegister = 65;  // Matching initialization for simulation

void updateHeatingData() {
  //Replace with actual sensor readings                         //Store slave's outsideTemp
  sensordata.outsideTemp = BME280Readings.temp;           //bme280 temperature
  sensordata.insideTemp = mlx.readAmbientTempF();         //ambient temperature
  sensordata.registerTemp = mlx.readObjectTempF();        //Register temperature
  sensordata.extraThermoTemp = 70.0;                      //themostaticSetpoint (fixed value)
  sensordata.elapsedMinutes = elapsedMinutes;  //heat event runtime in minutes
  sensordata.dailyTotalMinutes += dailyTotalMinutes;
}


void onMinute() {   //Ticher.h one minute Interrupt
  if (registerTemp != thermostatSetpoint) {
    // Your code here to execute every minute
    registerTemp++;
    elapsedMinutes++;
    Serial.print("\nRegisterTemp:  ");
    Serial.println(registerTemp);
    Serial.print("Elapsed Minutes (min): ");
    Serial.println(elapsedMinutes);
    if (registerTemp == thermostatSetpoint) {
      dailyTotalMinutes += elapsedMinutes;
      Serial.print("DailyTotalMinutes:  ");
      Serial.println(dailyTotalMinutes);
      Serial.println("❄️ Heating is OFF");
      alertFlag = true;
    }
  }
}

void checkdailyStatus() {
  if (heatAlert || (!heatActive && registerTemp < thermostatSetpoint)) {
    Serial.println("\nHeat Alert triggered or daily needed!");

    if (!heatActive && registerTemp < thermostatSetpoint) {
      heatActive = true;
      Serial.println("\n🔥Heat is ON");
      onMinute();  //Ticker.h increament by one minute

    }

    if (heatActive && registerTemp >= thermostatSetpoint) {
      heatActive = false;
      Serial.println("❄️ daily OFF");
      Serial.print("Elapsed Minutes (min): ");
      Serial.println(elapsedMinutes);
      Serial.print("Total daily (min): ");
      Serial.println(dailyTotalMinutes);
    }
  }
}

void resetDailySum() {               // Reset at midnight
  sensordata.dailyTotalMinutes = 0;  //dailyTotalMinutes for day posted to Google Sheets
  Serial.println("Total daily minutes reset for new day.");
}

void displayData() {
  getDateTime();
  Serial.println("\n----- Heating System Monitor -----");
  Serial.print(dtStamp);
  Serial.printf("\nOutside Temp: %.2f°F\n", BME280Readings.temp);
  Serial.printf("Inside Temp: %.2f°F\n", sensordata.insideTemp);
  Serial.printf("Register Temp: %.2f°F\n", sensordata.registerTemp);
  Serial.printf("ThermostatSetpoint: %.2f°F\n", sensordata.extraThermoTemp);
  Serial.printf("Elapsed time of event (minutes) : %d\n", sensordata.elapsedMinutes);
  Serial.printf("Total elapsed time (24 hr): %d\n", sensordata.dailyTotalMinutes);
  Serial.println("----------Resets at Midnight---------");
}

void sendDataToServer(String lastUpdate, float outsideTemp, float insideTemp, float registerTemp, float extraThermoTemp, float elapsedMinutes, float dailyTotalMinutes) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi!");
    return;
  }

  // Construct the URL
  getDateTime();  // Fetch the formatted date-time string
  Serial.print(dtStamp);
  String data = "&lastUpdate=" + dtStamp
                + "&outsideTemp=" + BME280Readings.temp
                + "&insideTemp=" + mlx.readAmbientTempF()
                + "&registerTemp=" + mlx.readObjectTempF()
                + "&extraThermoTemp=" + sensordata.extraThermoTemp
                + "&elapsedMinutes=" + sensordata.elapsedMinutes
                + "&dailyTotalMinutes=" + sensordata.dailyTotalMinutes;

  Serial.println(data);

  String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + data;
  Serial.print("POST data to spreadsheet:");
  urlFinal.replace(" ", "%20");
  Serial.println(urlFinal);
  HTTPClient http;
  http.begin(urlFinal.c_str());
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  Serial.print("\nHTTP Status Code: ");
  Serial.println(httpCode);

  //getting response from google sheet
  String payload;
  if (httpCode > 0) {
    payload = http.getString();
    Serial.println("Payload: " + payload);
  }
  http.end();
}

#define SCL 22
#define SDA 21

#define CHANNEL 4

// Define the alert pin, interrupt number, and relay pin
#define ALERT_PIN 2
#define ALERT_PIN_INTERRUPT 2

int connect = 0;

String macAddr = WiFi.macAddress();

// Read temperature sensordata from MLX90614
float ambientTemp = 0;
float objectTemp = 0;

// Variables to track daily system state, cycles, and runtime
bool dailyOn = false;
//unsigned long dailyStartTime = 0;  //Prviously defined 206
unsigned long totalRuntime = 0;
int cycleCount = 0;
unsigned long lastLogTime = 0;
const unsigned long logInterval = 86400000;  // 24 hours in milliseconds

bool flag = true;

float simulatedAmbient = 65.0;  // Initialize temperature globally
//float simulatedroomRegister;
const float minTemperature = 65.0;
const float maxTemperature = 75.0;

void simulateTemperatureChange() {
  if (alertFlag) {
    delay(200);
    simulatedroomRegister = 65;            // Random temperature range
    registerTemp = simulatedroomRegister;  // Update global `registerTemp`

    Serial.print("\nregisterTemp: ");
    Serial.println(registerTemp);

    checkdailyStatus();
  }
}

unsigned long previousMillis = 0;      // Stores the last time the temperature was updated
const unsigned long interval = 10000;  // Interval of 10 seconds

void setup() {
  Serial.begin(115200);
  while (!Serial) {};

  Serial.println("\n\n\nHeating System Monitor --ESP-Now Master\n\n");

  oneMinuteTicker.attach(60.0, onMinute);  // Attach a ticker to trigger every 60 seconds

  while (!Serial) {};
  randomSeed(analogRead(0));
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("Local IP: " + WiFi.localIP().toString());
  //Serial.println("MAC Address: " + WiFi.macAddress());
  // This is the mac address of the Master in Station Mode
  Serial.print("STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("STA CHANNEL ");
  Serial.println(WiFi.channel());

  configTime();

  bool fsok = LittleFS.begin(true);
  Serial.printf_P(PSTR("FS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  LittleFS.format();

  // setup the ftp server with username and password
  // ports are defined in FTPCommon.h, default is
  //   21 for the control connection
  //   50009 for the data connection (passive mode by default)
  ftpSrv.begin(F("admin"), F("admin"));  //username, password for ftp.

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

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  Serial.println("Peer added successfully\n\n");

  // Register for a callback function that will be called when sensordata is received
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // Set thresholds
  ts.setTlower(LOW_TEMP_C);
  ts.setTupper(HIGH_TEMP_C);
  ts.setTcritical(23);

  pinMode(ALERT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ALERT_PIN), heatISR, FALLING);

  // SET ALERT PARAMETERS
  uint16_t cfg = ts.getConfigRegister();
  cfg &= ~0x0001;  // set comparator mode
  // cfg &= ~0x0002;      // set polarity HIGH
  cfg |= 0x0002;   // set polarity LOW
  cfg &= ~0x0004;  // use upper lower and critical
  cfg |= 0x0008;   // enable alert
  ts.setConfigRegister(cfg);

  if (!mlx.begin(0x5A)) {
    Serial.println("Error connecting to MLX sensor. Check wiring.");
    //while (1) {};
  }
}

void loop() {
  if (heatAlert) {
    heatAlert = false;
    Serial.println("🔥 Heating event detected via ALERT pin!");
    checkdailyStatus();
  }

  checkdailyStatus();

  if(alertFlag){
    alertFlag = true;
    sendAlertFlag();  //Request BME280 outside temperature
    delay(1000);
    updateHeatingData();
    displayData();
    logData();
    sendGoogleSheetsData();
    alertFlag = false;
    elapsedMinutes = 0;
    checkdailyStatus();
  }
}

void resetDailyStats() {
  dailyEventTotalMinutes = 0.0;
  dailyEventCount = 0;
  disabledailyCooling();  //Turn off daily/cooling systems
  Serial.println("Daily stats reset.");
}

void IRAM_ATTR temperatureInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastISRTime > 50) {  // Debounce logic
    alertFlag = 30;
    lastISRTime = currentTime;
  }
}

void logData() {

  // Check if file exists, create if not
  if (!LittleFS.exists("/log.txt")) {
    Serial.println("File does not exist, creating...");
    File log = LittleFS.open("/log.txt", FILE_WRITE);
    if (!log) {
      Serial.println("Failed to create file!");
      return;
    }
    log.close();
  }

  // Open log file for appending
  File log = LittleFS.open("/log.txt", FILE_APPEND);
  if (!log) {
    Serial.println("File 'log.txt' open failed!");
    return;
  }

  Serial.println("Logging data...");

  log.print(dtStamp);
  log.print(" , ");
  log.print("\tOutsideTemp: ");
  log.print(BME280Readings.temp, 2);
  log.print(" °F,  ");
  log.print("\tInside Temp: ");
  log.print(sensordata.insideTemp, 2);
  log.print(" °F,  ");
  log.print("\tThermostat: ");
  log.print(thermostatSetpoint, 2);
  log.print(" °F,  ");
  log.print("\tElapsed Time of event:  ");
  log.print(elapsedMinutes, 2);
  log.print(", \tTotal Runtime (24 HR): ");
  log.print(dailyTotalMinutes, 2);
  log.print("\n");

  log.close();
  Serial.println("Log written successfully.");
}

void sendGoogleSheetsData() {
  getDateTime();
  // Collect the sensor sensordata
  String lastUpdate = dtStamp;  //lastUpdate global String used to store dtStamp
  float outsideTemp = BME280Readings.temp;
  float insideTemp = sensordata.insideTemp;
  float registerTemp = sensordata.registerTemp;
  float extraThermoTemp = sensordata.extraThermoTemp;
  float elapsedMinutes = elapsedMinutes;
  float dailyTotalMinutes = dailyTotalMinutes;
  Serial.println("\nData sent to Google Sheets!");
  //Send sensordata data to Google Sheets for logging
  sendDataToServer(lastUpdate, outsideTemp, insideTemp, registerTemp, extraThermoTemp, elapsedMinutes, dailyTotalMinutes);
}
