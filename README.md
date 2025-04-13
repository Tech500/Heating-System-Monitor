
# Heating System Monitor

An ESP-NOW-based solution to monitor heating system activity without direct integration. This project logs heating events to a Google Sheets spreadsheet, providing a simple and effective way to track heating usage over time.

## Overview

This project utilizes two ESP32 microcontrollers communicating via ESP-NOW protocol to detect and log heating system activity. The system operates independently of the heating system's internal controls, making it a non-intrusive monitoring solution.

## Features

- **Wireless Monitoring**: Employs ESP-NOW for efficient, low-latency communication between devices.
- **Non-Invasive Setup**: Monitors heating activity without requiring direct integration with the heating system.
- **Data Logging**: Records heating events to a Google Sheets spreadsheet for easy access and analysis.
- **Visual Indicators**: Includes LED indicators to display system status and activity.

## Components

- **ESP-Now_Master.ino**: Firmware for the master ESP32 device responsible for detecting heating activity and sending data.
- **ESP-Now_Slave.ino**: Firmware for the slave ESP32 device that receives data from the master and processes it.
- **Google Script --Heating System Monitor.txt**: Google Apps Script to handle incoming data and log it to Google Sheets.
- **Heating System Monitor GS.jpg**: Diagram illustrating the Google Sheets setup.
- **Heating System Monitor.mp4**: Demonstration video showcasing the system in operation.
- **putty.log**: Sample log file capturing serial output for debugging purposes.

## Setup Instructions

### Hardware Setup

Project used 2- ESP32 Devkit v1 Development boards, on the Master MCP9808 Presicion temperature sensor with interrup pin, and a MLX90614 Infrared temperature sensor.  Outside Slave used a BME280. 

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

Once set up, the master device will detect heating system activity and send a signal to the slave device via ESP-NOW. The slave device will then send BME280data to the Master; which, will send 
lastest updated project data in the form of key and value --in the form of a String, named "data" to Google Apps Script web app, which logs the event to the Monthly; perpetual, Google Sheet. You can monitor heating 
activity in real-time by viewing the Google Sheet.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.  README.md --created by ChatGPT, edited by and project created by William Lucid, Retired Computer Specialist.  Project collabation 
from lead Assistant, ChatGPT; with assists from Geminni, and Copilot.

---

For more information, visual demonstrations, and troubleshooting, please refer to the included `Heating System Monitor.mp4` video and `putty.log` file.

