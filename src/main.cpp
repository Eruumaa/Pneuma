#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <vector>

// ============ EEPROM CONFIG ============
#define EEPROM_SIZE 512
#define EEPROM_ADDR_CHATID 0
std::vector<String> chatIDs;
String savedChatID = "";

// ============ WiFi & Telegram ============
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";
#define BOT_TOKEN "your_telegram_bot_token"
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// ============ MQTT ============
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqtt_server = "your_mqtt_broker";
const int mqtt_port = 1883;

// ============ OLED & LCD ============
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ============ RTC & DHT & BME ============
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BME280 bme;

// ============ Gas Sensors & Actuators ============
#define MQ135_PIN 35
#define MQ2_PIN 34
#define MQ2_THRESHOLD 3000
#define BUZZER_PIN 26
#define LAMP_PIN 27
#define GREEN_LAMP_PIN 32
float R0 = 10.0;

// ============ Timer ============
unsigned long lastBotSend = 0;
const unsigned long botInterval = 3600000;
unsigned long lastDangerSent = 0;
const unsigned long dangerCooldown = 30000;

//==== Fire Notification =========
unsigned long lastFireSent = 0;
const unsigned long fireCooldown = 60000;

// =================== FUNCTION =====================
String loadChatID() {
  int len = EEPROM.read(EEPROM_ADDR_CHATID);
  String result = "";
  for (int i = 0; i < len; i++) {
    result += char(EEPROM.read(EEPROM_ADDR_CHATID + 2 + i));
  }
  return result;
}

void saveChatID(String chatID) {
  if (!std::count(chatIDs.begin(), chatIDs.end(), chatID)) {
    chatIDs.push_back(chatID);
  }
}

void showTypingAnimation(String text, int repeatCount = 3, int delayMs = 200) {
  for (int r = 0; r < repeatCount; r++) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int centerX = (SCREEN_WIDTH - text.length() * 12) / 2;
    int centerY = SCREEN_HEIGHT / 2 - 10;
    for (int i = 0; i < text.length(); i++) {
      display.clearDisplay();
      display.setCursor(centerX, centerY);
      display.print(text.substring(0, i + 1));
      display.display();
      delay(delayMs);
    }
    delay(500);
  }
}

float calibrateSensor() {
  int numReadings = 100;
  float total = 0.0;
  for (int i = 0; i < numReadings; i++) {
    int sensorValue = analogRead(MQ135_PIN);
    if (sensorValue > 0) {
      total += (4095.0 / sensorValue - 1.0) * 10.0;
    }
    delay(100);
  }
  return total / numReadings;
}

void updateDisplay(DateTime now, float temperature, float humidity, float pressure, float altitude, float CO2_ppm, String airQuality, String fireLevel, int mq2Value) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("Temp: %.1f C\nHumidity: %.1f %%\nCO2: %.1f PPM\n", temperature, humidity, CO2_ppm);
  display.printf("Air: %s\nFire: %s\nPres: %.1f hPa\nAlt : %.1f m\n", airQuality.c_str(), fireLevel.c_str(), pressure, altitude);
  display.display();
}

