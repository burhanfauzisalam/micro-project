#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// =========================
// PIN DEFINISI ESP32
// =========================
#define LED_RED 12
#define LED_GREEN 14
#define LED_YELLOW 13
#define BUZZER 33

#define SDA_PIN 21
#define SCL_PIN 22

// === PIN PN532 (MODE I2C) ===
#define PN532_IRQ   2
#define PN532_RESET 15

// =========================
// OBJEK
// =========================
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================
// KONFIG API
// =========================
const char* api_url = "http://192.168.229.69/sdkhadijah-apps/api/attendance/register-card";
const char* deviceId = "sdkhw1";

// =========================
// SCROLL TEXT (LCD)
// =========================
void scrollText(LiquidCrystal_I2C &lcd, int row, String text, int delayTime = 250) {
  int len = text.length();

  if (len <= 16) {
    lcd.setCursor(0, row);
    lcd.print(text);
    return;
  }

  for (int pos = 0; pos <= len - 16; pos++) {
    lcd.setCursor(0, row);
    lcd.print(text.substring(pos, pos + 16));
    delay(delayTime);
  }
}

// =========================
// BUZZER
// =========================
void bipBip() {
  digitalWrite(BUZZER, HIGH);
  delay(250);
  digitalWrite(BUZZER, LOW);
  delay(150);
  digitalWrite(BUZZER, HIGH);
  delay(250);
  digitalWrite(BUZZER, LOW);
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(BUZZER, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Mulai WiFi...");

  // WiFi Manager
  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("ESP32-RFID")) {
    lcd.clear();
    lcd.print("WiFi gagal!");
    delay(2000);
    ESP.restart();
  }

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, HIGH);

  lcd.clear();
  lcd.print("WiFi OK!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(2000);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // PN532
  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    lcd.clear();
    lcd.print("PN532 Error!");
    Serial.println("PN532 tidak terdeteksi!");
    while (1) delay(10);
  }

  nfc.SAMConfig();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready...");
  lcd.setCursor(0, 1);
  lcd.print("Scan Kartu...");
}

// =========================
// LOOP
// =========================
void loop() {

  // Status LED WiFi
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_YELLOW, LOW);
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, HIGH);
  }

  uint8_t uid[7];
  uint8_t uidLength;

  // =========================
  // BACA KARTU RFID
  // =========================
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {

    // Convert UID ke HEX String
    String uidString = "";
    char buf[3];
    for (uint8_t i = 0; i < uidLength; i++) {
      sprintf(buf, "%02X", uid[i]);
      uidString += String(buf);
    }

    Serial.println("Kartu: " + uidString);

    lcd.setCursor(0, 1);
    lcd.print("Memproses...     ");

    // ===========================
    // KARTU RESET WIFI
    // ===========================
    if (uidString == "09E6C701") {
      Serial.println("Kartu Reset!");
      lcd.setCursor(0, 1);
      lcd.print("Reset WiFi...   ");

      digitalWrite(BUZZER, HIGH); delay(200);
      digitalWrite(BUZZER, LOW);

      WiFi.disconnect(true, true);
      delay(1000);
      ESP.restart();
    }

    // ===========================
    // KIRIM KE API
    // ===========================
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(api_url);
      http.addHeader("Content-Type", "application/json");

      // JSON Payload
      StaticJsonDocument<128> json;
      json["rfid_uid"] = uidString;
      json["device_name"] = deviceId;

      String jsonPayload;
      serializeJson(json, jsonPayload);

      Serial.println("payload: " + jsonPayload);

      int httpCode = http.POST(jsonPayload);

      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);

      if (httpCode > 0) {
        String response = http.getString();
        Serial.println("RESPONSE: " + response);

        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, response);

        if (!err) {

            String status = doc["status"].as<String>();

            // =======================
            // SUCCESS
            // =======================
            if (status == "success") {

                String msg  = doc["message"].as<String>();
                String name = doc["name"].as<String>();

                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(msg);

                Serial.println("SUCCESS: " + msg);

                digitalWrite(LED_GREEN, HIGH);
                bipBip();

                scrollText(lcd, 1, name);
                delay(2000);

                digitalWrite(LED_GREEN, LOW);
            }

            // =======================
            // ERROR: NO_STUDENT
            // =======================
            else if (doc["error_code"] == "NO_STUDENT") {

                String msg = doc["message"].as<String>();

                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Gagal!");

                Serial.println("ERROR NO_STUDENT: " + msg);

                digitalWrite(LED_RED, HIGH);
                bipBip();

                scrollText(lcd, 1, msg);
                delay(2000);

                digitalWrite(LED_RED, LOW);
            }

            // =======================
            // ERROR: CARD_REGISTERED
            // =======================
            else if (doc["error_code"] == "ALREADY_REGISTERED") {

                String msg  = doc["message"].as<String>();
                String name = doc["name"].as<String>();

                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(msg);

                Serial.println("CARD_REGISTERED: " + msg);

                digitalWrite(LED_RED, HIGH);
                bipBip();

                scrollText(lcd, 1, name);
                delay(2000);

                digitalWrite(LED_RED, LOW);
            }

            // =======================
            // ERROR: Tidak dikenali
            // =======================
            else {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Error API");

                Serial.println("UNKNOWN ERROR");

                scrollText(lcd, 1, "Kesalahan tidak dikenal");
                delay(2000);
            }

        } else {
            lcd.clear();
            lcd.print("JSON Error!");
            Serial.println("JSON PARSE ERROR");
            delay(1500);
        }

    } else {
        lcd.clear();
        lcd.print("Gagal Kirim!");
        Serial.printf("HTTP ERROR: %d\n", httpCode);
        delay(1500);
    }

    http.end();

    } else {
      lcd.print("WiFi Error!");
    }

    // kembali ke mode scan kartu
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ready...");
    lcd.setCursor(0, 1);
    lcd.print("Scan Kartu...");
  }

  delay(50);
}
