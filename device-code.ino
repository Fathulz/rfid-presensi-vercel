#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// --- KONFIGURASI PIN RFID & HARDWARE ---
#define RST_PIN     0   // D3 (GPIO0)[cite: 1, 3]
#define SS_PIN      2   // D4 (GPIO2)[cite: 1, 3]
const int buzzerPin = 15; // D8 (GPIO15)
const int modePin = 14;   // D5 (GPIO14) — tombol mode, INPUT_PULLUP ke GND

// Mode: false = presensi, true = baca UID. Persist di EEPROM addr 0.
bool modeBaca = false;

// Detik terakhir yang sudah digambar di LCD (-1 = perlu gambar ulang, -2 = NTP belum sync)
int jamTerakhir = -1;

// Inisialisasi Objek RFID dan LCD (Alamat I2C umum: 0x27 atau 0x3F)
MFRC522 rfid(SS_PIN, RST_PIN); 
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- KREDENSIAL JARINGAN & ENDPOINT ---
const char* ssid     = "KALONG 5G";
const char* password = "";

// Ganti dengan endpoint server baru Anda
const char* serverEndpoint = "https://rfid-presensi.vercel.app/api/presensi"; 

void setup() {
  // Inisialisasi Serial Monitor untuk debugging
  Serial.begin(115200);
  delay(1000);

  // Inisialisasi Pin Buzzer
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Inisialisasi tombol mode (pull-up, terhubung ke GND)
  pinMode(modePin, INPUT_PULLUP);

  // Baca mode terakhir dari EEPROM
  EEPROM.begin(4);
  modeBaca = (EEPROM.read(0) == 1);

  // Inisialisasi LCD
  lcd.init();                      
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Presensi");
  lcd.setCursor(0, 1);
  lcd.print("Memulai Koneksi..");

  // Inisialisasi Modul SPI & RFID
  SPI.begin();
  rfid.PCD_Init();

  // Koneksi ke Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWi-Fi Terhubung!");

  // Sinkron jam via NTP (WITA / UTC+8, sama dengan timezone server)
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Terhubung!");
  delay(2000);
  
  // Tampilkan layar siap / siaga
  tampilkanSiaga();
}

void loop() {
  cekTombol();
  updateJam();

  // Cek apakah ada kartu baru yang menempel
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  // Ambil UID Kartu dan ubah ke format String Hexadesimal
  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  Serial.print("UID Kartu Terbaca: ");
  Serial.println(uidString);

  // Tampilkan UID terbaca dulu di LCD sebelum kirim ke server
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID: ");
  lcd.print(uidString);
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("Memproses...");

  // Kirim data ke endpoint server jika Wi-Fi terhubung
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // skip validasi sertifikat — cukup buat testing
    HTTPClient http;

    http.begin(client, serverEndpoint);
    http.addHeader("Content-Type", "application/json");

    // Format payload JSON yang dikirim ke server
    String httpRequestData = modeBaca
      ? "{\"uid_kartu\":\"" + uidString + "\",\"mode\":\"read\"}"
      : "{\"uid_kartu\":\"" + uidString + "\"}";
    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println("Respon Server: " + response);

      // Parsing data JSON dari respon server
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        String status = doc["status"];
        String nama = doc["nama"];

        lcd.clear();
        if (modeBaca) {
          // Mode baca: konfirmasi UID terkirim
          if (status == "read") {
            lcd.setCursor(0, 0);
            lcd.print(uidString);
            lcd.setCursor(0, 1);
            lcd.print("UID Terkirim");
            digitalWrite(buzzerPin, HIGH); delay(80); digitalWrite(buzzerPin, LOW);
          } else {
            lcd.setCursor(0, 0);
            lcd.print("Gagal Kirim");
            errorBeep();
          }
        } else if (status == "success") {
          // Menampilkan format nama dan pesan berhasil diabsen pada LCD
          lcd.setCursor(0, 0);
          lcd.print(nama);
          lcd.setCursor(0, 1);
          lcd.print("Berhasil Diabsen");
          
          // Indikator Suara Sukses (Beep 2 kali)
          suksesBeep();
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Kartu Ditolak");
          lcd.setCursor(0, 1);
          lcd.print("Tidak Dikenali");
          
          // Indikator Suara Gagal (Beep panjang)
          errorBeep();
        }
      } else {
        Serial.println("Gagal parsing JSON!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Format Respon");
        lcd.setCursor(0, 1);
        lcd.print("Server Salah");
        errorBeep();
      }
    } else {
      Serial.print("HTTP Error Code: ");
      Serial.println(httpResponseCode);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Koneksi Server");
      lcd.setCursor(0, 1);
      lcd.print("Gagal / Error");
      errorBeep();
    }

    http.end(); // Tutup sesi HTTP
  } else {
    Serial.println("Wi-Fi Terputus!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Terputus!");
  }

  // Hentikan komunikasi kartu sementara agar tidak terbaca beruntun
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  // Jeda waktu agar pesan di LCD terbaca user sebelum kembali ke mode siaga (3 detik)
  delay(3000); 
  tampilkanSiaga();
}

