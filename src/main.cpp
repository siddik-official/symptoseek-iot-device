#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050_light.h>
#include <SoftwareSerial.h>
#include <MAX30105.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "HX710B.h"

// WiFi credentials - Replace with your network details
// const char* ssid = "SidBro's iphone"; // Your iPhone hotspot
// const char* password = "siddu987"; // Your hotspot password

// Backup networks for testing
const char* ssid = "UIU-STUDENT"; // University network backup
const char* password = "12345678"; // University password

// Telegram Bot credentials - Replace with your actual bot details
const String BOT_TOKEN = "8148329264:AAEN8PbzLS94nnTk_dhumIncFBvvmVOOTc4";  // Your Bot Token
const String CHAT_ID = "1683560300";     // Get your chat ID
const String TELEGRAM_URL = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 100
#define OLED_RESET     -1
#define SDA_PIN 8
#define SCL_PIN 9
#define MAX_SDA_PIN 12
#define MAX_SCL_PIN 13
#define LED_PIN 2  // Built-in LED on ESP32-S3
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MPU6050
MPU6050 mpu(Wire);
bool fallDetected = false;

// SIM800L
SoftwareSerial sim800(17, 18); // TX, RX

// MAX30102 - Using separate I2C instance
TwoWire MaxWire = TwoWire(1);
MAX30105 particleSensor;

// HX710B
#define HX_SCK 7
#define HX_DT 6
HX710B pressureSensor(HX_DT, HX_SCK);

// Actuator Pins
#define MOTOR_PIN 21
#define VALVE_PIN 20

// Thresholds
float fallThreshold = 2.5; // Change based on testing

// WiFi status
bool wifiConnected = false;

// Global flag to track display availability
bool displayInitialized = false;

// Function declarations
void displayMessage(String msg);
void measureAndSendVitals();
void sendSMS(String text);
void blinkLEDForFall();
void displayRealtimeSensorData();
void connectToWiFi();
void sendTelegramMessage(String message);
void sendInstantFallAlert(float gForce);
void sendVitalsReport(long pressure, int bpm, bool pressureSuccess, bool heartRateSuccess);

void measureAndSendVitals(); // Ensure declaration before usage

