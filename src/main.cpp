#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// Fallback definitions if not in config.h
#ifndef VOLTAGE_DIVIDER_RATIO_INT
  #define VOLTAGE_DIVIDER_RATIO_INT 57 // 5.7 * 10
#endif

#ifndef ADC_VOLTAGE_NUMERATOR
  #define ADC_VOLTAGE_NUMERATOR 32 // 3.2 * 10
#endif

#ifndef ADC_VOLTAGE_DENOMINATOR
  #define ADC_VOLTAGE_DENOMINATOR 10240 // 1024 * 10
#endif

#ifndef MQTT_ENABLED
  #define MQTT_ENABLED true
#endif

#ifndef DEBUG_MODE
  #define DEBUG_MODE false
#endif

#ifndef FAKE_BATTERY_FULL
  #define FAKE_BATTERY_FULL false
#endif

#ifndef HEARTBEAT_INTERVAL
  #define HEARTBEAT_INTERVAL 5000
#endif

// WiFi and MQTT fallback credentials (only if MQTT is enabled)
#if MQTT_ENABLED
  #ifndef WIFI_SSID
    #define WIFI_SSID "wa-g1"
  #endif

  #ifndef WIFI_PASSWORD
    #define WIFI_PASSWORD "helpdesk!3"
  #endif

  #ifndef MQTT_SERVER
    #define MQTT_SERVER "192.168.3.2"
  #endif

  #ifndef MQTT_PORT
    #define MQTT_PORT 1884
  #endif

  #ifndef MQTT_USER
    #define MQTT_USER "denis"
  #endif

  #ifndef MQTT_PASSWORD
    #define MQTT_PASSWORD "rotdam"
  #endif
#endif

#ifndef USE_CALIBRATION
  #define USE_CALIBRATION 0
#endif

#ifndef THEORETICAL_READING
  #define THEORETICAL_READING 1165  // 11.65V (value * 100)
#endif

#ifndef MULTIMETER_READING
  #define MULTIMETER_READING 1401  // 14.01V (value * 100)
#endif

// Fallback definitions for UPS thresholds if not in config.h
#ifndef UPS_LOW_VOLTAGE_THRESHOLD
  #define UPS_LOW_VOLTAGE_THRESHOLD 136  // 13.6V (value * 10)
#endif

#ifndef UPS_CRITICAL_VOLTAGE_THRESHOLD
  #define UPS_CRITICAL_VOLTAGE_THRESHOLD 120  // 12.0V (value * 10)
#endif

#ifndef UPS_SHUTDOWN_DELAY
  #define UPS_SHUTDOWN_DELAY 30
#endif

/*
 * Voltage Meter using Wemos D1 Mini with WiFi and MQTT
 *
 * Hardware Setup:
 * - A voltage divider is connected to the A0 analog input pin
 * - The voltage divider consists of two resistors: R1 (470k) and R2 (100k)
 * -kThe formula for the voltage divider is: Vout = Vin * (R2 / (R1 + R2))
 * - The ADC reads Vout, and we calculate Vin by multiplying by the divider ratio
 *
 * Connection Diagram:
 * Input Voltage (to be measured) ---> R1 (470k) ---> A0 pin ---> R2 (100k) ---> GND
 *
 * The Wemos D1 Mini's ADC can only measure voltage between 0 and ~3.2V, but with
 * this voltage divider we can measure voltages up to ~18V (3.2V * 5.7).
 * CAUTION: Do not exceed the maximum input voltage capacity of your voltage divider!
 */

// MQTT topic will be set to the MAC address
String mqtt_topic;

// Set up the calibration values from config.h (converting from integers to float)
const float externalDividerRatio = VOLTAGE_DIVIDER_RATIO_INT / 10.0f;
const float adcVoltageFactor = (float)ADC_VOLTAGE_NUMERATOR / (float)ADC_VOLTAGE_DENOMINATOR;

// Calculate calibration factor if calibration is enabled
#if USE_CALIBRATION == 1
  // Use the provided reference values for calibration (convert from integers back to float)
  const float theoreticalFloat = THEORETICAL_READING / 100.0f;
  const float multimeterFloat = MULTIMETER_READING / 100.0f;
  const float calibrationFactor = multimeterFloat / theoreticalFloat;
#else
  // Default to no calibration
  const float calibrationFactor = 1.0f;
#endif