// Fungsi tampilan standby / siaga (tampilkan mode aktif)
void tampilkanSiaga() {
  lcd.clear();
  if (modeBaca) {
    lcd.setCursor(0, 0);
    lcd.print(tengah("deviceId"));
  } else {
    lcd.setCursor(0, 0);
    lcd.print(tengah("--:--:--"));
    jamTerakhir = -1;
  }
  lcd.setCursor(0, 1);
  lcd.print(tengah("Tap Kartu Anda"));
  if (!modeBaca) updateJam();
}

// Rata tengah di layar 16 kolom
String tengah(const String& teks) {
  int pad = (16 - teks.length()) / 2;
  String hasil = "";
  for (int i = 0; i < pad; i++) hasil += " ";
  return hasil + teks;
}

// Gambar jam di baris 0, refresh tiap detik (mode presensi saja)
void updateJam() {
  if (modeBaca) return;
  time_t now = time(nullptr);
  if (now < 1000000000) { // NTP belum sinkron
    if (jamTerakhir != -2) {
      lcd.setCursor(0, 0);
      lcd.print(tengah("--:--:--"));
      jamTerakhir = -2;
    }
    return;
  }
  struct tm* t = localtime(&now);
  if (t->tm_sec == jamTerakhir) return;
  jamTerakhir = t->tm_sec;
  char jam[9];
  strftime(jam, sizeof(jam), "%H:%M:%S", t);
  lcd.setCursor(0, 0);
  lcd.print(tengah(jam));
}

// Cek tombol mode: tekan pendek = ganti mode, tahan >= 3 detik = restart
void cekTombol() {
  if (digitalRead(modePin) != LOW) return;
  unsigned long tekanMulai = millis();
  while (digitalRead(modePin) == LOW) {
    if (millis() - tekanMulai >= 3000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Restart...");
      ESP.restart();
    }
    delay(10);
  }
  delay(30); // debounce setelah tombol dilepas
  gantiMode();
}

// Ganti mode dan simpan ke EEPROM
void gantiMode() {
  modeBaca = !modeBaca;
  EEPROM.write(0, modeBaca ? 1 : 0);
  EEPROM.commit();
  Serial.println(modeBaca ? "Mode: BACA UID" : "Mode: PRESENSI");
  tampilkanSiaga();
}

// Fungsi Buzzer Sukses
void suksesBeep() {
  digitalWrite(buzzerPin, HIGH); delay(80);
  digitalWrite(buzzerPin, LOW);  delay(80);
  digitalWrite(buzzerPin, HIGH); delay(80);
  digitalWrite(buzzerPin, LOW);
}

// Fungsi Buzzer Gagal / Error
void errorBeep() {
  digitalWrite(buzzerPin, HIGH); delay(400);
  digitalWrite(buzzerPin, LOW);
}