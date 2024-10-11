#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "env.h"  // Contains your certificates and keys

// WiFi credentials
const char WIFI_SSID[] = "sia";
const char WIFI_PASSWORD[] = "12345678";

// AWS IoT Core parameters
const char THINGNAME[] = "ESP8266";
const char MQTT_HOST[] = "a1qavnvw56vtpy-ats.iot.eu-north-1.amazonaws.com";

// MQTT topics
const char NOTIFIC_TOPIC_FROID_SUB[] = "manda_smart/notifications_froid";
const char NOTIFIC_TOPIC_CHAUD_SUB[] = "manda_smart/notifications_chaud";

const char TOPIC_DATA_TEMPERATURES_PUB[] = "manda_smart/envoi_data";

const char COMMAND_TOPIC_FROID_SUB[] = "manda_smart/command_froid";
const char COMMAND_TOPIC_CHAUD_SUB[] = "manda_smart/command_chaud";

const char TOPIC_CONFIRM_RESPONSE[] = "manda_smart/confirm/response";



// Timezone offset from UTC
const int8_t TIME_ZONE = 0;  // Adjust as needed

// Global variables to store data for both cold and hot compartments
String command_froid, command_chaud;
int temperatureMin_froid = 0, temperatureMax_froid = 0;
int temperatureMin_chaud = 0, temperatureMax_chaud = 0;
bool hasTemperatureData_froid = false, hasTemperatureData_chaud = false;

String RESPONSE_CHAUD_ON = "response_chaud_on";
String RESPONSE_CHAUD_OFF = "response_chaud_off";
String RESPONSE_FROID_ON = "response_froid_on";
String RESPONSE_FROID_OFF = "response_froid_off";

bool isMessage = false;

// WiFiClientSecure for secure connection
WiFiClientSecure net;
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);

// MQTT client
PubSubClient client(net);

// Function to synchronize time using NTP
void NTPConnect() {
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) { // Check if time is set
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
}

// MQTT message callback
void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to String
  String payloadStr;
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

  // Parse JSON message
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payloadStr);

  if (error) {
    Serial.print("JSON deserialization error: ");
    Serial.println(error.f_str());
    return;
  }

  // Clear previous data for both compartments
  command_froid = "";
  command_chaud = "";
  temperatureMin_froid = temperatureMax_froid = 0;
  temperatureMin_chaud = temperatureMax_chaud = 0;
  hasTemperatureData_froid = hasTemperatureData_chaud = false;

  // Store parsed data for cold compartment
  if (doc.containsKey("command_froid")) {
    command_froid = doc["command_froid"].as<String>();
  }
  if (doc.containsKey("temperatureMin_froid")) {
    temperatureMin_froid = doc["temperatureMin_froid"].as<int>();
    hasTemperatureData_froid = true;
  }
  if (doc.containsKey("temperatureMax_froid")) {
    temperatureMax_froid = doc["temperatureMax_froid"].as<int>();
    hasTemperatureData_froid = true;
  }

  // Store parsed data for hot compartment
  if (doc.containsKey("command_chaud")) {
    command_chaud = doc["command_chaud"].as<String>();
  }
  if (doc.containsKey("temperatureMin_chaud")) {
    temperatureMin_chaud = doc["temperatureMin_chaud"].as<int>();
    hasTemperatureData_chaud = true;
  }
  if (doc.containsKey("temperatureMax_chaud")) {
    temperatureMax_chaud = doc["temperatureMax_chaud"].as<int>();
    hasTemperatureData_chaud = true;
  }

  // Display the JSON for debugging
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  Serial.println(jsonOutput);

  // Confirm actions based on received commands
  if (command_chaud == "ON_CHAUD") {
    client.publish(TOPIC_CONFIRM_RESPONSE, RESPONSE_CHAUD_ON.c_str());
    Serial.println("Chauffage activé");
  } else if (command_chaud == "OFF_CHAUD") {
    client.publish(TOPIC_CONFIRM_RESPONSE, RESPONSE_CHAUD_OFF.c_str());
    Serial.println("Chauffage désactivé");
  }

  if (command_froid == "ON_FROID") {
    client.publish(TOPIC_CONFIRM_RESPONSE, RESPONSE_FROID_ON.c_str());
    Serial.println("Refroidissement activé");
  } else if (command_froid == "OFF_FROID") {
    client.publish(TOPIC_CONFIRM_RESPONSE, RESPONSE_FROID_OFF.c_str());
    Serial.println("Refroidissement désactivé");
  }
}

// Function to connect to AWS IoT Core
void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nWiFi connected");

  // Synchronize time with NTP
  NTPConnect();

  // Set up secure connection
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  // Set MQTT server and callback
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(callback);

  Serial.println("Connecting to AWS IoT Core...");

  // Connect to AWS IoT Core
  while (!client.connected()) {
    if (client.connect(THINGNAME)) {
      Serial.println("Connected to AWS IoT Core");
      // Subscribe to MQTT topics
      client.subscribe(COMMAND_TOPIC_FROID_SUB);
      client.subscribe(COMMAND_TOPIC_CHAUD_SUB);
    } else {
      Serial.print("Failed to connect, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 2 seconds");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  connectAWS();
}

void loop() {
  if (!client.connected()) {
    connectAWS();
  }
  client.loop();

  // Publish data for cold compartment if available
  if (hasTemperatureData_froid) {
    StaticJsonDocument<200> docFroid;
    docFroid["temperatureMin_froid"] = temperatureMin_froid;
    docFroid["temperatureMax_froid"] = temperatureMax_froid;
    docFroid["status_froid"] = "1";

    String jsonOutputFroid;
    serializeJson(docFroid, jsonOutputFroid);
    client.publish(NOTIFIC_TOPIC_FROID_SUB, jsonOutputFroid.c_str());

    hasTemperatureData_froid = false;
  }

  // Publish data for hot compartment if available
  if (hasTemperatureData_chaud) {
    StaticJsonDocument<200> docChaud;
    docChaud["temperatureMin_chaud"] = temperatureMin_chaud;
    docChaud["temperatureMax_chaud"] = temperatureMax_chaud;
    docChaud["status_chaud"] = "1";

    String jsonOutputChaud;
    serializeJson(docChaud, jsonOutputChaud);
    client.publish(NOTIFIC_TOPIC_CHAUD_SUB, jsonOutputChaud.c_str());
    client.publish(TOPIC_CONFIRM_RESPONSE, "temp_min_max_chaud");
    hasTemperatureData_chaud = false;
  }

  // Publish temperature data if available from Serial
  if (Serial.available()) {
    String jsonData = Serial.readStringUntil('\n');
    client.publish(TOPIC_DATA_TEMPERATURES_PUB, jsonData.c_str());
  }

  delay(1000);
}
