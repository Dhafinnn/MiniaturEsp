#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

// Pin LED
const int led1 = 32;
const int led2 = 33;
const int led3 = 25;
const int led4 = 26;
const int fan = 19;

// Pin LED di array
int ledPins[] = {led1, led2, led3, led4, fan};

// Pin dan tipe DHT
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// WiFi dan MQTT
const char* ssid = "Hematech Nusantara";
const char* password = "hematech123";
const char* mqttServer = "2e51f35445d7476b8552cdfb74edc1d6.s1.eu.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "house_";
const char* mqttPassword = "Trymqtt2";
const char* mqttTopic = "hematech/data"; // Satu topik untuk semua data
const char* mqttControlTopic = "hematech/led_control"; // Topik untuk kontrol LED

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Pin untuk sensor magnetik
const int sensorPin = 18;
bool lastDoorStatus = false;

// Variabel LED
int LED_STATE[] = {0, 0, 0, 0, 0};

// Fungsi koneksi WiFi
void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Fungsi untuk membaca sensor dan mengirim data ke MQTT
void publishData() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  bool doorStatus = lastDoorStatus;

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  String payloadJson = "{";
  payloadJson += "\"door\": \"" + String(doorStatus ? 0 : 1) + "\",";
  payloadJson += "\"temperature\": " + String(t) + ",";
  payloadJson += "\"humidity\": " + String(h) + ",";
  payloadJson += "\"led\": [";
  for (int i = 0; i < 5; i++) {
    payloadJson += String(LED_STATE[i]);
    if (i < 4) payloadJson += ",";
  }
  payloadJson += "]}";

  Serial.print("Payload JSON: ");
  Serial.println(payloadJson);
  client.publish(mqttTopic, payloadJson.c_str(), true);
}

// Callback untuk MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(message);

  if (String(topic) == mqttControlTopic && message.length() == 5) {
    for (int i = 0; i < 5; i++) {
      if (message[i] == '1') {
        LED_STATE[i] = 1;
        digitalWrite(ledPins[i], HIGH);
      } else if (message[i] == '0') {
        LED_STATE[i] = 0;
        digitalWrite(ledPins[i], LOW);
      } // Angka lain tidak memengaruhi status perangkat
    }
    Serial.print("Updated LED states: ");
    for (int i = 0; i < 5; i++) Serial.print(LED_STATE[i]);
    Serial.println();
    publishData();
  } else {
    Serial.println("Invalid LED control message.");
  }
}

// Fungsi reconnect MQTT
void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT!");
      client.subscribe(mqttControlTopic);
    } else {
      Serial.print("Failed, state=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds...");
      delay(5000);
    }
  }
}

// Setup
void setup() {
  Serial.begin(115200);
  connectToWiFi();
  espClient.setInsecure();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  pinMode(sensorPin, INPUT_PULLUP);
  for (int i = 0; i < 5; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW); // Pastikan semua LED mati di awal
  }
  dht.begin();
}

// Loop utama
void loop() {
  if (!client.connected()) connectToMQTT();
  client.loop();

  bool doorStatus = digitalRead(sensorPin) == LOW;
  if (doorStatus != lastDoorStatus) {
    lastDoorStatus = doorStatus;
    publishData();
  }

  static unsigned long lastPublish = 0;
  if (millis() - lastPublish > 10000) {
    lastPublish = millis();
    publishData();
  }
}