// Debug macro to conditionally print messages
#define DEBUG_PRINT(...) if(DEBUG_MODE) { Serial.print(__VA_ARGS__); }
#define DEBUG_PRINTLN(...) if(DEBUG_MODE) { Serial.println(__VA_ARGS__); }

#if MQTT_ENABLED
// Create an instance of the MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Variables to track last publish time
unsigned long lastPublishTime = 0;
const long publishInterval = 5000; // Publish every 5 seconds
#endif

// Variables to track UPS data reporting via serial
unsigned long lastUpsReportTime = 0;
const long upsReportInterval = 2000; // Send UPS data every 2 seconds

// Variables for NUT heartbeat
unsigned long lastHeartbeatTime = 0;
const long heartbeatInterval = HEARTBEAT_INTERVAL; // Send heartbeat from config

#if MQTT_ENABLED
// Function to get MAC address as a String
String getMacAddress() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// Function to setup WiFi connection
void setupWiFi() {
  delay(10);
  DEBUG_PRINTLN();
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINT("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  // Set MQTT topic to MAC address
  mqtt_topic = getMacAddress();
  DEBUG_PRINT("MQTT Topic (MAC address): ");
  DEBUG_PRINTLN(mqtt_topic);
}

// Function to reconnect to MQTT server
void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "VoltMeter-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect with credentials
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      DEBUG_PRINTLN("connected");
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
#else
// Function to get device ID as a String when MQTT is disabled
// This provides a unique identifier for the device in NUT data
String getDeviceId() {
  uint8_t mac[6];
  WiFi.macAddress(mac); // Still works without connecting to WiFi
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X",
           mac[3], mac[4], mac[5]); // Just use last 3 bytes for shorter ID
  return String(macStr);
}
#endif

// Track time in low voltage state
unsigned long lowVoltageStartTime = 0;
boolean isInLowVoltageState = false;

// Convert threshold values to float for comparisons
const float lowVoltageThreshold = UPS_LOW_VOLTAGE_THRESHOLD / 10.0;
const float criticalVoltageThreshold = UPS_CRITICAL_VOLTAGE_THRESHOLD / 10.0;

// Function to determine UPS status based on battery voltage
String getUpsStatus(float voltage) {
  if (voltage <= criticalVoltageThreshold) {
    return "OB DISCHRG LB"; // On Battery, Discharging, Low Battery (critical)
  } else if (voltage <= lowVoltageThreshold) {
    return "OB DISCHRG"; // On Battery, Discharging (warning)
  } else {
    return "OL"; // Online (normal)
  }
}

// Calculate estimated battery percentage based on voltage
// Using a simplified linear scale between critical (0%) and full (100%)
int getBatteryPercentage(float voltage) {
  if (voltage <= criticalVoltageThreshold) {
    return 0;
  } else if (voltage >= 14.4) { // Assuming 14.4V is fully charged lead acid battery
    return 100;
  } else {
    // Linear interpolation between critical voltage (0%) and full voltage (100%)
    return int(((voltage - criticalVoltageThreshold) /
               (14.4 - criticalVoltageThreshold)) * 100.0);
  }
}

// Get time remaining estimate based on current voltage level
// This is a simplified estimate; real UPS systems use load and discharge curve
int getTimeRemaining(float voltage) {
  if (voltage <= criticalVoltageThreshold) {
    return 0; // No time remaining at critical level
  } else if (voltage <= lowVoltageThreshold) {
    return UPS_SHUTDOWN_DELAY; // Just use the shutdown delay as estimate
  } else {
    // Simple linear estimate - would need calibration for accuracy
    return int(((voltage - lowVoltageThreshold) * 60) + UPS_SHUTDOWN_DELAY);
  }
}