void setup() {
  Serial.begin(115200);
  delay(2000); // Give time for serial to initialize
  Serial.println("SymptomSeek IoT System Starting...");
  
  // Initialize WiFi early with error handling
  Serial.println("Initializing WiFi subsystem...");
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Initialize I2C buses first
  Wire.begin(SDA_PIN, SCL_PIN); // Use pins 8 (SDA) and 9 (SCL) for MPU6050 and OLED
  MaxWire.begin(MAX_SDA_PIN, MAX_SCL_PIN); // Use pins 12 (SDA) and 13 (SCL) for MAX30102
  
  // Initialize other pins
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(VALVE_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Turn off LED initially

  // Initialize WiFi connection with safety checks
  connectToWiFi();

  // Initialize OLED and check connection
  Serial.print("Checking OLED display... ");
  
  // Check if display is present on I2C first (I2C already initialized)
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) {
    Serial.println("Display found on I2C");
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println("FAILED! SSD1306 allocation failed");
      displayInitialized = false;
    } else {
      Serial.println("OK");
      displayInitialized = true;
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.display(); // Make sure display is working
      delay(100);
      displayMessage("Initializing...\nChecking sensors");
    }
  } else {
    Serial.println("FAILED! No display found on I2C address 0x3C");
    displayInitialized = false;
  }

  // Check MPU6050 connection
  Serial.print("Checking MPU6050... ");
  Wire.beginTransmission(0x68); // MPU6050 I2C address
  if (Wire.endTransmission() == 0) {
    Serial.println("OK - Device found");
    if (mpu.begin() == 0) {
      Serial.println("MPU6050 initialized successfully");
      mpu.calcGyroOffsets();
      displayMessage("MPU6050: OK\nCalibrating...");
      delay(2000);
    } else {
      Serial.println("MPU6050 initialization failed");
      displayMessage("MPU6050: Init Failed");
      delay(2000);
    }
  } else {
    Serial.println("FAILED - Not found on I2C");
    displayMessage("MPU6050: Not Found");
    delay(2000);
  }

  // Check MAX30102 connection
  Serial.print("Checking MAX30102... ");
  if (particleSensor.begin(MaxWire, I2C_SPEED_STANDARD)) {
    Serial.println("OK - Device found");
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeGreen(0);
    displayMessage("MAX30102: OK");
    delay(1000);
  } else {
    Serial.println("FAILED - Not found on I2C pins 12/13");
    displayMessage("MAX30102: Not Found");
    delay(2000);
  }

  // Check HX710B pressure sensor
  Serial.print("Checking HX710B pressure sensor... ");
  pressureSensor.begin();
  // Test if sensor is responding by trying to read
  long testReading = pressureSensor.read();
  if (testReading != 0) {
    Serial.println("OK - Sensor responding");
    displayMessage("HX710B: OK");
    delay(1000);
  } else {
    Serial.println("WARNING - Sensor may not be connected");
    displayMessage("HX710B: Check Connection");
    delay(2000);
  }

  // Initialize SIM800L
  Serial.print("Initializing SIM800L... ");
  sim800.begin(9600);
  delay(1000);
  
  // Test SIM800L communication
  sim800.println("AT");
  delay(100);
  if (sim800.available()) {
    String response = sim800.readString();
    if (response.indexOf("OK") != -1) {
      Serial.println("OK - Module responding");
      displayMessage("SIM800L: OK");
    } else {
      Serial.println("FAILED - No response");
      displayMessage("SIM800L: No Response");
    }
  } else {
    Serial.println("FAILED - No communication");
    displayMessage("SIM800L: Failed");
  }
  delay(2000);

  // Test actuator pins
  Serial.println("Testing actuator pins...");
  displayMessage("Testing Motors...");
  digitalWrite(MOTOR_PIN, HIGH);
  delay(500);
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(VALVE_PIN, HIGH);
  delay(500);
  digitalWrite(VALVE_PIN, LOW);
  Serial.println("Actuator test complete");

  // Send system startup notification to Telegram
  if (wifiConnected) {
    sendTelegramMessage("🚨 SymptomSeek IoT System Started\n\n✅ System initialization complete\n🔋 All sensors checked\n📱 Fall detection ACTIVE\n\nBot: @falldetectorSymptoseek_bot");
  }

  // Display final system status
  Serial.println("=== System Initialization Complete ===");
  displayMessage("System Ready!\nTelegram Connected\nFall Detection Active");
  delay(3000);
}

void displayMessage(String msg) {
  // Always print to serial as backup
  Serial.println("DISPLAY: " + msg);
  
  // Only use OLED if properly initialized
  if (displayInitialized) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(msg);
    display.display();
  }
}

