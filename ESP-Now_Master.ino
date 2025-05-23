/*

	    Heating System Monitor  << ESP Now --Master>> May 6, 2025 @ 05:41 EDT
	
 	              ESP-Now --Master code.
		
		    Developed by Wlliam Lucid in Colaboration with Microsoft's Copilot,
		         OpenAI's ChstGPT, and Google's Gemini   

                Project uses 3- ESP32 DevKit V1 Boards, 1- BME280, 1- MCP9808, 1- MLX90614, 1- MAX9914

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

Ticker secondTicker;

FTPServer ftpSrv(LittleFS);

volatile unsigned long lastISRTime = 0;  // For interrupts

bool blowerOn = false;
bool blowerIsOn = false;   // Current state
bool blowerWasOn = false;  // For edge detection
unsigned long blowerStartTime = 0;
unsigned long blowerElapsedTime = 0;
unsigned long lastEventMinutes = 0;
unsigned long blowerDailyTotal = 0;

volatile bool oneSecondElapsed = false;

void IRAM_ATTR countSecondsISR() {
  oneSecondElapsed = true;
}

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

String GOOGLE_SCRIPT_ID = "removed";  //Deployment ID: Double

//master MAC address
uint8_t masterAddress[] = { 0x3C, 0xE9, 0x0E, 0x84, 0xEE, 0x80 };  // Replace with receiver's MAC address

// First peer (BME280 sender)
uint8_t slave1Address[] = { 0xE4, 0x65, 0xB8, 0x25, 0x42, 0xF8 };  // Replace with actual MAC

// Second peer (Blower sound detector)
uint8_t slave2Address[] = { 0x30, 0x30, 0xF9, 0x33, 0x4B, 0x78 };  // Replace with actual MAC

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

float elapsedMinutes = 0;
float dailyTotalMinutes = 0;
float secondsCounter = 0;

HTTPClient http;


//const char *serverURL = "http://httpbin.org/post";  //Server for testing

//Interrupt routine
volatile bool alertFlag = false;

enum MessageType : uint8_t {
  MSG_BME280 = 0,
  MSG_ALERT_FLAG = 1,
  MSG_BLOWER_STATE = 2
};

float temp;
float hum;
float pres;

// Define variables to store incoming readings

struct BME280Data {
  MessageType type;
  float temp;
  float hum;
  float pres;
};

BME280Data localReadings;  // For readings from the local sensor
BME280Data data;           // For received values

BME280Data incomingReadings;

volatile float globalTemp;

struct __attribute__((packed)) AlertFlag {
  MessageType type;
  bool alert;
};

struct __attribute__((packed)) BlowerData {
  MessageType type;
  bool on;
};

//RTC Memory structure
struct RtcData {
  char timestamp[20];
  int cycleCount;
};

RtcData rtcData;

String keyValue;  //String of data sent to Google Sheets
String urlFinal;  //url for Googlesheets

void onSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

esp_now_peer_info_t peerInfo;

void onRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len <= 0) {
    Serial.println("Error: Empty or invalid message received.");
    return;
  }

  uint8_t type = incomingData[0];

  switch (type) {
    case MSG_BME280:
      if (len == sizeof(BME280Data)) {
        BME280Data data;
        memcpy(&data, incomingData, sizeof(data));
        incomingReadings = data;  // Save latest readings globally

        Serial.println("Updated BME280Readings:");
        Serial.print("Temperature: ");
        Serial.println(data.temp);
        Serial.print("Humidity: ");
        Serial.println(data.hum);
        Serial.print("Pressure: ");
        Serial.println(data.pres * 0.02952998751);  // hPa → inHg

        globalTemp = data.temp;
        // Update the global variable
      } else {
        Serial.println("BME280 message size mismatch");
      }
      break;


    case MSG_ALERT_FLAG:
      {
        if (len == sizeof(AlertFlag)) {
          AlertFlag receivedAlert;
          memcpy(&receivedAlert, incomingData, sizeof(AlertFlag));
          Serial.print("Alert flag received: ");
          Serial.println(receivedAlert.alert ? "TRUE" : "FALSE");
        } else {
          Serial.println("Error: Incorrect AlertFlag size.");
        }
        break;
      }

    case MSG_BLOWER_STATE:
      if (len == sizeof(BlowerData)) {
        BlowerData blower;
        memcpy(&blower, incomingData, sizeof(BlowerData));
        Serial.print("Blower on: ");
        Serial.println(blower.on ? "YES" : "NO");

        // ✅ Update the global flag used in loop()
        blowerIsOn = blower.on;

      } else {
        Serial.print("Invalid BlowerData size. Received: ");
        Serial.println(len);
      }
      break;
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
  float thermostat;
  float lastEventMinutes;
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

float heatStartTime = 0;  // Declare dailyStartTime globally
bool heatActive = false;  // Declare dailyActive globally

//float registerTemp = sensordata.registerTemp;  // Read register temperature
float LOW_TEMP_F = 65.0;   // Example in °F
float HIGH_TEMP_F = 74.0;  // Example in °F

// Convert to Celsius
float LOW_TEMP_C = (LOW_TEMP_F - 32) / 1.8;
float HIGH_TEMP_C = (HIGH_TEMP_F - 32) / 1.8;

float registerTemp = 65;           // Initialize to a default temperature
float simulatedroomRegister = 65;  // Matching initialization for simulation

void updateHeatingData() {
  //Replace with actual sdata.tempngs                         //Store slave's outsideTemp
  sensordata.outsideTemp = globalTemp;               //bme280 temperature
  sensordata.insideTemp = mlx.readAmbientTempF();    //ambient temperature
  sensordata.registerTemp = mlx.readObjectTempF();   //Register temperature
  sensordata.thermostat = 70.0;                 //themostaticSetpoint (fixed value)
  sensordata.lastEventMinutes = lastEventMinutes;        //heat event runtime in minutes
  sensordata.dailyTotalMinutes = dailyTotalMinutes;  //total daily runtime in minutes
}

void resetDailySum() {               // Reset at midnight
  sensordata.dailyTotalMinutes = 0;  //dailyTotalMinutes for day posted to Google Sheets
  Serial.println("Total daily minutes reset for new day.");
}

void displayData() {
  getDateTime();
  Serial.println("\n----- Heating System Monitor -----");
  Serial.print(dtStamp);
  Serial.printf("\nOutside.temp: %.2f°F\n", globalTemp);
  Serial.printf("Inside Temp: %.2f°F\n", sensordata.insideTemp);
  Serial.printf("Register Temp: %.2f°F\n", sensordata.registerTemp);
  Serial.printf("Thermostat: %.2f°F\n", sensordata.thermostat);
  Serial.printf("Elapsed time of event (minutes) : %d\n", lastEventMinutes);
  Serial.printf("Total elapsed time (24 hr): %d\n",  dailyTotalMinutes);
  Serial.println("----------Resets at Midnight---------");
}

void sendData(bool alertFlag) {
  AlertFlag alertData;
  alertData.type = MSG_ALERT_FLAG;
  alertData.alert = alertFlag;

  esp_err_t result = esp_now_send(slave1Address, (uint8_t *)&alertData, sizeof(AlertFlag));

  if (result == ESP_OK) {
    Serial.print("Alert flag sent successfully: ");
    Serial.println(alertFlag);
  } else {
    Serial.print("Error sending alert flag: ");
    Serial.println(esp_err_to_name(result));
  }
}


void sendDataToServer(String lastUpdate, float outsideTemp, float insideTemp, float registerTemp, float thermostat, float lastEventMinutes, float dailyTotalMinutes) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi!");
    return;
  }

  // Construct the URL
  getDateTime();  // Fetch the formatted date-time string
  Serial.print(dtStamp);
  String keyValue = "&lastUpdate=" + dtStamp
                    + "&outsideTemp=" + globalTemp
                    + "&insideTemp=" + mlx.readAmbientTempF()
                    + "&registerTemp=" + mlx.readObjectTempF()
                    + "&thermostat=" + sensordata.thermostat
                    + "&elapsedMinutes=" + sensordata.lastEventMinutes
                    + "&dailyTotalMinutes=" + sensordata.dailyTotalMinutes;

  Serial.println(keyValue);

  String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + keyValue;
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

unsigned long previousMillis = 0;      // Stores the last time the temperature was updated
const unsigned long interval = 10000;  // Interval of 10 seconds

void setup() {
  Serial.begin(115200);
  while (!Serial) {};

  Serial.println("\n\n\nHeating System Monitor --ESP-Now Master\n\n");

  while (!Serial) {};
  randomSeed(analogRead(0));
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  // Set WiFi to Station mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi STA mode set");

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

  //LittleFS.format();

  // setup the ftp server with username and password
  // ports are defined in FTPCommon.h, default is
  //   21 for the control connection
  //   50009 for the data connection (passive mode by default)
  ftpSrv.begin(F("admin"), F("admin"));  //username, password for ftp.

  secondTicker.attach(1, countSecondsISR);

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
  esp_now_register_send_cb(onSent);

  // Register peer
  memcpy(peerInfo.peer_addr, slave1Address, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer 1");
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, slave2Address, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(slave2Address)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add Blower peer");
    } else {
      Serial.println("Blower peer added.");
    }
  }


  // Register for a callback function that will be called when sensordata is received
  esp_now_register_recv_cb(esp_now_recv_cb_t(onRecv));

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

bool googleSheetsSent = false;  // Flag to track if data has been sent

void loop() {
  for (int x = 1; x < 5000; x++)
  {
    ftpSrv.handleFTP();    
  }

  static bool lastBlowerState = false;  // Track previous blower state

  if (oneSecondElapsed) {
    oneSecondElapsed = false;

    // Count time only if blower is on
    if (blowerIsOn) {
      secondsCounter++;
      elapsedMinutes = secondsCounter / 60;

      // Debug
      Serial.print("Seconds: ");
      Serial.print(secondsCounter);
      Serial.print("  Elapsed min: ");
      Serial.print(elapsedMinutes);
      Serial.print("  Daily total: ");
      Serial.println(dailyTotalMinutes);
    }

    if (lastBlowerState && !blowerIsOn) {
      // Blower just turned OFF
      lastEventMinutes = elapsedMinutes;  // Save for Google Sheets
      dailyTotalMinutes += lastEventMinutes;

      Serial.printf("Blower OFF. Added %lu min to daily total. New total: %lu min\n",
                    lastEventMinutes, dailyTotalMinutes);

      updateHeatingData();
      alertFlag = true;

      // Reset event counters
      elapsedMinutes = 0;
      secondsCounter = 0;
    }

    static bool didMidnightReset = false;

    if (HOUR == 0 && MINUTE == 0 && SECOND == 0) {
      if (!didMidnightReset) {
        dailyTotalMinutes = 0;
        Serial.println("Midnight reset: dailyTotalMinutes = 0");
        didMidnightReset = true;
      }
    } else {
      didMidnightReset = false;  // Ready for the next midnight
    }

    // Update last state
    lastBlowerState = blowerIsOn;
  }

  // Condition for checking if it's time to send data to Google Sheets
  if (alertFlag) {
    alertFlag = true;
    sendData(alertFlag);  // Request BME280 outside temperature
    delay(1000);
    displayData();
    logData();
    sendGoogleSheetsData();  // Send to Google Sheets

    googleSheetsSent = true;  // Flag Google Sheets data as sent
    alertFlag = false;
  }

  // Reset elapsedSeconds after sending data to Google Sheets
  if (googleSheetsSent) {
    secondsCounter = 0; // Reset the seconds counter after Google Sheets update
    lastEventMinutes = 0; 
    googleSheetsSent = false;  // Reset flag for next cycle
  }
}

void resetDailyStats() {
  //dailyEventTotalMinutes = 0.0;
  //dailyEventCount = 0;
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
  log.print("\tOutsideTemp:  ");
  log.print(globalTemp, 2);
  log.print(" °F,  ");
  log.print("\tInside Temp: ");
  log.print(sensordata.insideTemp, 2);
  log.print(" °F,  ");
  log.print("\tThermostat: ");
  log.print(thermostatSetpoint, 2);
  log.print(" °F,  ");
  log.print("\tElapsed Time of event:  ");
  log.print(sensordata.lastEventMinutes, 2);
  log.print(", \tTotal Runtime (24 HR): ");
  log.print(sensordata.dailyTotalMinutes, 2);
  log.print("\n");

  log.close();
  Serial.println("Log written successfully.");
}

void sendGoogleSheetsData() {
  if(dailyTotalMinutes < 1){
    return;  //skip sending to Google sheet
  }

  getDateTime();
  // Collect the sensor sensordata
  String lastUpdate = dtStamp;  //lastUpdate global String used to store dtStamp
  float outsideTemp = globalTemp;
  float insideTemp = sensordata.insideTemp;
  float registerTemp = sensordata.registerTemp;
  float thermostat = sensordata.thermostat;
  float lastEventMinutes = lastEventMinutes;
  float dailyTotalMinutes = dailyTotalMinutes;
  Serial.println("\nData sent to Google Sheets!");
  //Send sensordata data to Google Sheets for logging
  sendDataToServer(lastUpdate, outsideTemp, insideTemp, registerTemp, thermostat, elapsedMinutes, dailyTotalMinutes);
}
