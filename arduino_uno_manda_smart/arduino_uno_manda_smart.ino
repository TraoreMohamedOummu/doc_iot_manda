#include <DHT.h>
#include <ArduinoJson.h>

// Déclaration des pins pour les capteurs, relais et LED
#define DHTPIN_FROID 2
#define DHTPIN_CHAUD 3

#define DHTTYPE DHT11

#define RELAY_PIN_FROID 7
#define RELAY_PIN_CHAUD 8

#define LED_R_FROID 12
#define LED_V_FROID 13

#define LED_V_CHAUD 10
#define LED_R_CHAUD 11

// Commandes pour les relais froid et chaud
const String ON_RELAY_FROID = "ON_FROID";
const String OFF_RELAY_FROID = "OFF_FROID";
const String ON_RELAY_CHAUD = "ON_CHAUD";
const String OFF_RELAY_CHAUD = "OFF_CHAUD";

// Variables pour le compartiment froid
String command_froid = "";
int temperatureMin_froid = 0;
int temperatureMax_froid = 0;
bool thresholdsUpdated_froid = false;

// Variables pour le compartiment chaud
String command_chaud = "";
int temperatureMin_chaud = 0;
int temperatureMax_chaud = 0;
bool thresholdsUpdated_chaud = false;
bool relayChaudState = false;

DHT dht_froid(DHTPIN_FROID, DHTTYPE);
DHT dht_chaud(DHTPIN_CHAUD, DHTTYPE);

void setup() {
  Serial.begin(115200);

  dht_froid.begin();
  dht_chaud.begin();

  // Initialisation des pins pour les relais et les LED
  pinMode(RELAY_PIN_FROID, OUTPUT);
  digitalWrite(RELAY_PIN_FROID, LOW);

  pinMode(RELAY_PIN_CHAUD, OUTPUT);
  digitalWrite(RELAY_PIN_CHAUD, LOW);

  pinMode(LED_R_FROID, OUTPUT);
  pinMode(LED_V_FROID, OUTPUT);
  

  digitalWrite(LED_R_FROID, HIGH);
  digitalWrite(LED_V_FROID, LOW);

  pinMode(LED_R_CHAUD, OUTPUT);
  pinMode(LED_V_CHAUD, OUTPUT);

  digitalWrite(LED_R_CHAUD, HIGH);
  digitalWrite(LED_V_CHAUD, LOW);

  // Affichage de l'état initial des relais
  //printRelayState();
}