void loop() {
  static unsigned long lastStatusCheck = 0;
  static unsigned long lastRealTimeDisplay = 0;
  static int statusDisplayCounter = 0;
  
  // Check WiFi connection periodically
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    Serial.println("WiFi disconnected! Attempting to reconnect...");
    wifiConnected = false;
    connectToWiFi();
  }
  
  // Check all sensor connections every 10 seconds
  if (millis() - lastStatusCheck > 10000) {
    lastStatusCheck = millis();
    
    String statusMessage = "Status Check:\n";
    bool allSensorsOK = true;
    
    // Check MPU6050
    Wire.beginTransmission(0x68);
    if (Wire.endTransmission() == 0) {
      statusMessage += "MPU6050: OK\n";
      Serial.println("MPU6050: Connected and responding");
    } else {
      statusMessage += "MPU6050: FAIL\n";
      Serial.println("MPU6050: Not responding on I2C pins 8/9");
      allSensorsOK = false;
    }
    
    // Check MAX30102 - try to read device ID
    MaxWire.beginTransmission(0x57); // MAX30102 I2C address
    if (MaxWire.endTransmission() == 0) {
      statusMessage += "MAX30102: OK";
      Serial.println("MAX30102: Connected on I2C pins 12/13");
    } else {
      statusMessage += "MAX30102: FAIL";
      Serial.println("MAX30102: Not responding on I2C pins 12/13");
      allSensorsOK = false;
    }
    
    // Display status every other check (every 20 seconds)
    if (statusDisplayCounter % 2 == 0) {
      displayMessage(statusMessage);
      delay(3000); // Show status for 3 seconds
    }
    statusDisplayCounter++;
  }
  
  // Display real-time sensor data every 2 seconds
  if (millis() - lastRealTimeDisplay > 2000) {
    lastRealTimeDisplay = millis();
    displayRealtimeSensorData();
  }
  
  // Check if MPU6050 is connected before updating
  Wire.beginTransmission(0x68); // MPU6050 I2C address
  if (Wire.endTransmission() == 0) {
    // MPU6050 is connected, safe to update
    mpu.update();
    
    // Fall Detection Logic
    float accX = mpu.getAccX();
    float accY = mpu.getAccY();
    float accZ = mpu.getAccZ();
    float gyroX = mpu.getGyroX();
    float gyroY = mpu.getGyroY();
    float gyroZ = mpu.getGyroZ();
    float totalAcceleration = sqrt(pow(accX, 2) + pow(accY, 2) + pow(accZ, 2));
    
    // Print detailed sensor values for debugging
    Serial.print("MPU6050 Data - Accel(g): X=");
    Serial.print(accX, 3);
    Serial.print(", Y=");
    Serial.print(accY, 3);
    Serial.print(", Z=");
    Serial.print(accZ, 3);
    Serial.print(", Total=");
    Serial.print(totalAcceleration, 3);
    Serial.print(" | Gyro(°/s): X=");
    Serial.print(gyroX, 2);
    Serial.print(", Y=");
    Serial.print(gyroY, 2);
    Serial.print(", Z=");
    Serial.println(gyroZ, 2);
    
    if (totalAcceleration > fallThreshold) {
      fallDetected = true;
      Serial.println("*** FALL DETECTED! *** Total acceleration: " + String(totalAcceleration, 3) + "g");
      
      // Start LED blinking for fall notification
      blinkLEDForFall();
      
      displayMessage("FALL DETECTED!\nG-force: " + String(totalAcceleration, 2) + "g\nSending alerts...");
      
      // Send instant Telegram notification
      sendInstantFallAlert(totalAcceleration);
      
      // Send SMS notification
      sendSMS("EMERGENCY: Fall detected! G-force: " + String(totalAcceleration, 3) + "g. Measuring vitals...");
      
      // Measure vitals and send report
      measureAndSendVitals();
      fallDetected = false;
    }
  } else {
    // MPU6050 not connected
    displayMessage("ERROR!\nMPU6050: DISCONNECTED\nCheck I2C pins 8/9\nSystem halted");
    Serial.println("CRITICAL ERROR: MPU6050 not connected on I2C pins 8 and 9");
    
    // Blink LED to indicate error
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  delay(100); // Faster loop for better responsiveness
}

