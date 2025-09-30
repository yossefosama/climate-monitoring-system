#include "DHT.h"
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include "config.example.h" // WiFi + Firebase keys

#define DHTPIN 5
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;

void setup() {
  Serial.begin(9600);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected. IP: " + WiFi.localIP().toString());

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  if (Firebase.failed()) {
    Serial.println("Firebase init failed!");
  }

  dht.begin();
}

void loop() {
  int humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Calculate vapor pressure
  float vp = getVaporPressure((int)temperature);
  if (vp < 0) return;

  vp *= 1.33322; // torr → mbar
  float humidityRatio = calculateHumidityRatio(vp, humidity);

  // Upload to Firebase
  Firebase.setFloat("Climate/Temperature", temperature);
  Firebase.setInt("Climate/Humidity", humidity);
  Firebase.setFloat("Climate/HumidityRatio", humidityRatio);

  // Upload to ThingSpeak
  uploadToThingSpeak(temperature, humidity, humidityRatio);

  delay(400);
}




float getVaporPressure(int temperatureC) {
  static const float vpTable[] = {
    0, 9.2, 9.8, 10.5, 11.2, 12.0, 12.8, 13.6, 14.5, 15.5, // 10–19
    16.5, 17.5, 18.7, 19.8, 21.1, 22.4, 23.8, 25.2, 26.7, 28.4, // 20–29
    30.0, 31.8, 33.7, 35.7, 37.7, 39.9, 42.2, 44.6, 47.1, 49.7, // 30–39
    52.4, 55.3, 58.3, 61.5, 64.8, 68.3, 71.9, 75.7, 79.6, 83.7, // 40–49
    88.0, 92.5, 97.2, 102.1, 107.2, 112.5, 118.0, 123.8, 129.8, 136.1, // 50–59
    142.6, 149.4 // 60–61
  };

  if (temperatureC < 10 || temperatureC > 60) return -1; // out of table
  return vpTable[temperatureC - 9]; // offset because table starts at 10
}



float calculateHumidityRatio(float vp, float humidityPercent) {
  // constants
  const float pressure = 1013.0; // mbar
  return ((0.622 * vp) / (pressure - (0.378 * vp)) + 0.0006) * (humidityPercent / 100.0);
}
