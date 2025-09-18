#ifndef CONFIG_H
#define CONFIG_H

//============================================================================
// CORE SETTINGS
//============================================================================

// Debug mode - Set to true to enable diagnostic output on serial
// Set to false for clean NUT protocol data only
#define DEBUG_MODE false

// MQTT enable flag - Set to true to use WiFi & MQTT, false to disable
// When disabled, the device will only send UPS data via USB Serial
#define MQTT_ENABLED false

// Fake battery full - Set to true to always report battery at 100% and "OL" status
// Useful for testing TrueNAS integration without a real battery
#define FAKE_BATTERY_FULL false

// Heartbeat interval in milliseconds (for periodic status updates)
#define HEARTBEAT_INTERVAL 5000

//============================================================================
// UPS VOLTAGE THRESHOLDS
//============================================================================

// Alarm threshold - Battery voltage that triggers a warning (V)
// Stored as integers (value * 10) to avoid preprocessor issues with floating point
#define UPS_LOW_VOLTAGE_THRESHOLD 136     // 13.6V

// Critical threshold - Battery voltage that triggers shutdown (V) 
#define UPS_CRITICAL_VOLTAGE_THRESHOLD 120 // 12.0V

// Shutdown delay - Time in seconds after critical threshold before shutdown
#define UPS_SHUTDOWN_DELAY 30

//============================================================================
// CALIBRATION SETTINGS
//============================================================================

// Voltage divider theoretical ratio: (470k + 100k) / 100k = 5.7
// Stored as integer (value * 10) to avoid preprocessor issues with floating point
#define VOLTAGE_DIVIDER_RATIO_INT 57   // 5.7 * 10

// ADC reference voltage (Wemos D1 Mini's internal ADC reference)
// Stored as numerator and denominator to avoid preprocessor issues with floating point
#define ADC_VOLTAGE_NUMERATOR 32       // 3.2 * 10
#define ADC_VOLTAGE_DENOMINATOR 10240  // 1024 * 10

// Calibration reference (optional)
// If you have a multimeter reading, use these to auto-calibrate
// Set both to 0 to disable calibration
#define USE_CALIBRATION 0         // Set to 1 to enable calibration, 0 to disable
#define THEORETICAL_READING 1165   // Voltage reading without calibration (multiplied by 100)
#define MULTIMETER_READING 1401    // Actual voltage from multimeter (multiplied by 100)

//============================================================================
// WIFI & MQTT SETTINGS (only used if MQTT_ENABLED is true)
//============================================================================

// WiFi configuration
#define WIFI_SSID "your_wifi_name"
#define WIFI_PASSWORD "your_wifi_password"

// MQTT configuration
#define MQTT_SERVER "your_mqtt_server"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_username"
#define MQTT_PASSWORD "your_mqtt_password"

#endif // CONFIG_H