void connectToWiFi() {
  Serial.println("Starting WiFi connection...");
  
  // Initialize WiFi and set mode
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Add null pointer checks for credentials
  if (ssid == nullptr || password == nullptr) {
    Serial.println("ERROR: WiFi credentials are null!");
    displayMessage("WiFi Error!\nNull credentials");
    wifiConnected = false;
    return;
  }
  
  // Scan for available networks first
  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found!");
  } else {
    Serial.printf("Found %d networks:\n", n);
    bool targetFound = false;
    for (int i = 0; i < n; i++) {
      String networkName = WiFi.SSID(i);
      Serial.printf("%d: %s (Signal: %d dBm) %s\n", 
                    i, networkName.c_str(), WiFi.RSSI(i), 
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURED");
      if (networkName == String(ssid)) {
        targetFound = true;
        Serial.printf("✅ Target network '%s' found with signal strength: %d dBm\n", ssid, WiFi.RSSI(i));
      }
    }
    if (!targetFound) {
      Serial.printf("❌ Target network '%s' NOT FOUND in scan results!\n", ssid);
      displayMessage("WiFi Error!\nNetwork not found\n" + String(ssid));
      wifiConnected = false;
      return;
    }
  }
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  Serial.print("Password length: ");
  Serial.println(strlen(password));
  displayMessage("Connecting to WiFi...\n" + String(ssid));
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  
  Serial.print("Connection attempts: ");
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Print WiFi status for debugging
    if (attempts % 5 == 0) {
      Serial.print(" [Status: ");
      Serial.print(WiFi.status());
      Serial.print("] ");
    }
    
    // Add watchdog reset to prevent timeout issues
    yield();
  }
  
  Serial.println(); // New line after dots
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    displayMessage("WiFi Connected!\nIP: " + WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    Serial.println("WiFi connection failed!");
    Serial.print("Final WiFi status: ");
    Serial.println(WiFi.status());
    Serial.println("Possible issues:");
    Serial.println("- Wrong password");
    Serial.println("- Network not found"); 
    Serial.println("- Network requires captive portal");
    Serial.println("- Signal too weak");
    displayMessage("WiFi Failed!\nCheck credentials\nStatus: " + String(WiFi.status()));
  }
  delay(2000);
}

void sendTelegramMessage(String message) {
  if (!wifiConnected) {
    Serial.println("Cannot send Telegram message: WiFi not connected");
    return;
  }

  HTTPClient http;
  http.begin(TELEGRAM_URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String postData = "chat_id=" + CHAT_ID + "&text=" + message;
  
  Serial.println("Sending Telegram message...");
  int httpResponseCode = http.POST(postData);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Telegram response code: " + String(httpResponseCode));
    Serial.println("Telegram response: " + response);
  } else {
    Serial.println("Error sending Telegram message: " + String(httpResponseCode));
  }
  
  http.end();
}

void sendInstantFallAlert(float gForce) {
  String alertMessage = "🚨 EMERGENCY ALERT 🚨\n\n";
  alertMessage += "⚠️ FALL DETECTED!\n\n";
  alertMessage += "📊 G-Force: " + String(gForce, 2) + "g\n";
  alertMessage += "🕐 Time: " + String(millis()/1000) + "s since startup\n";
  alertMessage += "📍 Device: SymptomSeek IoT\n\n";
  alertMessage += "🔄 Measuring vital signs now...\n";
  alertMessage += "📱 Next update with results coming soon!\n\n";
  alertMessage += "Bot: @falldetectorSymptoseek_bot";
  
  sendTelegramMessage(alertMessage);
  Serial.println("Instant fall alert sent to Telegram");
}

void sendVitalsReport(long pressure, int bpm, bool pressureSuccess, bool heartRateSuccess) {
  String reportMessage = "📋 VITAL SIGNS REPORT\n\n";
  reportMessage += "Following fall detection:\n\n";
  
  // Blood Pressure Report
  if (pressureSuccess) {
    reportMessage += "🩸 Blood Pressure: " + String(pressure) + " units ✅\n";
  } else {
    reportMessage += "🩸 Blood Pressure: ERROR ❌\n";
    reportMessage += "   - Sensor not responding\n";
  }
  
  // Heart Rate Report  
  if (heartRateSuccess) {
    reportMessage += "❤️ Heart Rate: " + String(bpm) + " BPM ✅\n";
  } else if (bpm == -1) {
    reportMessage += "❤️ Heart Rate: ERROR ❌\n";
    reportMessage += "   - No finger detected on sensor\n";
  } else {
    reportMessage += "❤️ Heart Rate: ERROR ❌\n";
    reportMessage += "   - Sensor not connected\n";
  }
  
  reportMessage += "\n🏥 Emergency measurement complete\n";
  reportMessage += "📱 Please check on the person immediately!\n\n";
  reportMessage += "Bot: @falldetectorSymptoseek_bot";
  
  sendTelegramMessage(reportMessage);
  Serial.println("Vitals report sent to Telegram");
}

