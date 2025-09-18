# ESP8266 DIY UPS Monitor for TrueNAS

A DIY UPS monitoring system using ESP8266/Wemos D1 Mini with a voltage divider for TrueNAS integration.

## Project Overview

This project enables a TrueNAS server running on a single board computer such as an Odroid H4 to operate with a battery backup system, such as a lead-acid battery with a West Mountain Radio "Epic PWRgate" and Powerwerx S300V power supply. The ESP8266 microcontroller with a simple voltage divider circuit monitors the battery voltage and acts as a UPS interface for TrueNAS.

When utility power fails and battery voltage drops below the critical threshold, the system will send UPS 'NUT' protocol messages to the TrueNAS server, triggering a graceful shutdown to prevent data loss or filesystem corruption.

### TrueNAS Hardware Power Requirements

For reliable operation of TrueNAS hardware, such as an Odroid H4+ board:

- **Input Voltage**: 15V DC is required
- **Current Capacity**: At least 4 amps
- **Power Supply Options**:
  - For battery backup operation, use a DC-DC converter that can output 15V/4A from your 12V battery inpuy
  - Commercial options include adjustable boost converters with sufficient current capacity
  - Ensure the converter is rated for continuous operation at the required amperage

> **Note**: The voltage and current requirements may vary depending on your specific TrueNAS hardware. Check the specifications of your particular board.

## Features

- Measures input voltage through a voltage divider connected to A0 on the ESP8266 microcontroller
- Monitors battery voltage and generates UPS status information
- Outputs Network UPS Tools (NUT) compatible data via Serial/USB
- Optionally sends voltage readings to an MQTT server
- Designed for TrueNAS Scale/Community Edition integration via USB connection
- Configurable voltage thresholds for alerts and shutdown

## Hardware Setup

### Components Required

- Wemos D1 Mini (ESP8266) microcontroller
- 470kΩ resistor
- 100kΩ resistor
- USB cable for connecting the microcontrollerto TrueNAS server
- Power source for ESP8266 (can be from battery system)
- Breadboard and wires (for prototyping)
- Optional: PCB for permanent installation

### Voltage Divider Circuit

The voltage divider is necessary because the ESP8266's analog input (A0) can only handle voltages up to 3.3V, while we need to measure higher voltages from a battery (typically 12-14V).

1. **Complete Circuit Connection:**
   - Connect the battery/power supply POSITIVE terminal to one end of the 470kΩ resistor
   - Connect the other end of the 470kΩ resistor to both the A0 pin and one end of the 100kΩ resistor
   - Connect the other end of the 100kΩ resistor to GND (ground)
   - Connect the NEGATIVE terminal of the battery/power supply to the Wemos D1 Mini GND pin

2. **Power Supply for Wemos D1 Mini:**
   - The Wemos D1 Mini can be powered directly from the TrueNAS USB connection
   - No additional power supply is needed for the microcontroller itself
   - The USB connection simultaneously provides power and serves as the data link to TrueNAS

3. **Circuit Diagram:**
   ```
                                  Wemos D1 Mini
                               +-----------------+
                               |                 |
                               |                 |
   Battery (+) ----+          +--+               |
   (12-14V)        |          |A0|               |
                   |          +--+               |
                   |           |                 |
                   |           |                 |
                   Z 470kΩ     |                 |
                   |           |                 |       USB to TrueNAS
                   |           |                 |    +----------------
                   +--------+--+                 |    |
                             |                   |    |
                             Z 100kΩ             |    |
                             |                   |    |
                             |                   +-+  |
   Battery (-) --------------+-------------------+G|--+
   (GND)                                         |N|
                                                 |D|
                                                 +-+
                                                   |
                                              +----+
                                              |
                                              = GND
   ```

4. **Voltage Calculation:**
   - The voltage divider ratio is: (470k + 100k) / 100k = 5.7
   - This allows measuring voltages up to approximately 18V (3.3V × 5.7) safely
   - The code includes calibration to improve accuracy

### Multimeter Calibration

For accurate voltage readings, you should calibrate the device using a multimeter:

1. **Initial Setup:**
   - Connect the battery system to the voltage divider as shown in the diagram
   - Connect the Wemos D1 Mini to your computer via USB
   - Upload the code with `DEBUG_MODE` set to `true` to see the readings

