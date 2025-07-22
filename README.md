# SymptomSeek IoT - Fall Detection & Vital Signs Monitoring System

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-blue)](https://platformio.org/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Supported-green)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Arduino](https://img.shields.io/badge/Arduino-Framework-red)](https://www.arduino.cc/)

## 🚨 Project Overview

**SymptomSeek IoT** is an advanced fall detection and vital signs monitoring system built on the ESP32-S3 platform. The system combines multiple sensors to detect falls in real-time and automatically measure vital signs during emergencies, sending instant alerts via Telegram and SMS.

### 🎯 Key Features

- **🔄 Real-time Fall Detection** - Uses MPU6050 accelerometer/gyroscope with 2.5g threshold
- **❤️ Vital Signs Monitoring** - Blood pressure (HX710B) and heart rate (MAX30102) measurement
- **📱 Instant Telegram Alerts** - Emergency notifications with detailed reports
- **📞 SMS Notifications** - Backup communication via SIM800L GSM module
- **🖥️ OLED Display** - Real-time sensor data visualization
- **🔧 Automated Response** - Pneumatic cuff inflation/deflation for blood pressure
- **📊 Multi-sensor Integration** - Comprehensive health monitoring ecosystem

## 🛠️ Hardware Components

| Component | Model | Purpose | I2C Address |
|-----------|-------|---------|-------------|
| **Microcontroller** | ESP32-S3 Freenove WROOM N8R8 | Main processing unit | - |
| **Accelerometer/Gyro** | MPU6050 | Fall detection | 0x68 |
| **Heart Rate Sensor** | MAX30102 | Pulse oximetry | 0x57 |
| **Pressure Sensor** | HX710B | Blood pressure measurement | - |
| **Display** | SSD1306 OLED (128x100) | Real-time data visualization | 0x3C |
| **GSM Module** | SIM800L | SMS emergency alerts | - |
| **Actuators** | Motor + Valve | Pneumatic cuff control | - |

### 📋 Pin Configuration

```cpp
// I2C Bus 1 - MPU6050 & OLED
#define SDA_PIN 8
#define SCL_PIN 9

// I2C Bus 2 - MAX30102
#define MAX_SDA_PIN 12
#define MAX_SCL_PIN 13

// HX710B Pressure Sensor
#define HX_SCK 7
#define HX_DT 6

// SIM800L Serial
#define SIM_TX 17
#define SIM_RX 18

// Actuator Control
#define MOTOR_PIN 21  // Pneumatic pump
#define VALVE_PIN 20  // Release valve

// Status LED
#define LED_PIN 2     // Built-in LED
```

## 🚀 Getting Started

### Prerequisites

- **PlatformIO Core** (6.1.18+)
- **ESP32 Arduino Framework**
- **USB-C Cable** for programming
- **WiFi Network** or mobile hotspot

### 📦 Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/symptoseek_iot.git
   cd symptoseek_iot
   ```

2. **Install PlatformIO (if not installed):**
   ```bash
   # Create virtual environment
   python3 -m venv pio_env
   source pio_env/bin/activate  # Linux/Mac
   # pio_env\Scripts\activate  # Windows
   
   # Install PlatformIO
   pip install platformio
   ```

3. **Install udev rules (Linux only):**
   ```bash
   curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```

4. **Configure WiFi and Telegram:**
   ```cpp
   // In src/main.cpp, update these lines:
   const char* ssid = "YourWiFiNetwork";
   const char* password = "YourPassword";
   const String BOT_TOKEN = "YourTelegramBotToken";
   const String CHAT_ID = "YourChatID";
   ```

### 🔧 Build & Upload

```bash
# Build the project
pio run

# Upload to ESP32-S3
pio run --target upload

# Monitor serial output
pio device monitor --port /dev/ttyACM0 --baud 115200
```

## 📊 System Architecture

```
┌─────────────────────────────────────────────────────┐
│                   ESP32-S3 Core                     │
├─────────────────┬───────────────────┬───────────────┤
│   I2C Bus 1     │    I2C Bus 2      │   Actuators   │
│                 │                   │               │
│ ┌─────────────┐ │ ┌───────────────┐ │ ┌───────────┐ │
│ │   MPU6050   │ │ │   MAX30102    │ │ │  Motor +  │ │
│ │ (Fall Det.) │ │ │ (Heart Rate)  │ │ │   Valve   │ │
│ └─────────────┘ │ └───────────────┘ │ └───────────┘ │
│                 │                   │               │
│ ┌─────────────┐ │ ┌───────────────┐ │ ┌───────────┐ │
│ │ OLED Display│ │ │    HX710B     │ │ │  SIM800L  │ │
│ │  (Status)   │ │ │ (BP Sensor)   │ │ │   (SMS)   │ │
│ └─────────────┘ │ └───────────────┘ │ └───────────┘ │
└─────────────────┴───────────────────┴───────────────┘
                          │
                    ┌─────▼─────┐
                    │   WiFi    │
                    │ Telegram  │
                    │   Bot     │
                    └───────────┘
```

## 🔬 How It Works

### 1. **Fall Detection Algorithm**
```cpp
float totalAcceleration = sqrt(accX² + accY² + accZ²);
if (totalAcceleration > 2.5g) {
    // FALL DETECTED!
    triggerEmergencyResponse();
}
```

### 2. **Emergency Response Sequence**
1. **Immediate Alert** - LED blinks + Telegram notification
2. **Vital Signs Collection** - Automated blood pressure & heart rate
3. **Comprehensive Report** - Medical data sent via Telegram/SMS
4. **Continuous Monitoring** - Real-time status updates

### 3. **Sensor Data Flow**
- **MPU6050**: Continuous 100Hz acceleration monitoring
- **MAX30102**: IR-based pulse detection (finger placement required)
- **HX710B**: Pressure measurement during automated cuff inflation
- **OLED**: Real-time display of all sensor readings

## 📱 Telegram Bot Integration

### Bot Commands & Responses

**🚨 Fall Alert:**
```
🚨 EMERGENCY ALERT 🚨

⚠️ FALL DETECTED!

📊 G-Force: 3.2g
🕐 Time: 1230s since startup
📍 Device: SymptomSeek IoT

🔄 Measuring vital signs now...
📱 Next update with results coming soon!

Bot: @falldetectorSymptoseek_bot
```

**📋 Vitals Report:**
```
📋 VITAL SIGNS REPORT

Following fall detection:

🩸 Blood Pressure: 875 units ✅
❤️ Heart Rate: 78 BPM ✅

🏥 Emergency measurement complete
📱 Please check on the person immediately!

Bot: @falldetectorSymptoseek_bot
```

## 🐛 Troubleshooting

### Common Issues

**1. WiFi Connection Failed**
```
❌ Target network 'NetworkName' NOT FOUND in scan results!
```
- Verify network name and password
- Check signal strength and range
- Ensure hotspot is active (iPhone/Android)

**2. Sensor Not Detected**
```
FAILED - Not found on I2C address 0x68
```
- Check wiring connections
- Verify I2C pin assignments
- Test with I2C scanner

**3. Upload Permission Denied**
```
Warning! Please install '99-platformio-udev.rules'
```
- Install udev rules (Linux)
- Add user to dialout group: `sudo usermod -a -G dialout $USER`

### 📊 Sensor Status Monitoring

The system performs health checks every 10 seconds:
- **Green Status**: All sensors responding
- **Red Status**: I2C communication failure
- **Real-time Display**: Sensor readings every 2 seconds

## 🔧 Configuration

### Fall Detection Sensitivity
```cpp
float fallThreshold = 2.5; // Adjust based on testing (1.5-4.0g range)
```

### Vital Signs Timing
```cpp
// Pressure cuff inflation time
delay(5000); // 5 seconds - adjust for patient comfort

// Heart rate sampling
for (int i = 0; i < 5; i++) {
    irValue += particleSensor.getIR();
    delay(100); // 100ms intervals
}
```

## 📈 Technical Specifications

- **Microcontroller**: ESP32-S3 240MHz, 8MB Flash, 8MB PSRAM
- **Fall Detection**: ±16g range, 0.1g sensitivity
- **Heart Rate**: 50-200 BPM range, IR-based detection
- **Blood Pressure**: Pneumatic measurement with digital readout
- **Communication**: WiFi (802.11 b/g/n), GSM (2G/3G/4G via SIM800L)
- **Display**: 128x100 OLED, I2C interface
- **Power**: USB-C 5V input, low-power sleep modes

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👨‍💻 Author

**Abu Bakar Siddik**
- GitHub: [@siddik-official](https://github.com/siddik-official/)
- Project: SymptomSeek IoT Fall Detection System
- University: UIU (United International University)

## 🙏 Acknowledgments

- **Espressif Systems** - ESP32-S3 platform
- **PlatformIO** - Development environment
- **Arduino Community** - Sensor libraries
- **Telegram Bot API** - Real-time notifications
- **Open Source Contributors** - Various sensor libraries

## 📞 Support

For technical support or questions:
- 📧 Email: official.siddik@gmail.com
- 💬 Telegram: @falldetectorSymptoseek_bot
- 🐛 Issues: [GitHub Issues](https://github.com/siddik-official/symptoseek-iot-device/issues)

---

⭐ **Star this repository if it helped you!** ⭐