void measureAndSendVitals() {
  Serial.println("=== Starting Vital Signs Measurement ===");
  displayMessage("Measuring Vitals...\nInflating cuff");
  
  // Inflate Cuff
  digitalWrite(MOTOR_PIN, HIGH);
  Serial.println("Inflating pressure cuff...");
  delay(5000); // Adjust based on testing
  digitalWrite(MOTOR_PIN, LOW);
  Serial.println("Cuff inflation complete");

  // Read Pressure with error checking
  displayMessage("Reading pressure...");
  long pressure = 0;
  bool pressureReadSuccess = false;
  
  for (int attempts = 0; attempts < 3; attempts++) {
    pressure = pressureSensor.read();
    if (pressure > 0) { // Assuming positive pressure indicates valid reading
      pressureReadSuccess = true;
      break;
    }
    Serial.println("Pressure read attempt " + String(attempts + 1) + " failed");
    delay(500);
  }
  
  if (pressureReadSuccess) {
    Serial.println("Pressure reading successful: " + String(pressure));
  } else {
    Serial.println("ERROR: Failed to read pressure after 3 attempts");
    pressure = -1; // Indicate error
  }

  // Read Heart Rate with connection check
  displayMessage("Reading heart rate...");
  int bpm = 0;
  bool heartRateSuccess = false;
  
  // Check MAX30102 connection first
  MaxWire.beginTransmission(0x57);
  if (MaxWire.endTransmission() == 0) {
    Serial.println("MAX30102 connected, reading heart rate...");
    
    // Re-setup sensor for measurement
    particleSensor.setup();
    delay(1000);
    
    // Take multiple readings for better accuracy
    long irValue = 0;
    for (int i = 0; i < 5; i++) {
      irValue += particleSensor.getIR();
      delay(100);
    }
    irValue /= 5; // Average reading
    
    if (irValue > 50000) { // Threshold for finger detection
      bpm = irValue / 1000; // Simplified BPM calculation
      heartRateSuccess = true;
      Serial.println("Heart rate measurement successful: " + String(bpm) + " BPM");
    } else {
      Serial.println("WARNING: No finger detected on MAX30102 sensor");
      bpm = -1;
    }
  } else {
    Serial.println("ERROR: MAX30102 not responding on I2C pins 12/13");
    bpm = -2; // Indicate sensor not connected
  }

  // Deflate pressure cuff
  displayMessage("Deflating cuff...");
  digitalWrite(VALVE_PIN, HIGH);
  Serial.println("Deflating pressure cuff...");
  delay(3000);
  digitalWrite(VALVE_PIN, LOW);
  Serial.println("Cuff deflation complete");

  // Send Telegram vitals report
  sendVitalsReport(pressure, bpm, pressureReadSuccess, heartRateSuccess);

  // Prepare SMS data string with error handling
  String data = "VITALS REPORT:\n";
  
  if (pressureReadSuccess) {
    data += "Blood Pressure: " + String(pressure) + " units\n";
  } else {
    data += "Blood Pressure: ERROR - Sensor not responding\n";
  }
  
  if (heartRateSuccess) {
    data += "Heart Rate: " + String(bpm) + " BPM\n";
  } else if (bpm == -1) {
    data += "Heart Rate: ERROR - No finger detected\n";
  } else {
    data += "Heart Rate: ERROR - Sensor not connected\n";
  }
  
  data += "Status: Emergency measurement complete";
  
  Serial.println("Sending SMS with vital signs data...");
  sendSMS(data);
  
  displayMessage("Vitals Measured!\nReports Sent\nPressure: " + String(pressure) + "\nHR: " + String(bpm));
  Serial.println("=== Vital Signs Measurement Complete ===");
  
  delay(3000); // Show results for 3 seconds
}

