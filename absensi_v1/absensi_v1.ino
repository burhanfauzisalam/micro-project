#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// =========================
// PIN ESP32
// =========================
// #define LED_RED 12
// #define LED_GREEN 14
// #define LED_YELLOW 13
// #define BUZZER 33

#define LED_RED 4
#define LED_GREEN 5
#define LED_YELLOW 15
#define BUZZER 19
#define BUTTON_PIN 14
unsigned long lastButtonPress = 0;

#define SDA_PIN 21
#define SCL_PIN 22

// PN532
#define PN532_IRQ   2
#define PN532_RESET 15

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================
// KARTU SPESIAL
// =========================
const String WIFI_RESET_CARD = "09E6C701";
const String MODE_SWITCH_CARD = "E2FEFD03"; // GANTI ke UID mode Anda

// =========================
// MODE & API URL
// =========================
int currentMode = 2;

const char* api_url_mode1 = "http://192.168.229.69/sdkhadijah-apps/api/attendance/register-card";
const char* api_url_mode2 = "http://192.168.229.69/sdkhadijah-apps/api/attendance/scan";

const char* deviceId = "sdkhw1";

// =========================
// SCROLL LCD
// =========================
void scrollText(int row, String text, int speed = 250) {
  lcd.setCursor(0, row);

  if (text.length() <= 16) {
    lcd.print(text);
    return;
  }

  for (int i = 0; i <= text.length() - 16; i++) {
    lcd.setCursor(0, row);
    lcd.print(text.substring(i, i + 16));
    delay(speed);
  }
}

// =========================
// READY SCREEN
// =========================
void showReadyScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (currentMode == 1) lcd.print("Pendaftaran");
  else lcd.print("Absensi");

  lcd.setCursor(0, 1);
  lcd.print("Scan Kartu...");
}

// =========================
// TAMPILKAN MODE
// =========================
void showMode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mode Sekarang:");

  lcd.setCursor(0, 1);
  if (currentMode == 1) lcd.print("Pendaftaran");
  else lcd.print("Absensi");

  delay(2000);
}

// =========================
// BUZZER
// =========================
void bipBip() {
  digitalWrite(BUZZER, HIGH); delay(250);
  digitalWrite(BUZZER, LOW);  delay(150);
  digitalWrite(BUZZER, HIGH); delay(250);
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

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_RED, HIGH);

  lcd.init();
  lcd.backlight();
  lcd.print("Mulai WiFi...");

  WiFiManager wm;
  if (!wm.autoConnect("ESP32-RFID")) {
    lcd.clear();
    lcd.print("WiFi gagal!");
    delay(2000);
    ESP.restart();
  }

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, HIGH);

  lcd.clear();
  lcd.print("WiFi OK!");
  lcd.setCursor(1, 1);
  lcd.print(WiFi.localIP());
  delay(2000);

  Wire.begin(SDA_PIN, SCL_PIN);

  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    lcd.clear();
    lcd.print("PN532 ERROR!");
    while (1);
  }
  nfc.SAMConfig();

  showMode();
  showReadyScreen();
}

// =========================
// HANDLE RESPONSE MODE 1
// =========================
void handleMode1Response(StaticJsonDocument<256> &doc) {
  String status = doc["status"].as<String>();

  // SUCCESS
  if (status == "success") {
    lcd.clear();
    lcd.print(doc["message"].as<String>());
    digitalWrite(LED_GREEN, HIGH);
    bipBip();
    scrollText(1, doc["name"].as<String>());
    delay(2000);
    digitalWrite(LED_GREEN, LOW);
    return;
  }

  // NO STUDENT
  if (doc["error_code"] == "NO_STUDENT") {
    lcd.clear();
    lcd.print("Gagal!");
    digitalWrite(LED_RED, HIGH);
    bipBip();
    scrollText(1, doc["message"].as<String>());
    digitalWrite(LED_RED, LOW);
    return;
  }

  // ALREADY REGISTERED
  if (doc["error_code"] == "ALREADY_REGISTERED") {
    lcd.clear();
    lcd.print(doc["message"].as<String>());
    digitalWrite(LED_RED, HIGH);
    bipBip();
    scrollText(1, doc["name"].as<String>());
    delay(2000);
    digitalWrite(LED_RED, LOW);
    return;
  }

  // UNKNOWN
  lcd.clear();
  lcd.print("Error API");
  scrollText(1, "Kesalahan tidak dikenal");
}

