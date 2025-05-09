Heating System Monitor
An ESP-NOW-based solution to monitor heating system activity without direct integration. This project logs heating events to a Google Sheets spreadsheet, providing a simple and effective way to track heating usage over time. 

Overview

This project uses three ESP32 microcontrollers communicating via ESP-NOW: one for outside temperature, one for blower sound detection (using Blower_Slave.ino), and one as the master to consolidate communications and log heating system activity. The system operates independently of the heating system’s internal controls, making it a non-intrusive monitoring solution.

Features

•	Wireless Monitoring: Employs ESP-NOW for efficient, low-latency communication between devices.
•	Non-Invasive Setup: Monitors heating activity without requiring direct integration with the heating system.
•	Data Logging: Records heating events to a Google Sheets spreadsheet and local LittleFS for easy access and analysis. Built-in FTP for viewing ESP32 log file.
•	Visual Indicators: Includes LED indicators to display system status and activity.

Components

•	ESP-Now_Master.ino: Firmware for the master ESP32 device responsible for consolidating data and sending it to Google Sheets.
•	ESP-Now_Slave.ino: Firmware for the slave ESP32 device that measures outside temperature using a BME280 sensor.
•	Blower_Slave.ino: Firmware for the second slave ESP32 detecting blower sound using DSP bandpass filtering.
•	Google Script --Heating System Monitor.txt: Google Apps Script to handle incoming data and log it to Google Sheets.
•	Heating System Monitor GS.jpg: Diagram illustrating the Google Sheets setup.
•	Heating System Monitor.mp4: Demonstration video showcasing the system in operation.
•	putty.log: Sample log file capturing serial output for debugging purposes.

Setup Instructions

  Hardware Setup
1.	Master Device:
o	Connect the ESP32 to a sensor for temperature (MCP9808) and optional MLX90614 infrared sensor.
o	Upload the ESP-Now_Master.ino firmware.
2.	Slave Device (Temperature):
o	Connect the ESP32 to a BME280 sensor for outside temperature measurement.
o	Upload the ESP-Now_Slave.ino firmware.
3.	Slave Device (Blower Sound Detection):
o	Set up a microphone with DSP bandpass filtering for blower sound detection.
o	Upload the Blower_Slave.ino firmware.

  Software Setup
1.	Google Sheets:
o	Create a new Google Sheet to store heating event data.
o	Open the Script Editor (Extensions > Apps Script).
o	Paste the contents of Google Script --Heating System Monitor.txt into the editor.
o	Deploy the script as a web app and note the URL.
o	📘 Need help with perpetual access to the web app? Check out the guide: Perpetual Script Execution with Google Sheets
2.	ESP32 Configuration:
o	Update the WEB_APP_URL variable in the firmware with the URL obtained from the Google Apps Script deployment.
o	Ensure all ESP32 devices are configured with correct Wi-Fi credentials and within range for ESP-NOW communication.

Usage

Once set up, the master device coordinates data from both slave devices and sends a formatted string (key-value) to the Google Apps Script web app, which logs the event to the perpetual Google Sheet.

License

This project is licensed under the MIT License. See the LICENSE file for details.
README.md created by ChatGPT, edited by and project created by William Lucid, Retired Computer Specialist. Project collaboration from lead Assistant, ChatGPT; with assists from Gemini, and Copilot.
________________________________________

For more information, visual demonstrations, and troubleshooting, please refer to the included Heating System Monitor.mp4 video and putty.log file.
Project was a collaborative effort by ChatGPT, Gemini, Copilot, and William Lucid (Tech500).

