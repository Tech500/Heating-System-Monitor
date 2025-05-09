Heating System Monitor

An ESP-NOW-based solution to monitor heating system activity without direct integration. This project logs heating events to a Google Sheets spreadsheet, providing a simple and effective way to track heating usage over time.

Overview

This project uses three ESP32 microcontrollers communicating via ESP-NOW: one for outside temperature, one for blower sound detection (using Blower_Slave.ino), and one as the master to consolidate communications and log heating system activity.
The system operates independently of the heating system's internal controls, making it a non-intrusive monitoring solution.

Features

Wireless Monitoring: Employs ESP-NOW for efficient, low-latency communication between devices.

Non-Invasive Setup: Monitors heating activity without requiring direct integration with the heating system.

Data Logging: Records heating events to a Google Sheets spreadsheet and local LittleFS for easy access and analysis. Built-in FTP for viewing ESP32 log file.

Visual Indicators: Includes LED indicators to display system status and activity.

Components

ESP-Now_Master.ino: Firmware for the master ESP32 device responsible for detecting heating activity and sending data.

ESP-Now_Slave.ino: Firmware for the slave ESP32 device that sends BME280 temperature data to the master.

Blower_Slave.ino: Firmware for the second slave ESP32 device that detects blower sound.

Google Script --Heating System Monitor.txt: Google Apps Script to handle incoming data and log it to Google Sheets.

Heating System Monitor GS.jpg: Diagram illustrating the Google Sheets setup.

Heating System Monitor.mp4: Demonstration video showcasing the system in operation.

putty.log: Sample log file capturing serial output for debugging purposes.

Setup Instructions

Hardware Setup

Master ESP32: Connected to MCP9808 Precision temperature sensor with interrupt pin, and MLX90614 Infrared temperature sensor.

Slave ESP32 (Outside): Connected to a BME280 temperature sensor.

Second Slave ESP32: Configured for blower sound detection using DSP bandpass filtering.

Software Setup

Google Sheets:

Create a new Google Sheet to store heating event data.

Open the Script Editor (Extensions > Apps Script).

Paste the contents of Google Script --Heating System Monitor.txt into the editor.

Deploy the script as a web app and note the URL.

📘 Need help with perpetual access to the web app? Check out the guide: [Perpetual Script Execution with Google Sheets](https://gist.github.com/Tech500/57fe6a035e46da4...
}
]
}

Create a new Google Sheet to store heating event data.

Open the Script Editor (Extensions > Apps Script).

Paste the contents of Google Script --Heating System Monitor.txt into the editor.

Deploy the script as a web app and note the URL.

ESP32 Configuration:

Update the WEB_APP_URL variable in the firmware with the URL obtained from the Google Apps Script deployment.

Usage

Once set up, the master device coordinates data from the two slaves and sends the compiled data to Google Sheets.

License

This project is licensed under the MIT License. See the LICENSE file for details. Created by William Lucid, Tech500, with significant collaboration from ChatGPT, Gemini, and Copilot.

For more information, visual demonstrations, and troubleshooting, please refer to the included Heating System Monitor.mp4 video and putty.log file.

