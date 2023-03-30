#include <Arduino.h>
#include <Ticker.h>
#include <Wire.h>
#include <WiFi.h>
#include "BH1750.h"
#include "DHTesp.h"
#include "device.h"
#include <PubSubClient.h>

#define WIFI_SSID "POCO X3 Pro"
#define WIFI_PASSWORD "ketokpintu"

#define MQTT_BROKER  "broker.emqx.io"
#define MQTT_TOPIC_PUBLISH   "michaelac_esp32/data"
#define MQTT_TOPIC_SUBSCRIBE "michaelac_esp32/data" 

Ticker timerLux;
Ticker timerTemp;
Ticker timerHum;
DHTesp dht;

// Global Parameters
float globalLux, globalTemp, globalHum;

// MQTT Configuration
Ticker timerPublish;
char g_szDeviceId[30];
Ticker timerMqtt;
WiFiClient espClient;
PubSubClient mqtt(espClient);
boolean mqttConnect();
void WifiConnect();

// Ticker MQTT
Ticker timerTempMQTT;
Ticker timerHumMQTT;
Ticker timerLuxMQTT;

void publish5Sec() {
  char tempInfo[50];
  sprintf(tempInfo, "Temp: %.2f C\n", globalTemp);
  mqtt.publish(MQTT_TOPIC_PUBLISH, tempInfo);
}

void publish4Sec() {
  char humInfo[50];
  sprintf(humInfo, "Hum: %.2f %\n", globalHum);
  mqtt.publish(MQTT_TOPIC_PUBLISH, humInfo);
}

void publish6Sec()
{
  char luxInfo[50];
  sprintf(luxInfo, "Lux: %.2f lux", globalLux);
  mqtt.publish(MQTT_TOPIC_PUBLISH, luxInfo);
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();
}

boolean mqttConnect() {
  sprintf(g_szDeviceId, "esp32_%08X",(uint32_t)ESP.getEfuseMac());
  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(mqttCallback);
  Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId);

  boolean fMqttConnected = false;
  for (int i=0; i<3 && !fMqttConnected; i++) {
    Serial.print("Connecting to mqtt broker...");
    fMqttConnected = mqtt.connect(g_szDeviceId);
    if (fMqttConnected == false) {
      Serial.print(" fail, rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }

  if (fMqttConnected)
  {
    Serial.println(" success");
    mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);
    Serial.printf("Subcribe topic: %s\n", MQTT_TOPIC_SUBSCRIBE);
    publish4Sec();
    publish5Sec();
    publish6Sec();
  }
  return mqtt.connected();
}

void WifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }  
  Serial.print("System connected with IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());
}

// Local Data
void HTBound(float humidity, float temperature) {
  if(humidity > 80 && temperature > 28) {
    digitalWrite(LED_RED, 1);
  }
  else if((humidity > 60 && humidity < 80) && temperature > 28) {
    digitalWrite(LED_YELLOW, 1);
  }
  else if(humidity < 60 && temperature < 28) {
    digitalWrite(LED_GREEN, 1);
  }
}

void onReadHum() {
  float humidity = dht.getHumidity();
  if(dht.getStatus() == DHTesp::ERROR_NONE) {
    Serial.printf("Humidity: %.2f%%\n", humidity);
  }
  globalHum = humidity;
}

void onReadTemp() {
  float temperature = dht.getTemperature();
  if(dht.getStatus() == DHTesp::ERROR_NONE) {
    Serial.printf("Temperature: %.2f C\n", temperature);
  }
  globalTemp = temperature;
}

void lightBound(float lux) {
  bool ledOn = false;
  if(lux > 400) {
    Serial.printf("Warning: Pintu Dibuka. %.2f lux\n", lux);
  }
  else {
    Serial.printf("Pintu Tertutup\n");
  }
}

BH1750 lightMeter;
void onReadLight() {
  float lux = lightMeter.readLightLevel();
  Serial.printf("Light: %.2f lux\n", lux);
  lightBound(lux);
  globalLux = lux;
}


void onReadSensors() {
  onReadHum();
  onReadTemp();
  onReadLight();
}

void setup() {
  Serial.begin(9600);

  // Temp & Humidity
  dht.setup(PIN_DHT, DHTesp::DHT11);

  // Luxmeter
  Wire.begin(PIN_SDA, PIN_SCL);
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);

  Serial.printf("Board: %s\n", ARDUINO_BOARD);
  Serial.printf("DHT Sensor ready, sampling period: %d ms\n", dht.getMinimumSamplingPeriod());
  #if defined(ESP32)
    timerLux.attach_ms(4000, onReadLight);
    timerTemp.attach_ms(5000, onReadTemp);
    timerHum.attach_ms(6000, onReadHum);
  #endif

  // MQTT
  pinMode(LED_BUILTIN, OUTPUT);
  WifiConnect();
  mqttConnect();
  timerLuxMQTT.attach_ms(4000, publish4Sec);
  timerTempMQTT.attach_ms(5000, publish5Sec);
  timerHumMQTT.attach_ms(6000, publish6Sec);
}

void loop() {
  // put your main code here, to run repeatedly:
  mqtt.loop();
}