void setup() {
  Serial.begin(2400);

  #if MQTT_ENABLED
  // Setup WiFi connection
  setupWiFi();

  // Configure MQTT connection
  client.setServer(MQTT_SERVER, MQTT_PORT);
  #endif

  // Initialize random seed
  randomSeed(micros());

  if (DEBUG_MODE) {
    DEBUG_PRINTLN("\n===========================");
    DEBUG_PRINTLN("ESP8266 DIY UPS Monitor");
    DEBUG_PRINTLN("============================");
    #if MQTT_ENABLED
    DEBUG_PRINTLN("- WiFi & MQTT: ENABLED");
    DEBUG_PRINTLN("- Voltage measurements sent via MQTT");
    #else
    DEBUG_PRINTLN("- WiFi & MQTT: DISABLED");
    #endif
    #if FAKE_BATTERY_FULL
    DEBUG_PRINTLN("- FAKE BATTERY MODE: ENABLED");
    DEBUG_PRINTLN("- Always reporting battery at 100% and 'OL' status");
    DEBUG_PRINTLN("- Useful for testing TrueNAS integration");
    #else
    DEBUG_PRINTLN("- Real battery monitoring: ENABLED");
    #endif
    DEBUG_PRINTLN("- UPS status sent via Serial/USB");
    DEBUG_PRINTLN("- Connect USB to TrueNAS for direct UPS monitoring");
    DEBUG_PRINT("- UPS alarm threshold: ");
    DEBUG_PRINT(lowVoltageThreshold);
    DEBUG_PRINTLN("V");
    DEBUG_PRINT("- UPS critical threshold: ");
    DEBUG_PRINT(criticalVoltageThreshold);
    DEBUG_PRINTLN("V");
    DEBUG_PRINT("- UPS shutdown delay: ");
    DEBUG_PRINT(UPS_SHUTDOWN_DELAY);
    DEBUG_PRINTLN(" seconds");
    DEBUG_PRINTLN("- UPS data format: NUT compatible");
    DEBUG_PRINTLN("===========================");
  }
}

