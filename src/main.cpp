#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"

// WiFi Bilgileri
const char* ssid = "admin";
const char* password = "ia2323Ai";

// MQTT Sunucusu
const char* mqtt_server = "192.168.1.109";

WiFiClient espClient;
PubSubClient client(espClient);

// DHT Ayarları
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// OLED Ayarları
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// GPIO LED'ler
#define LED_MID 19
#define LED_LOW 18
#define LED_HIGH 5
#define LED_HIGH_CHANNEL 0

// NTP Zaman ayarları
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;

// Zaman değişkenleri
unsigned long lastDisplaySwitch = 0;
bool showClock = true;

void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT bağlantısı deneniyor...");
    if (client.connect("ESP32_Client")) {
      Serial.println("bağlandı.");
    } else {
      Serial.print("bağlanamadı. Hata kodu: ");
      Serial.print(client.state());
      Serial.println(" 5 sn sonra tekrar denenecek...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_MID, OUTPUT);
  pinMode(LED_LOW, OUTPUT);

  ledcSetup(LED_HIGH_CHANNEL, 5000, 8);
  ledcAttachPin(LED_HIGH, LED_HIGH_CHANNEL);
  ledcWrite(LED_HIGH_CHANNEL, 0);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 başlatılamadı"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  WiFi.begin(ssid, password);
  Serial.print("Wi-Fi bağlantısı deneniyor...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi bağlı!");
  Serial.println("IP adresi: " + WiFi.localIP().toString());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {
    if (temperature < 25) {
      digitalWrite(LED_MID, LOW);
      digitalWrite(LED_LOW, HIGH);
      ledcWrite(LED_HIGH_CHANNEL, 0);
    } else if (temperature < 27) {
      digitalWrite(LED_MID, HIGH);
      digitalWrite(LED_LOW, LOW);
      ledcWrite(LED_HIGH_CHANNEL, 0);
    } else {
      digitalWrite(LED_MID, LOW);
      digitalWrite(LED_LOW, LOW);
      ledcWrite(LED_HIGH_CHANNEL, 180);
    }

    String payload = "{\"sicaklik\":";
    payload += String(temperature, 1);
    payload += ", \"nem\":";
    payload += String(humidity, 1);
    payload += "}";
    client.publish("house/esp32-1", payload.c_str());
    Serial.println("MQTT veri gönderildi: " + payload);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastDisplaySwitch > 5000) {
    showClock = !showClock;
    lastDisplaySwitch = currentMillis;
  }

  display.clearDisplay();

  if (showClock) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[16];
      char dateStr[16];
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
      strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);

      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("Saat:");
      display.setTextSize(3);
      display.setCursor(0, 18);
      display.println(timeStr);
      display.setTextSize(1);
      display.setCursor(19, 55);
      display.print("Tarih: ");
      display.println(dateStr);
    } else {
      display.setCursor(0, 0);
      display.println("Zaman alinamadi.");
    }
  } else {
    if (!isnan(temperature) && !isnan(humidity)) {
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.print("Sicaklik:\n");
      display.setCursor(0,21);
      display.print(temperature, 1);
      display.println("C");

      display.setCursor(0, 40);
      display.print("Nem: ");
      display.print(humidity, 1);
      display.println("%");
    } else {
      display.setCursor(0, 0);
      display.println("Sensor okunamadi!");
    }
  }

  display.display();

  delay(100);  // Düşük gecikme ile ekran animasyon gibi akar
}