void printToSerial(DateTime now, float temperature, float humidity, float pressure, float altitude, float CO2_ppm, String airQuality, String fireLevel, int mq2Value) {
  Serial.println("=== SENSOR MONITORING ===");
  Serial.printf("🕒 Time   : %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
  Serial.printf("📅 Date   : %02d/%02d/%d (%s)\n", now.day(), now.month(), now.year(), daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.printf("🌡 Temp   : %.1f °C\n", temperature);
  Serial.printf("💧 Humid  : %.1f %%\n", humidity);
  Serial.printf("💨 Press  : %.1f hPa\n", pressure);
  Serial.printf("🗻 Alt    : %.1f m\n", altitude);
  Serial.printf("🏭 CO₂    : %.1f ppm\n", CO2_ppm);
  Serial.printf("🌬 Air    : %s\n", airQuality.c_str());
  Serial.printf("🔥 Fire   : %s (MQ2: %d)\n", fireLevel.c_str(), mq2Value);
  Serial.println("==========================\n");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message on Topic: ");
  Serial.println(topic);
  Serial.print("Content: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void connectToMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT Broker...");
    String clientId = "ESP32Client-" + String(WiFi.macAddress());
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Connected");
      mqttClient.subscribe("esp32/control");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

// =================== SETUP =====================
void setup() {
  Wire.begin(21, 22);
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  savedChatID = loadChatID();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (1);
  lcd.init(); lcd.backlight();
  if (!rtc.begin()) while (1);
  if (rtc.lostPower()) rtc.adjust(DateTime(__DATE__,__TIME__) + TimeSpan(0, 0, 3, 0));
  dht.begin(); if (!bme.begin(0x76)) while (1);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LAMP_PIN, OUTPUT);
  pinMode(GREEN_LAMP_PIN, OUTPUT);

  delay(10000);
  R0 = calibrateSensor();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  secured_client.setInsecure();
  connectToMQTT();

  if (savedChatID != "") {
    saveChatID(savedChatID);
    for (String id : chatIDs) {
      bot.sendMessage(id, "🤖 Bot is active with EEPROM!", "");
    }
  }

  for (String id : chatIDs) {
    bot.sendMessage(id, "🤖 PNEUMA IS READY!", "");
  }

  showTypingAnimation("PNEUMA", 3, 200);
}

// =================== LOOP =====================
void loop() {
  if (!mqttClient.connected()) connectToMQTT();
  mqttClient.loop();

  DateTime now = rtc.now() + TimeSpan(0, 1, 3, 0);
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float pressure = bme.readPressure() / 100.0F;
  float altitude = bme.readAltitude(1013.25);
  int mq135Value = analogRead(MQ135_PIN);
  int mq2Value = analogRead(MQ2_PIN);

  float CO2_ppm = 0;
  String airQuality = "Unknown";
  String fireLevel = (mq2Value >= MQ2_THRESHOLD) ? "Dangerous" : "Safe";
  String currentDay = daysOfTheWeek[now.dayOfTheWeek()];

  if (mq135Value > 0) {
    float RS = (4095.0 / mq135Value - 1.0) * R0;
    float ratio = RS / R0;
    CO2_ppm = 116.6020682 * pow(ratio, -2.769034857);
    if (CO2_ppm < 50) airQuality = "Excellent";
    else if (CO2_ppm < 100) airQuality = "Good";
    else if (CO2_ppm < 200) airQuality = "Moderate";
    else if (CO2_ppm < 300) airQuality = "Unhealthy";
    else airQuality = "Hazardous";
  }

  if (airQuality == "Hazardous" || mq2Value >= MQ2_THRESHOLD) {
    digitalWrite(GREEN_LAMP_PIN, LOW);
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH); digitalWrite(LAMP_PIN, HIGH);
      delay(200); digitalWrite(BUZZER_PIN, LOW); digitalWrite(LAMP_PIN, LOW);
      delay(200);
    }
  } else {
    digitalWrite(GREEN_LAMP_PIN, HIGH);
    digitalWrite(LAMP_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  mqttClient.publish("esp32/temperature", String(temperature).c_str());
  mqttClient.publish("esp32/humidity", String(humidity).c_str());
  mqttClient.publish("esp32/pressure", String(pressure).c_str());
  mqttClient.publish("esp32/altitude", String(altitude).c_str());
  mqttClient.publish("esp32/co2_ppm", String(CO2_ppm).c_str());
  mqttClient.publish("esp32/air_quality", airQuality.c_str());
  mqttClient.publish("esp32/fire_level", fireLevel.c_str());
  mqttClient.publish("esp32/mq2_value", String(mq2Value).c_str());
  mqttClient.publish("esp32/time", String(now.timestamp(DateTime::TIMESTAMP_TIME)).c_str());
  mqttClient.publish("esp32/date", String(now.timestamp(DateTime::TIMESTAMP_DATE)).c_str());
  mqttClient.publish("esp32/day", currentDay.c_str());

  if (millis() - lastBotSend > botInterval && !chatIDs.empty()) {
    String msg = "⏰ Hourly Report:\n";
    msg += "🕒 Time: " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + "\n";
    msg += "📅 Date: " + String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + "\n";
    msg += "📆 Day: " + currentDay + "\n\n";
    msg += "🌡 Temperature: " + String(temperature, 1) + " °C\n";
    msg += "💧 Humidity: " + String(humidity, 1) + " %\n";
    msg += "💨 Pressure: " + String(pressure, 1) + " hPa\n";
    msg += "🗻 Altitude: " + String(altitude, 1) + " m\n";
    msg += "🏭 CO₂: " + String(CO2_ppm, 1) + " ppm\n";
    msg += "🌬 Air Quality: " + airQuality + "\n";
    msg += "🔥 Fire Status: " + String((mq2Value >= MQ2_THRESHOLD) ? "🔥 Dangerous" : "✅ Safe") + "\n";
    for (String id : chatIDs) {
      bot.sendMessage(id, msg, "Markdown");
    }
    lastBotSend = millis();
  }

  // === FIRE NOTIFICATION ===
  if (mq2Value >= MQ2_THRESHOLD && millis() - lastFireSent > fireCooldown && !chatIDs.empty()) {
  DateTime now = rtc.now() + TimeSpan(0, 1, 3, 0);
  String currentDay = daysOfTheWeek[now.dayOfTheWeek()];

  char dateTimeBuf[40];
  snprintf(dateTimeBuf, sizeof(dateTimeBuf), "%02d-%02d-%04d %02d:%02d:%02d", 
           now.day(), now.month(), now.year(), 
           now.hour(), now.minute(), now.second());

  String fireMsg = "🚨 FIRE QUALITY ALERT!\n\n";
  fireMsg += "🔥 Air Contaminated Due to Fire!\n\n";
  fireMsg += "📆 Detected On: " + currentDay + ", " + String(dateTimeBuf) + "\n";
  fireMsg += "📉 Status: HAZARDOUS ⚠\n\n";
  fireMsg += "Smoke and toxic particles from the fire are detected in the air around you! It can contain PM2.5, carbon monoxide (CO), and other harmful chemicals.\n\n";
  fireMsg += "👥 Who is at Risk?\n";
  fireMsg += "Children, the elderly, pregnant women, asthmatics, and people with respiratory disorders are most vulnerable.\n\n";
  fireMsg += "🔧 Prevention & Mitigation Measures:\n";
  fireMsg += "✅ Use N95/KN95 mask when leaving the house\n";
  fireMsg += "✅ Close all windows and vents\n";
  fireMsg += "✅ Turn on the air purifier or fan with HEPA filter\n";
  fireMsg += "✅ Avoid outdoor activities as much as possible\n";
  fireMsg += "✅ Stick a wet cloth in the cracks of doors and windows to filter air\n";
  fireMsg += "✅ Drink more water to help detoxify the body\n";
  fireMsg += "✅ Monitor symptoms such as cough, sore throat, or shortness of breath\n";
  fireMsg += "✅ Immediately seek medical help if experiencing serious symptoms\n\n";
  fireMsg += "📲 Monitor Air Quality:\n";
  fireMsg += "💻 [Live Monitoring](https://bit.ly/pneuma-uiweb)\n\n";
  fireMsg += "💡 Important Note:\n";
  fireMsg += "• Don't light candles or cigarettes in the house\n";
  fireMsg += "• Minimize the use of gas stoves\n";
  fireMsg += "• Prepare an evacuation path if the fire spreads\n\n";
  fireMsg += "#FireAirAlert #SmokeDanger #IoT4Life #PneumaSmartAirMonitor";

  for (String id : chatIDs) {
    bot.sendMessage(id, fireMsg, "Markdown");
  }

  lastFireSent = millis();
}

//======= Air Quality Notification =========
    if (airQuality == "Hazardous" && millis() - lastDangerSent > dangerCooldown && !chatIDs.empty()) {
      DateTime now = rtc.now() + TimeSpan(0, 1, 3, 0);
String currentDay = daysOfTheWeek[now.dayOfTheWeek()];

char dateTimeBUf[40];
snprintf(dateTimeBUf, sizeof(dateTimeBUf), "%02d-%02d-%04d %02d:%02d:%02d", 
         now.day(), now.month(), now.year(),
         now.hour(), now.minute(), now.second());

String dangerMsg = "🚨 DANGEROUS AIR QUALITY ALERT!\n\n";
dangerMsg += "🏭 Air Quality is HAZARDOUS!\n\n";
dangerMsg += "📆 Detected On: " + currentDay + ", " + String(dateTimeBUf) + "\n";
dangerMsg += "📉 Status: DANGEROUS ⚠\n\n";
dangerMsg += "🔥 The air currently contains harmful particles or gases (e.g. high PM2.5, CO, or NO2).\n";
dangerMsg += "These conditions pose high health risks, especially for children, the elderly, and people with asthma.\n\n";
dangerMsg += "🔧 Solutions & Mitigation Measures:\n";
dangerMsg += "✅ Use an N95 or KN95 mask if you have to leave the house\n";
dangerMsg += "✅ Close all windows and doors to prevent polluted air from entering\n";
dangerMsg += "✅ Turn on the air purifier, filter fan, or exhaust fan\n";
dangerMsg += "✅ Avoid outdoor sports or physical activity\n";
dangerMsg += "✅ Use a wet cloth in the door/window gap as an emergency filter\n";
dangerMsg += "✅ Drink more water to keep your hydrated\n";
dangerMsg += "✅ Monitor the health condition of family and friends\n";
dangerMsg += "✅ If shortness of breath or chest pain occurs, seek medical attention immediately\n\n";
dangerMsg += "🕒 Air quality updates will be sent periodically.\n\n";
dangerMsg += "💡 Tips:\n";
dangerMsg += "- Click this [link](https://bit.ly/pneuma-uiweb) to check the monitoring website\n";
dangerMsg += "- Do not light candles, cigarettes, or gas stoves for too long indoors\n\n";
dangerMsg += "#PneumaSmartAirMonitor #AirAlert #IoT4Life";

    for (String id : chatIDs) {
      bot.sendMessage(id, dangerMsg, "Markdown");
    }

  lastDangerSent = millis();
}

  int newMessages = bot.getUpdates(bot.last_message_received + 1);
  while (newMessages) {
    for (int i = 0; i < newMessages; i++) {
      String text = bot.messages[i].text;
      String from = bot.messages[i].from_id;
      String user_name = bot.messages[i].from_name;  
      if (text == "/start") {
        saveChatID(from);
        String welcomeMsg = "🤖 *Welcome";
      if (user_name.length() > 0) {
        welcomeMsg += " " + user_name;
      }
        welcomeMsg += "* to Pneuma Telegram Bot Microcontroller Monitoring! 🤖\n\n";
        welcomeMsg += "To check the sensor status 🍃, please type /status\n";
        welcomeMsg += "To see the solution for air and environmental conditions 🛡, type /solution\n\n";
        welcomeMsg += "Need help?😊 try /help \n\n";
        welcomeMsg += "🌐 [Pneuma Website](https://pneumainventor.wixsite.com/pneuma)\n";
        welcomeMsg += "💻 [Live Monitoring](https://bit.ly/pneuma-uiweb)\n\n";
        welcomeMsg += "Best regards,\n*PNEUMA TEAM* 🤖";
        bot.sendMessage(from, welcomeMsg, "Markdown");
    }
      else if (text == "/status") {
        saveChatID(from);
        String msg = "📊 Sensor Status:\n";
        msg += "🕒 Time: " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + "\n";
        msg += "📅 Date: " + String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + "\n";
        msg += "📆 Day: " + currentDay + "\n\n";
        msg += "🌡 Temperature: " + String(temperature, 1) + " °C\n";
        msg += "💧 Humidity: " + String(humidity, 1) + " %\n";
        msg += "💨 Pressure: " + String(pressure, 1) + " hPa\n";
        msg += "🗻 Altitude: " + String(altitude, 1) + " m\n";
        msg += "🏭 CO₂: " + String(CO2_ppm, 1) + " ppm\n";
        msg += "🌬 Air Quality: " + airQuality + "\n";
        msg += "🔥 Fire Status: " + String((mq2Value >= MQ2_THRESHOLD) ? "🔥 Dangerous" : "✅ Safe") + "\n\n";
        msg += String("🌐 [Pneuma Website](https://pneumainventor.wixsite.com/pneuma)") + "\n";
        msg += "💻 [Live Monitoring](https://bit.ly/pneuma-uiweb)";
        bot.sendMessage(from, msg, "Markdown");
      }
    else if (text == "/solution") {
      saveChatID(from);
      String msg = "🛡 Environmental Safety Solutions\n\n";
      msg += "📌 General Recommendations:\n";
      msg += "1. 🪟 Improve ventilation by opening windows and doors.\n";
      msg += "2. 🌿 Use air purifiers or place indoor plants.\n";
      msg += "3. 🚭 Avoid smoking or burning indoors.\n";
      msg += "4. 🚪 Seal cracks in walls and doors to prevent gas leakage.\n";
      msg += "5. ⚠ Evacuate if high gas levels or fire detected.\n";
      msg += "6. ☎ Contact local authorities in case of emergency.\n\n";
      msg += "📊 Live Sensor-Based Actions:\n";

      if (airQuality == "Hazardous") {
      msg += "\n🏭 Air Quality: Hazardous\n";
      msg += "• Increase ventilation.\n";
      msg += "• Use air purifiers and indoor plants.\n";
      msg += "• Avoid using aerosols and smoking indoors.\n";
      msg += "• Stay indoors with filtered air when possible.\n";
      msg += "• Monitor air quality continuously.\n";
      }

      if (mq2Value >= MQ2_THRESHOLD) {
      msg += "\n🔥 Fire Gas Detected!\n";
      msg += "• Evacuate the area immediately.\n";
      msg += "• Ensure fire extinguishers are accessible.\n";
      msg += "• Contact emergency services.\n";
      msg += "• Shut off gas/electric sources if safe.\n";
      msg += "• Warn others nearby.\n";
      }

      if (airQuality != "Hazardous" && mq2Value < MQ2_THRESHOLD) {
      msg += "\n✅ Current Status: Safe\n";
      msg += "• All systems normal. Maintain good practices.\n";
      }

      msg += "\nStay safe and breathe clean air!\n*PNEUMA TEAM* 🤖";
  bot.sendMessage(from, msg, "Markdown");
}
    else if (text == "/help") {
      saveChatID(from);
      String msg = "📖 PNEUMA Help Menu\n\n";
      msg += "/start – Start interacting with the bot and register your chat.\n";
      msg += "/status – Show current sensor data like temperature, CO₂, humidity, fire status, etc.\n";
      msg += "/solution – Get solutions and actions based on current air quality and gas levels.\n";
      msg += "/help – Show this help message.\n\n";
      msg += "🌐 [Pneuma Website](https://pneumainventor.wixsite.com/pneuma)\n";
      msg += "💻 [Live Monitoring Dashboard](https://bit.ly/pneuma-uiweb)\n\n";
      msg += "🔔 Make sure you've clicked /start to register your chat for alerts.\n";
      msg += "🛡 Stay safe and informed!\n\n";
      msg += "PNEUMA TEAM 🤖";

      bot.sendMessage(from, msg, "Markdown");
}
    }
    newMessages = bot.getUpdates(bot.last_message_received + 1);
  }

  lcd.setCursor(0, 0);
  lcd.printf("Time: %02d:%02d:%02d", now.hour(), now.minute(), now.second());
  lcd.setCursor(0, 1);
  lcd.printf("Date: %02d/%02d/%d", now.day(), now.month(), now.year());
  lcd.setCursor(0, 2);
  lcd.print("Day : ");
  lcd.print(daysOfTheWeek[now.dayOfTheWeek()]);
  lcd.setCursor(0, 3);
  lcd.print("                    ");

  updateDisplay(now, temperature, humidity, pressure, altitude, CO2_ppm, airQuality, fireLevel, mq2Value);
  printToSerial(now, temperature, humidity, pressure, altitude, CO2_ppm, airQuality, fireLevel, mq2Value);

  delay(2500);
}