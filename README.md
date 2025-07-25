# 🌬️ PNEUMA — Smart Air Quality & Fire Monitoring IoT System

[![Website](https://img.shields.io/badge/Website-pneumainventor.wixsite.com-blue)](https://pneumainventor.wixsite.com/pneuma)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**PNEUMA** is an advanced IoT-based microcontroller system built using the ESP32, designed to monitor environmental air quality, detect fire-related gases, and send real-time alerts via a Telegram bot and MQTT. It features a dual-display system (OLED and LCD), WiFi connectivity, MQTT integration, EEPROM chat storage, and highly accurate data sensing using DHT11, MQ135, MQ2, and BME280 sensors.

---

## 🚀 Features

- 🔥 Fire gas detection (via MQ2 sensor)
- 🏭 CO₂ & air quality monitoring (via MQ135 sensor)
- 🌡️ Environmental sensing (Temp, Humidity, Pressure, Altitude via DHT11 & BME280)
- 📅 Real-time clock (via DS3231)
- 📟 OLED + 20x4 LCD display output
- 🧠 EEPROM memory for storing Telegram Chat IDs
- ☁️ MQTT support for cloud-based monitoring
- 📲 Telegram Bot alert & command system
- ⚠️ Intelligent air/fire hazard notifications with health recommendations
- 🌐 Web UI for live data ([Live Monitoring Dashboard](https://bit.ly/pneuma-uiweb))

---

## 🛠️ Hardware Used

- **ESP32**
- **DHT11** (Temperature & Humidity)
- **BME280** (Pressure & Altitude)
- **MQ135** (CO₂ / Air Quality)
- **MQ2** (Flammable Gas Detection)
- **RTC DS3231**
- **SSD1306 OLED Display**
- **20x4 I2C LCD**
- **Buzzer + Indicator LEDs**
- **EEPROM (on ESP32)**
- **WiFi Module (built-in)**

---

##📌 Pin Configuration
Component	ESP32 Pin(s) Used	Notes
DHT11	GPIO 13	Data pin connected to GPIO 13
MQ135	GPIO 34 (Analog)	Analog gas sensor input
MQ2	GPIO 35 (Analog)	Analog gas sensor input
BME280 (I2C)	GPIO 21 (SDA), GPIO 22 (SCL)	I2C communication pins
OLED Display	GPIO 21 (SDA), GPIO 22 (SCL)	Shares I2C with BME280
LCD 20x4 I2C	GPIO 21 (SDA), GPIO 22 (SCL)	Shares I2C with OLED and BME280
RTC DS3231	GPIO 21 (SDA), GPIO 22 (SCL)	Shares I2C bus
Buzzer / LED	GPIO 12	Output indicator (can be changed)
EEPROM	Built-in	Uses ESP32’s internal flash EEPROM
WiFi / Telegram Bot	—	Uses ESP32's built-in WiFi, no pin required

⚠️ Ensure I2C devices have unique addresses. You may need an I2C scanner script to debug.

*WIring diagram coming soon

---

## 🧠 How It Works

1. **Sensor Data Collection**  
   The system continuously reads data from:
   - **DHT11**: temperature, humidity
   - **BME280**: pressure, altitude
   - **MQ135**: gas sensor for CO₂ (air quality)
   - **MQ2**: detects flammable gases / smoke for fire warning
   - **RTC**: fetches current time and date

2. **Display & Serial Output**  
   Data is displayed on:
   - **OLED**: condensed multi-line stats
   - **LCD**: formatted real-time time, date, and day

3. **Actuators & Alerts**  
   - When **fire or hazardous air** is detected:
     - Buzzer + red LED blinks
     - Green LED turns off
     - Telegram alert is sent (once per interval)

4. **Telegram Bot**  
   Supports user commands:
   - `/start` – Register and welcome message
   - `/status` – Real-time sensor data report
   - `/solution` – Suggestions based on air/gas quality
   - `/help` – Bot usage guide

5. **MQTT Integration**  
   Sends sensor data to MQTT topics like:
   - `esp32/temperature`
   - `esp32/fire_level`
   - `esp32/air_quality` and more...

6. **EEPROM Chat ID Storage**  
   Saves and loads Telegram chat IDs for persistent alerting, even after ESP32 restart.

---

## 📸 Simulator Layout / Sketch Explanation

The simulated sketch includes:
- **Left section**: ESP32 microcontroller with connected sensors (DHT11, MQ135, MQ2, BME280)
- **Top section**: OLED and 20x4 LCD showing real-time stats
- **Right section**: Buzzer, indicator LEDs (green = safe, red = danger)
- **Bottom section**: WiFi module simulating connection to MQTT and Telegram
- **Virtual serial monitor**: Displays all system logs and sensor readouts for debug

The OLED displays data like:

```
Temp: 28.3 C
Humidity: 65.1 %
CO2: 150.0 PPM
Air: Moderate
Fire: Safe
Pres: 1012.4 hPa
Alt : 15.3 m
```

While the LCD shows:

```
Time: 13:45:02
Date: 24/07/2025
Day : Thursday
````

---

## 📦 Installation & Setup

1. **Install Libraries in Arduino IDE or VSCode (PlatformIO):**
   - `Adafruit_SSD1306`
   - `Adafruit_GFX`
   - `Adafruit_BME280`
   - `DHT sensor library`
   - `RTClib`
   - `LiquidCrystal_I2C`
   - `WiFi`
   - `WiFiClientSecure`
   - `UniversalTelegramBot`
   - `PubSubClient`
   - `EEPROM`

2. **Configure WiFi & Telegram Bot:**
   In `main.cpp`:
   ```cpp
   #define BOT_TOKEN "your_telegram_bot_token"
   const char* ssid = "your_wifi_ssid";
   const char* password = "your_wifi_password";
   const char* mqtt_server = "your_mqtt_broker";
   ```
   
3. **Upload to ESP32:**
   Use Arduino IDE or PlatformIO to flash the code. Serial monitor baud rate: `115200`.

4. **Test & Run:**

   * Join your Telegram Bot and send `/start`
   * View logs via serial monitor
   * Access live web dashboard: [https://bit.ly/pneuma-uiweb](https://bit.ly/pneuma-uiweb)
   * This project links are Official Pneuma (you can make your own UI using Node-red - *JSON file template coming soon)

5. **Display Result:**
   

---

## 🌐 Resources

* 🔗 [Website](https://pneumainventor.wixsite.com/pneuma)
* 🔗 [Live Dashboard](https://bit.ly/pneuma-uiweb)
* 🔗 [GitHub Repo](https://github.com/Eruumaa/Pneuma)

---

## 📄 License

This project is licensed under the MIT License — feel free to use and modify it.

---

## 🤖 Built with ❤️ by the PNEUMA Team
