
# Heating System Monitor

An ESP-NOW-based solution to monitor heating system activity without direct integration. This project logs heating events to a Google Sheets spreadsheet, providing a simple and effective way to track heating usage over time.

## Overview

This project utilizes three ESP32 microcontrollers communicating via ESP-NOW protocol; one to detect sound and  one for outside tempture and and one to consoildate communications, plus logging heating system activity. 
The system operates independently of the heating system's internal controls, making it a non-intrusive monitoring solution.  Optional hardware on Master; MCP9808 temperature sensor with alert pin, MLX90614 Infrared temperature sensor --have switched from temperature sensing to sound detection for sound of blower starting and stopping.  Outside ESP32, Slave; one BME280 used for sensing temperature.  Second ESP32 is used for blower sound detection --with DSP bandpass filtering allowing blower sound frequency range to pass.

## Features  

- **Wireless Monitoring**: Employs ESP-NOW for efficient, low-latency communication between devices.
- **Non-Invasive Setup**: Monitors heating activity without requiring direct integration with the heating system.
- **Data Logging**: Records heating events to a Google Sheets spreadsheet and local LittleFS for easy access and analysis.  Built-in FTP for viewing ESP32 log file.
- **Visual Indicators**: Includes LED indicators to display system status and activity.

## Components

- **ESP-Now_Master.ino**: Firmware for the master ESP32 device responsible for detecting heating activity and sending data.
- **ESP-Now_Slave.ino**: Firmware for the slave ESP32 device that receives signal from the master and sends BME280 data to master.
- **Google Script --Heating System Monitor.txt**: Google Apps Script to handle incoming data and log it to Google Sheets.
- **Heating System Monitor GS.jpg**: Diagram illustrating the Google Sheets setup.
- **Heating System Monitor.mp4**: Demonstration video showcasing the system in operation.
- **putty.log**: Sample log file capturing serial output for debugging purposes.

## Setup Instructions

### Hardware Setup

Project used three- ESP32 Devkit v1 Development boards, on the Master MCP9808 Presicion temperature sensor with interrup pin, and a MLX90614 Infrared temperature sensor.  Outside Slave used a BME280. 

1. **Master Device**:
   - Connect the ESP32 to a sensor or input that can detect heating system activity (e.g., temperature sensor, current sensor).
   - Upload the `ESP-Now_Master.ino` firmware to the device.  

2. **Slave Device**:
   - Connect the ESP32 to a Wi-Fi network with internet access.
   - Upload the `ESP-Now_Slave.ino` firmware to the device.

### Software Setup

1. **Google Sheets**:
   - Create a new Google Sheet to store heating event data.
   - Open the Script Editor (`Extensions > Apps Script`).
   - Paste the contents of `Google Script --Heating System Monitor.txt` into the editor.
   - Deploy the script as a web app (`Deploy > New deployment`) and note the URL.
   - 📘 *Need help with perpetual access to the web app?* Check out the guide: [Perpetual Script Execution with Google Sheets](https://gist.github.com/Tech500/57fe6a035e46da4112d6330a637367d0)

2. **ESP32 Configuration**:
   - In the `ESP-Now_Slave.ino` file, update the `WEB_APP_URL` variable with the URL obtained from the Google Apps Script deployment.
   - Ensure both ESP32 devices are configured with the correct Wi-Fi credentials and are within range for ESP-NOW communication.

## Usage

Once set up, the master device cor-ordinates slave data and will detect heating system activity and send a signal to the slave devices via ESP-NOW. BME280 slave device will then send BME280data to the Master; which, will send 
lastest updated project data in the form of key and value --in the form of a String, named "data" to Google Apps Script web app, which logs the event to the Monthly; perpetual, Google Sheet. You can monitor heating 
activity in real-time by viewing the Google Sheet.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.  README.md --created by ChatGPT, edited by and project created by William Lucid, Retired Computer Specialist.  Project collabation 
from lead Assistant, ChatGPT; with assists from Geminni, and Copilot.

---

For more information, visual demonstrations, and troubleshooting, please refer to the included `Heating System Monitor.mp4` video and `putty.log` file.  

Project was a collabitive effort by ChatGPT, Gemini, Copilot and William Lucid, Tech500.