// =========================
// HANDLE RESPONSE MODE 2
// =========================
void handleMode2Response(StaticJsonDocument<256> &doc) {
  String status = doc["status"].as<String>();

  // SUCCESS
  if (status == "success") {
    lcd.clear();
    lcd.print(doc["message"].as<String>());
    digitalWrite(LED_GREEN, HIGH);
    bipBip();
    scrollText(1, doc["name"].as<String>());
    delay(2000);
    digitalWrite(LED_GREEN, LOW);
    return;
  }

  // ALREADY
  if (doc["error_code"] == "ALREADY") {
    lcd.clear();
    lcd.print(doc["message"].as<String>());
    digitalWrite(LED_GREEN, HIGH);
    bipBip();
    scrollText(1, doc["name"].as<String>());
    delay(2000);
    digitalWrite(LED_GREEN, LOW);
    return;
  }

  lcd.clear();
  lcd.print("Error API");
}

// =========================
// LOOP
// =========================
void loop() {

  // LED status
  digitalWrite(LED_RED, WiFi.status() != WL_CONNECTED);
  digitalWrite(LED_YELLOW, WiFi.status() == WL_CONNECTED);

  // =============================
  // CEK TOMBOL MODE (GPIO 14)
  // =============================
  if (digitalRead(BUTTON_PIN) == LOW) {  
    if (millis() - lastButtonPress > 600) { // debounce
      lastButtonPress = millis();

      currentMode = (currentMode == 1) ? 2 : 1;

      lcd.clear();
      lcd.print("Mode Diubah");
      lcd.setCursor(0, 1);
      lcd.print(currentMode == 1 ? "Pendaftaran" : "Absensi");

      Serial.println("MODE CHANGE BTN: " + String(currentMode));

      delay(1500);
      showReadyScreen();
    }
  }

  uint8_t uid[7];
  uint8_t uidLength;

  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    delay(20);
    return;
  }

  // Convert UID
  String uidString = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    char buf[3];
    sprintf(buf, "%02X", uid[i]);
    uidString += buf;
  }

  Serial.println("SCAN UID: " + uidString);
  lcd.setCursor(0, 1);
  lcd.print("Memproses...   ");

  // ============ RESET WIFI ============
  if (uidString == WIFI_RESET_CARD) {
    lcd.clear();
    lcd.print("Reset WiFi...");
    WiFi.disconnect(true, true);
    delay(1500);
    ESP.restart();
  }

  // ============ GANTI MODE ============
  if (uidString == MODE_SWITCH_CARD) {
    currentMode = (currentMode == 1) ? 2 : 1;

    lcd.clear();
    lcd.print("Mode Diubah!");
    lcd.setCursor(0, 1);
    lcd.print(currentMode == 1 ? "Pendaftaran" : "Absensi");

    Serial.println("MODE NOW: " + String(currentMode));

    delay(2000);
    showReadyScreen();
    return;
  }

  // ============ KIRIM DATA ============
  if (WiFi.status() != WL_CONNECTED) {
    lcd.print("WiFi Error!");
    delay(1500);
    showReadyScreen();
    return;
  }

  HTTPClient http;

  // Tentukan URL berdasarkan mode
  String url = (currentMode == 1) ? api_url_mode1 : api_url_mode2;

  http.begin(url);
  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/json");

  // Payload
  StaticJsonDocument<128> json;
  json["rfid_uid"] = uidString;
  json["device_name"] = deviceId;

  String payload;
  serializeJson(json, payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    String response = http.getString();
    Serial.println(response);

    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, response)) {

      if (currentMode == 1) handleMode1Response(doc);
      else handleMode2Response(doc);

    } else {
      lcd.clear();
      lcd.print("JSON Error!");
    }
  } else {
    lcd.clear();
    lcd.print("Gagal Kirim!");
  }

  http.end();
  showReadyScreen();
}