void loop() {
  // Lire les données des capteurs
  float currentTempFroid = dht_froid.readTemperature();
  float currentTempChaud = dht_chaud.readTemperature();

  // Vérifier les erreurs de lecture
  if (isnan(currentTempFroid)  || isnan(currentTempChaud)) {
    Serial.println("Erreur de lecture des capteurs ");
    return;
  }

  // Envoyer les données capteurs sous format JSON
  sendSensorData(currentTempFroid, currentTempChaud);

  if(digitalRead(RELAY_PIN_CHAUD)) {
    if(currentTempChaud >= 50) {
      command_chaud = "OFF_CHAUD";
      digitalWrite(RELAY_PIN_CHAUD, LOW);
      digitalWrite(LED_V_CHAUD, LOW);
      digitalWrite(LED_R_CHAUD, HIGH);
      command_chaud = "ON_CHAUD";
    }
  }else if (command_chaud == "ON_CHAUD") {
   if(currentTempChaud < 40 ) {
      digitalWrite(RELAY_PIN_CHAUD, HIGH);
      digitalWrite(LED_V_CHAUD, HIGH);
      digitalWrite(LED_R_CHAUD, LOW);
    }
  }

  if(digitalRead(RELAY_PIN_FROID)) {
    command_froid = "ON_FROID";
    if(command_froid == "ON_FROID") {
      digitalWrite(RELAY_PIN_FROID, HIGH);
      digitalWrite(LED_V_FROID, HIGH);
      digitalWrite(LED_R_FROID, LOW);
    }
    
  }

  // Lire les commandes envoyées
  if (Serial.available()) {
    String incomingData = Serial.readStringUntil('\n');
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, incomingData);

    if (error) {
      Serial.print("Erreur de désérialisation JSON : ");
      Serial.println(error.f_str());
      return;
    }

    // Extraire les commandes et les températures minimales/maximales

    // Gestion de commandes pour le systeme de refroidissement
    if (doc.containsKey("command_froid")) {

      command_froid = doc["command_froid"].as<String>();
      command_froid.trim();
      if(command_froid == "ON_FROID") {
        digitalWrite(RELAY_PIN_FROID, HIGH);
        if(digitalRead(RELAY_PIN_FROID)) {
          digitalWrite(LED_V_FROID, HIGH);
          digitalWrite(LED_R_FROID, LOW);
        }else {
          digitalWrite(LED_R_FROID, HIGH);
          digitalWrite(LED_V_FROID, LOW);
        }
      }else if (command_froid == "OFF_FROID") {
        digitalWrite(RELAY_PIN_FROID, LOW);
        if(!digitalRead(RELAY_PIN_FROID)) {
          digitalWrite(LED_R_FROID, HIGH);
          digitalWrite(LED_V_FROID, LOW);
        }else {
          digitalWrite(LED_V_FROID, HIGH);
          digitalWrite(LED_R_FROID, LOW);
        }
      }

      int newTempMinFroid = doc["temperatureMin_froid"];
      int newTempMaxFroid = doc["temperatureMax_froid"];
      if ((temperatureMin_froid != newTempMinFroid) || (temperatureMax_froid != newTempMaxFroid)) {
        temperatureMin_froid = newTempMinFroid;
        temperatureMax_froid = newTempMaxFroid;
        thresholdsUpdated_froid = true;
      }
    }

    
    // Gestion de commandes pour le systeme de sechage
    if (doc.containsKey("command_chaud")) {
      command_chaud = doc["command_chaud"].as<String>();
      command_chaud.trim();
      if(command_chaud == "ON_CHAUD") {
        digitalWrite(RELAY_PIN_CHAUD, HIGH);
        if(digitalRead(RELAY_PIN_CHAUD)) {

          digitalWrite(LED_V_CHAUD, HIGH);
          digitalWrite(LED_R_CHAUD, LOW);
        }else {
          digitalWrite(LED_R_CHAUD, HIGH);
          digitalWrite(LED_V_CHAUD, LOW);
        }
      }else if (command_chaud == "OFF_CHAUD") {
        digitalWrite(RELAY_PIN_CHAUD, LOW);
        if(!digitalRead(RELAY_PIN_CHAUD)) {
          digitalWrite(LED_R_CHAUD, HIGH);
          digitalWrite(LED_V_CHAUD, LOW);
        }else {
          digitalWrite(LED_V_CHAUD, HIGH);
          digitalWrite(LED_R_CHAUD, LOW);
        }
      }

      int newTempMinChaud = doc["temperatureMin_chaud"];
      int newTempMaxChaud = doc["temperatureMax_chaud"];
      if ((temperatureMin_chaud != newTempMinChaud) || (temperatureMax_chaud != newTempMaxChaud)) {
        temperatureMin_chaud = newTempMinChaud;
        temperatureMax_chaud = newTempMaxChaud;
        thresholdsUpdated_chaud = true;
      }
    }
  }



  // Ajuster le relais en fonction des températures et des seuils
  if (thresholdsUpdated_froid) {
    adjustTemperature(RELAY_PIN_FROID, temperatureMin_froid, temperatureMax_froid, currentTempFroid);
    thresholdsUpdated_froid = false;
  }

  // Ajuster le relais en fonction des températures et des seuils
  if (thresholdsUpdated_chaud) {
    adjustTemperature(RELAY_PIN_CHAUD, temperatureMin_chaud, temperatureMax_chaud, currentTempChaud);
    thresholdsUpdated_chaud = false;
  }

  delay(3000);

}

// Envoyer les données du capteur sous format JSON
void sendSensorData(float tempFroid, float tempChaud) {
  String jsonData = "{\"temperature_froid\": " + String(tempFroid) +  ", \"temperature_chaud\": " + String(tempChaud) + "}";
  Serial.println(jsonData);
}

// Fonction pour ajuster le relais en fonction de la température
void adjustTemperature(int relayPin, int minTemp, int maxTemp, float currentTemp) {
  int currentTempInt = (int)currentTemp;
  if (currentTempInt < minTemp) {
    digitalWrite(relayPin, HIGH);  // Activer le relais
  } else if (currentTempInt > maxTemp) {
    digitalWrite(relayPin, LOW);   // Désactiver le relais
  }
}


