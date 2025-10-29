#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <time.h>

// === PIN DEFINISI ===
#define LED_RED 4      // LED merah (ubah sesuai pin kamu)
#define LED_GREEN 5    // LED hijau
#define BUZZER 19       // pin buzzer aktif HIGH
#define SDA_PIN 21
#define SCL_PIN 22

// === OBJEK ===
Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === KONFIGURASI API ===
const char* api_url = "https://sdkhadijahwonorejo.sch.id/api/attendance/register-card";

// === KONFIGURASI WAKTU ===
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

int lastMinute = -1;
unsigned long lastNtpSync = 0;
const unsigned long ntpSyncInterval = 60 * 1000; // sinkron tiap 1 menit

// === FUNGSI WAKTU ===
void sinkronNTP() {
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.println("⏰ NTP sinkron ulang!");
    } else {
      Serial.println("⚠️  Gagal sinkron NTP");
    }
  }
}

void tampilkanWaktu() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_min != lastMinute) {
      lastMinute = timeinfo.tm_min;
      char timeStr[6];
      strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
      lcd.setCursor(0, 0);
      lcd.print("      ");
      lcd.setCursor(0, 0);
      lcd.print(timeStr);
      Serial.print("Jam: ");
      Serial.println(timeStr);
    }
  }
}

// === SETUP ===
void setup() {
  Serial.begin(115200);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED_RED, HIGH);   // default: merah nyala sampai WiFi tersambung
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(BUZZER, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Mulai WiFi...");

  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("ESP32-RFID")) {
    lcd.clear();
    lcd.print("WiFi gagal!");
    delay(3000);
    ESP.restart();
  }

  // WiFi berhasil
  digitalWrite(LED_RED, LOW); // matikan LED merah
  lcd.clear();
  lcd.print("WiFi OK!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(2000);

  sinkronNTP();
  lastNtpSync = millis();

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    lcd.clear();
    lcd.print("PN532 Error!");
    while (1);
  }
  nfc.SAMConfig();

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Scan Kartu...");
}

// === LOOP ===
void loop() {
  tampilkanWaktu();

  // Sinkron ulang waktu tiap 1 menit
  if (millis() - lastNtpSync >= ntpSyncInterval) {
    sinkronNTP();
    lastNtpSync = millis();
  }

  // Jika WiFi terputus → LED merah nyala terus
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_RED, HIGH);
  } else {
    digitalWrite(LED_RED, LOW);
  }

  uint8_t uid[7];
  uint8_t uidLength;

  // === PEMBACAAN KARTU ===
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    String uidString = "";
    char buf[3];
    for (uint8_t i = 0; i < uidLength; i++) {
      sprintf(buf, "%02X", uid[i]);
      uidString += String(buf);
    }

    Serial.println("Kartu: " + uidString);
    lcd.setCursor(0, 1);
    lcd.print("Memproses data...  ");

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

          // ✅ BERHASIL → LED HIJAU + BUZZER pendek
          digitalWrite(LED_GREEN, HIGH);
          bipBip(); 
          delay(500);
          digitalWrite(LED_GREEN, LOW);
        } else {
          lcd.print("Parse JSON Error");
        }
      } else {
        // ❌ GAGAL kirim → LED HIJAU nyala 2 detik + buzzer panjang
        lcd.print("Gagal kirim!");
        Serial.printf("Error kirim: %d\n", httpResponseCode);
        digitalWrite(LED_GREEN, HIGH);
        tone(BUZZER, 1000, 500);  // bunyi panjang
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

  delay(100);
}

void bipBip() {
  // bunyi pertama
  digitalWrite(BUZZER, HIGH);   // aktifkan buzzer (LOW kalau kamu hubungkan negatif ke pin)
  delay(250);                  // durasi bunyi
  digitalWrite(BUZZER, LOW);  // matikan buzzer
  delay(150);                  // jeda antar bunyi

  // bunyi kedua
  digitalWrite(BUZZER, HIGH);
  delay(250);
  digitalWrite(BUZZER, LOW);
}