void loop() {
  #if MQTT_ENABLED
  // Check and maintain MQTT connection
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  #endif

  // Read the analog value from the A0 pin
  int adcValue = analogRead(A0);

  // Convert the ADC value to the voltage at the A0 pin
  float measuredVoltage = adcValue * adcVoltageFactor;

  // Apply the external voltage divider ratio to get the uncalibrated input voltage
  float rawInputVoltage = measuredVoltage * externalDividerRatio;

  // Apply calibration factor to get the calibrated voltage
  float calibratedVoltage = rawInputVoltage * calibrationFactor;

  // Get UPS status based on voltage
  String upsStatus = getUpsStatus(calibratedVoltage);
  int batteryPercentage = getBatteryPercentage(calibratedVoltage);
  int timeRemaining = getTimeRemaining(calibratedVoltage);

  // Track low voltage state for TrueNAS integration
  if (calibratedVoltage <= lowVoltageThreshold) {
    if (!isInLowVoltageState) {
      // Just entered low voltage state
      lowVoltageStartTime = millis();
      isInLowVoltageState = true;
      DEBUG_PRINTLN("⚠️ WARNING: Battery voltage below threshold!");
    }
  } else {
    if (isInLowVoltageState) {
      // Just exited low voltage state
      isInLowVoltageState = false;
      DEBUG_PRINTLN("✓ Battery voltage restored to normal.");
    }
  }

  // Print the results to the serial monitor (once per second) if in debug mode
  static unsigned long lastPrintTime = 0;
  unsigned long currentTime = millis();
  if (DEBUG_MODE && currentTime - lastPrintTime >= 1000) {
    lastPrintTime = currentTime;
    DEBUG_PRINT("ADC Value: ");
    DEBUG_PRINT(adcValue);
    DEBUG_PRINT("\tRaw Voltage: ");
    DEBUG_PRINT(rawInputVoltage, 2); // Print with 2 decimal places
    DEBUG_PRINT("V\tCalibrated: ");
    DEBUG_PRINT(calibratedVoltage, 2); // Print with 2 decimal places
    DEBUG_PRINT("V\tStatus: ");
    DEBUG_PRINT(upsStatus);
    DEBUG_PRINT("\tBattery: ");
    DEBUG_PRINT(batteryPercentage);
    DEBUG_PRINTLN("%");
  }

  // Get the current milliseconds for timing purposes
  unsigned long currentMillis = millis();

  #if MQTT_ENABLED
  // If no command has been received recently, we can still publish to MQTT if enabled
  if (MQTT_ENABLED && currentMillis - lastPublishTime >= publishInterval) {
    lastPublishTime = currentMillis;

    // Regular voltage data payload
    String payload = "{\"";
    payload += "adc_value\": " + String(adcValue) + ", ";
    payload += "\"raw_voltage\": " + String(rawInputVoltage, 2) + ", ";
    payload += "\"input_voltage\": " + String(calibratedVoltage, 2);
    payload += "}";

    // Publish to the MQTT topic (MAC address)
    DEBUG_PRINT("Publishing to topic: ");
    DEBUG_PRINTLN(mqtt_topic);
    DEBUG_PRINT("Payload: ");
    DEBUG_PRINTLN(payload);

    if (client.publish(mqtt_topic.c_str(), payload.c_str())) {
      DEBUG_PRINTLN("Publish successful");
    } else {
      DEBUG_PRINTLN("Publish failed");
    }
  }
  #endif

  // Process any incoming serial commands using the Megatec protocol
  if (Serial.available()) {
    String command = Serial.readStringUntil('\r');

    // Prepare the values to send - either real or fake
    int reportedBatteryPercent;
    float reportedVoltage;
    float reportedInputVoltage = 230.0; // Default input voltage (no actual measurement)
    int reportedRuntime;
    String reportedStatus;
    float reportedFrequency = 50.0; // Default frequency (no actual measurement)
    float reportedTemperature = 25.0; // Default temperature (no actual measurement)
    int reportedLoad = 50; // Default load (no actual measurement)

    #if FAKE_BATTERY_FULL
      // Use fake battery full data
      reportedBatteryPercent = 100;
      reportedVoltage = 14.4; // Typical fully charged lead-acid battery
      reportedRuntime = 3600; // 1 hour in seconds
      reportedStatus = "OL"; // Online status
    #else
      // Use real battery data
      reportedBatteryPercent = batteryPercentage;
      reportedVoltage = calibratedVoltage;
      reportedRuntime = timeRemaining * 60; // Convert minutes to seconds
      reportedStatus = upsStatus;
    #endif

    if (DEBUG_MODE) {
      DEBUG_PRINT("Received command: ");
      DEBUG_PRINTLN(command);
    }

    // Process commands according to the Megatec protocol
    if (command == "Q1") {
      // Status inquiry command - format: (MMM.M NNN.N PPP.P QQQ RR.R S.SS TT.T b7b6b5b4b3b2b1b0<cr>
      // Create the status byte b7b6b5b4b3b2b1b0
      String statusByte = "00000000"; // Default all bits to 0

      if (reportedStatus == "OB DISCHRG LB") {
        // Utility fail (bit 7) and battery low (bit 6)
        statusByte = "11000000";
      } else if (reportedStatus == "OB DISCHRG") {
        // Utility fail (bit 7) only
        statusByte = "10000000";
      } else {
        // Online - everything normal
        statusByte = "00000000";
      }

      // Format voltage with 1 decimal place
      char voltageStr[7];
      sprintf(voltageStr, "%05.2f", reportedVoltage);  // Battery voltage with 2 decimal places

      // Send the formatted status response
      Serial.print("(");
      Serial.print(String(reportedInputVoltage, 1)); // Input voltage
      Serial.print(" ");
      Serial.print(String(reportedInputVoltage, 1)); // Input fault voltage (same as normal since we don't track faults)
      Serial.print(" ");
      Serial.print(String(reportedInputVoltage, 1)); // Output voltage (same as input for this simple implementation)
      Serial.print(" ");
      Serial.print(String(reportedLoad).length() < 3 ? "0" + String(reportedLoad) : String(reportedLoad)); // Load as percentage
      Serial.print(" ");
      Serial.print(String(reportedFrequency, 1)); // Frequency
      Serial.print(" ");
      Serial.print(voltageStr); // Battery voltage
      Serial.print(" ");
      Serial.print(String(reportedTemperature, 1)); // Temperature
      Serial.print(" ");
      Serial.print(statusByte); // Status byte
      Serial.print("\r");
    }
    else if (command == "F") {
      // Rating information command - format: #MMM.M QQQ SS.SS RR.R<cr>
      Serial.print("#");
      Serial.print("230.0"); // Rating voltage
      Serial.print(" ");
      Serial.print("010"); // Rating current (10 amps)
      Serial.print(" ");
      Serial.print("12.00"); // Battery voltage nominal
      Serial.print(" ");
      Serial.print("50.0"); // Rating frequency
      Serial.print("\r");
    }
    else if (command == "I") {
      // UPS information command - format: #Company_Name UPS_Model Version<cr>
      Serial.print("#");
      Serial.print("DIY-ESP8266    "); // Company name (15 chars)
      Serial.print("UPS-V1    "); // UPS model (10 chars)
      Serial.print("1.0       "); // Version (10 chars)
      Serial.print("\r");
    }

    if (DEBUG_MODE) {
      DEBUG_PRINTLN("Command processed");
    }
  }

  // We no longer send heartbeat messages as they're not part of the Megatec protocol
  // Instead, we'll respond to commands sent by NUT

  // The main loop continues to run but we don't need the delay anymore
  // as we're now responsive to serial commands
  delay(100); // Small delay to prevent CPU hogging
}
