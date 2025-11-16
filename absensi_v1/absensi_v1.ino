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
const char* api_url = "https://sdkhadijahwonorejo.sch.id/api/attendance/register-card";

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

  // =========================
  // WiFi Manager
  // =========================
  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("ESP32-RFID")) {
    lcd.clear();
    lcd.print("WiFi gagal!");
    delay(3000);
    ESP.restart();
  }

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, HIGH);

  lcd.clear();
  lcd.print("WiFi OK!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(2000);

  // =========================
  // I2C
  // =========================
  Wire.begin(SDA_PIN, SCL_PIN);

  // =========================
  // PN532
  // =========================
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();

  if (!versiondata) {
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

  // Status WiFi indicator
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
    String uidString = "";
    char buf[3];

    for (uint8_t i = 0; i < uidLength; i++) {
      sprintf(buf, "%02X", uid[i]);
      uidString += String(buf);
    }

    Serial.println("Kartu: " + uidString);

    lcd.setCursor(0, 1);
    lcd.print("Memproses...     ");

    // ===========================================
    //   KARTU KHUSUS UNTUK RESET WIFI & ESP32
    // ===========================================
    if (uidString == "09E6C701") {
      Serial.println("Kartu Reset Terdeteksi!");
      lcd.setCursor(0, 1);
      lcd.print("Reset WiFi...   ");

      // BIP
      digitalWrite(BUZZER, HIGH);
      delay(200);
      digitalWrite(BUZZER, LOW);

      delay(500);

      WiFi.disconnect(true, true); // hapus WiFi
      delay(1000);

      ESP.restart();
    }
    
    // Kirim ke API
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(api_url);
      http.addHeader("Content-Type", "application/json");

      String jsonPayload = "{\"rfid_uid\":\"" + uidString + "\"}";
      int httpResponseCode = http.POST(jsonPayload);

      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);

      if (httpResponseCode > 0) {
        String response = http.getString();

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, response);

        if (!error && doc.containsKey("message")) {

          String msg = doc["message"].as<String>();
          if (msg.length() > 16) msg = msg.substring(0, 16);

          lcd.print(msg);
          Serial.println("Pesan API: " + msg);

          digitalWrite(LED_GREEN, HIGH);
          bipBip();
          delay(500);
          digitalWrite(LED_GREEN, LOW);

        } else {
          lcd.print("JSON Error");
        }

      } else {
        lcd.print("Gagal kirim!");
        Serial.printf("Error kirim: %d\n", httpResponseCode);
        digitalWrite(LED_GREEN, HIGH);
        tone(BUZZER, 1000, 500);
        delay(2000);
        digitalWrite(LED_GREEN, LOW);
        noTone(BUZZER);
      }

      http.end();

    } else {
      lcd.print("WiFi Error!");
    }

    lcd.setCursor(0, 1);
    lcd.print("Scan Kartu...   ");
    delay(500);
  }

  delay(50);
}