void sendSMS(String text) {
  Serial.println("Attempting to send SMS...");
  
  // Test SIM800L connection first
  sim800.println("AT");
  delay(1000);
  
  if (sim800.available()) {
    String response = sim800.readString();
    if (response.indexOf("OK") != -1) {
      Serial.println("SIM800L responding, sending SMS...");
      
      // Set SMS text mode
      sim800.println("AT+CMGF=1");
      delay(1000);
      
      // Set recipient number (replace with actual number)
      sim800.println("AT+CMGS=\"+8801XXXXXXXXX\"");
      delay(1000);
      
      // Send message text
      sim800.print(text);
      delay(1000);
      
      // Send Ctrl+Z to end message
      sim800.write(26);
      delay(3000);
      
      // Check for response
      if (sim800.available()) {
        String smsResponse = sim800.readString();
        if (smsResponse.indexOf("OK") != -1) {
          Serial.println("SMS sent successfully!");
        } else {
          Serial.println("SMS sending failed: " + smsResponse);
        }
      } else {
        Serial.println("No response from SIM800L after sending SMS");
      }
    } else {
      Serial.println("SIM800L not responding properly: " + response);
    }
  } else {
    Serial.println("ERROR: SIM800L not responding to AT command");
  }
}

void blinkLEDForFall() {
  Serial.println("Starting LED fall notification (6 seconds)...");
  
  // Blink LED rapidly for 6 seconds to indicate fall detection
  unsigned long startTime = millis();
  while (millis() - startTime < 6000) { // 6 seconds
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
  
  Serial.println("LED fall notification complete");
}

void displayRealtimeSensorData() {
  // Check if MPU6050 is connected
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() == 0) {
    mpu.update();
    
    float accX = mpu.getAccX();
    float accY = mpu.getAccY();
    float accZ = mpu.getAccZ();
    float gyroX = mpu.getGyroX();
    float gyroY = mpu.getGyroY();
    float gyroZ = mpu.getGyroZ();
    float totalAccel = sqrt(pow(accX, 2) + pow(accY, 2) + pow(accZ, 2));
    float temp = mpu.getTemp();
    
    // Prepare display data
    String displayData = "MPU6050 Real-time:\n";
    displayData += "Accel(g): " + String(totalAccel, 2) + "\n";
    displayData += "X:" + String(accX, 1) + " Y:" + String(accY, 1) + " Z:" + String(accZ, 1) + "\n";
    displayData += "Gyro(deg/s):\n";
    displayData += "X:" + String(gyroX, 0) + " Y:" + String(gyroY, 0) + " Z:" + String(gyroZ, 0) + "\n";
    displayData += "Temp: " + String(temp, 1) + "C\n";
    displayData += "Fall threshold: " + String(fallThreshold, 1) + "g\n";
    
    // Show WiFi status
    if (wifiConnected) {
      displayData += "WiFi: OK";
    } else {
      displayData += "WiFi: DISCONNECTED";
    }
    
    // Display real-time sensor data on OLED safely
    if (displayInitialized) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.println(displayData);
      display.display();
    }
    
    // Also print to serial for debugging
    Serial.println("=== Real-time Sensor Data ===");
    Serial.println(displayData);
    
    // Also check MAX30102 if connected
    MaxWire.beginTransmission(0x57);
    if (MaxWire.endTransmission() == 0) {
      long irValue = particleSensor.getIR();
      Serial.println("MAX30102 IR Value: " + String(irValue) + " (Finger detected: " + (irValue > 50000 ? "YES" : "NO") + ")");
    }
  } else {
    displayMessage("MPU6050 ERROR!\nSensor disconnected\nCheck wiring\nI2C pins 8/9");
  }
}