2. **Calibration Procedure:**
   - Using a digital multimeter, measure the actual battery voltage
   - Note the voltage reported by the ESP8266 via the Serial Monitor
   - In `config.h`, set the following values:
     ```cpp
     #define USE_CALIBRATION 1         // Enable calibration
     #define THEORETICAL_READING 1165   // Your ESP8266 reading × 100 (e.g., 11.65V becomes 1165)
     #define MULTIMETER_READING 1401    // Your multimeter reading × 100 (e.g., 14.01V becomes 1401)
     ```
   - The code will automatically calculate and apply the calibration factor

3. **Verification:**
   - After uploading the updated code, check that the calibrated voltage matches your multimeter
   - Fine-tune the values if needed for maximum accuracy

## Software Setup

### PlatformIO Installation

This project uses PlatformIO for development. To install and use PlatformIO:

1. Install Visual Studio Code (VS Code) from [code.visualstudio.com](https://code.visualstudio.com/)

2. Open VS Code and install the PlatformIO extension:
   - Click on the Extensions icon in the sidebar (or press `Ctrl+Shift+X`)
   - Search for "PlatformIO"
   - Click "Install" on the PlatformIO IDE extension

3. After installation, restart VS Code and wait for PlatformIO to initialize

### Project Setup

1. Clone or download this repository

2. Open the project in VS Code with PlatformIO:
   - Click on the PlatformIO icon in the sidebar
   - Select "Open Project"
   - Navigate to the project folder and open it

3. Copy the configuration template:
   ```bash
   cp include/config.example.h include/config.h
   ```

4. Edit `include/config.h` with your settings:
   - WiFi SSID and password (if using MQTT)
   - MQTT server details (if using MQTT)
   - Set `MQTT_ENABLED` to true or false as needed
   - Adjust UPS voltage thresholds if necessary
   - Set `DEBUG_MODE` to true for troubleshooting

5. Build and upload the code:
   - Click on the PlatformIO icon in the sidebar
   - Under "Project Tasks", expand your project
   - Click "Build" to compile
   - Click "Upload" to flash to your Wemos D1 Mini
   
   Alternatively, use the command line:
   ```bash
   # Build the project
   pio run
   
   # Upload to the device
   pio run --target upload
   ```

## TrueNAS Configuration

### Step 1: Enable and Configure the UPS Service

1. Log in to the TrueNAS web interface as an administrator
2. Navigate to System Settings > Services
3. Find the UPS service in the list and click the edit icon (a pencil)
4. Check the box to "Enable" the service

### Step 2: Set the Connection and Driver

In the same UPS configuration window, set the following parameters:

1. **Identifier**: Give your UPS a name, such as `esp8266ups`

2. **Driver**: Select `blazer_usb` from the dropdown menu
   - This driver works well with our line-based output format
   - Alternative drivers: `genericups` or `usbhid-ups`

3. **Port**: Set to the USB serial port where your ESP8266 is connected
   - For Linux-based TrueNAS systems, this is usually `/dev/ttyUSB0` or similar
   - For TrueNAS SCALE, check the port with `ls /dev/tty*` command in Shell
   - You may need to set correct permissions for the port: `chmod 777 /dev/ttyUSB0`

4. **Baud Rate**: Set to `115200` (very important - must match our code)
   - This can typically be set in the driver configuration file
   - Create or edit `/etc/nut/ups.conf` with:
     ```
     [esp8266ups]
       driver = blazer_usb
       port = /dev/ttyUSB0
       baudrate = 115200
     ```

5. **Description**: Optional description like "ESP8266 DIY UPS Monitor"

6. **Driver Configuration** (advanced):
   - If the driver doesn't work out-of-the-box, create a small script that reads from the serial port and feeds data to NUT in its expected format
   - Sample script using Python:
     ```python
     #!/usr/bin/python3
     import serial
     import time
     import subprocess
     
     ser = serial.Serial('/dev/ttyUSB0', 115200)
     
     while True:
         line = ser.readline().decode('utf-8').strip()
         if line.startswith('battery.') or line.startswith('ups.'):
             key, value = line.split(':', 1)
             subprocess.run(['upsc', '-s', key.strip(), value.strip()])
         time.sleep(0.1)
     ```

### Step 3: Configure the Shutdown Policy

This is the most critical part that tells TrueNAS when to start the shutdown process:

1. In the UPS configuration window, go to the "Shutdown" section

2. Set the **Battery Level** field to a safe threshold (e.g., 10%)
   - This tells TrueNAS to shut down once the battery level reaches this percentage

3. You can also set a **Shutdown Timer** which tells the server to shut down after
   a specific number of seconds once power is lost

4. Set the **Shutdown Mode** to `Local` so the TrueNAS server shuts down itself

5. Click **Save** to apply your settings

TrueNAS will then automatically start the service and monitor your ESP8266 UPS monitor
for power loss or low battery signals.

### NUT Communication Protocol

The Wemos D1 Mini communicates with TrueNAS using two types of messages:

1. **Full UPS Data Reports** (every 2 seconds):
   - Complete set of UPS status data including voltage, battery percentage, estimated runtime
   - Format includes `UPS_DATA_START` and `UPS_DATA_END` markers
   - Contains all values needed by TrueNAS for monitoring and decision-making

2. **Heartbeat Messages** (every 10 seconds):
   - Lightweight messages to confirm the UPS monitor is still functioning
   - Includes basic status information: current status, voltage, and battery charge
   - Helps TrueNAS detect if communication is lost with the UPS

These dual message types ensure reliable communication between the UPS monitor and TrueNAS.

## Configuration Options

The device behavior is controlled through the `config.h` file:

- `MQTT_ENABLED`: Set to `true` to enable WiFi and MQTT publishing, `false` to disable
- `DEBUG_MODE`: Set to `true` to enable debug output on serial, `false` for clean NUT protocol data
- `HEARTBEAT_ONLY_MODE`: Set to `true` to send only heartbeat messages (for testing serial connection)
- `HEARTBEAT_INTERVAL`: Time in milliseconds between heartbeats (default: 5000 ms = 5 seconds)
- `UPS_LOW_VOLTAGE_THRESHOLD`: Voltage at which to send a warning (default: 13.6V)
- `UPS_CRITICAL_VOLTAGE_THRESHOLD`: Voltage at which to trigger shutdown (default: 12.0V)
- `UPS_SHUTDOWN_DELAY`: Delay in seconds before shutdown after critical threshold (default: 30)

## Operational States

The UPS monitor reports these status conditions to TrueNAS:

- **OL** (Online): Normal operation, voltage above warning threshold
- **OB DISCHRG** (On Battery Discharging): Warning state, voltage below warning threshold
- **OB DISCHRG LB** (On Battery Discharging Low Battery): Critical state, voltage below critical threshold

## Testing the Serial Connection

Troubleshooting the connection between the ESP8266 and TrueNAS can be challenging. Here's how to test it:

1. **Enable Heartbeat-Only Mode**:
   - Set `HEARTBEAT_ONLY_MODE = true` in your `config.h`
   - This will send only simple heartbeat messages without the full UPS data
   - Heartbeat messages are sent every `HEARTBEAT_INTERVAL` milliseconds (default: 5000)

2. **Verify Serial Communication on TrueNAS**:
   ```bash
   # Check if device is recognized
   ls -l /dev/ttyUSB*
   
   # Set permissions if needed
   chmod 777 /dev/ttyUSB0
   
   # View raw serial output
   cat /dev/ttyUSB0
   # OR using screen (may need to install)
   screen /dev/ttyUSB0 115200
   ```

3. **Test NUT Configuration**:
   ```bash
   # Restart NUT services
   service nut-server restart
   service nut-monitor restart
   
   # Check if UPS is detected
   upsc esp8266ups
   ```

4. **Examine NUT Logs**:
   ```bash
   # Check NUT logs for errors
   cat /var/log/nut/ups*
   ```

## Troubleshooting

1. Enable `DEBUG_MODE` in config.h to view detailed diagnostic information
2. Check serial output at 115200 baud to verify voltage readings
3. Verify voltage divider connections and resistor values
4. Use a multimeter to validate actual battery voltage against reported values
5. If TrueNAS can't detect the UPS, try these steps:
   - Ensure the USB port has correct permissions
   - Try a different USB port
   - Try a different USB cable
   - Try the heartbeat-only mode to simplify the communication

## License

MIT License
