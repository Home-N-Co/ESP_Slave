/**
* @file main.cpp
 * @brief Gestion de feux tricolores connectés avec ESP32 et Adafruit IO (WiFi, MQTT, HTTP).
 *
 * @details
 * - Permet la connexion WiFi avec configuration web
 * - Utilise MQTT pour recevoir l'état du trafic
 * - Pilote les changements des feux selon le trafic
 * - Utilise la mémoire non-volatile pour stocker les identifiants
 */

#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "Preferences.h"
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "ArduinoJson.h"

// -----------------------------------------------------------------------------
// Variables globales
// -----------------------------------------------------------------------------

/** @brief Objet pour stocker et récupérer les préférences (WiFi, Adafruit IO) */
Preferences preferences;

/** @brief Formulaire HTML pour configurer WiFi et Adafruit IO */
auto htmlForm = R"rawliteral(
<!DOCTYPE HTML><html>
  <head>
    <title>ESP32 WiFi Setup</title>
  </head>
  <body>
    <h1>Enter WiFi and Adafruit IO Credentials</h1>
    <form action="/save" method="POST">
      <label for="ssid">WiFi SSID:</label><br>
      <input type="text" id="ssid" name="ssid"><br>

      <label for="password">WiFi Password:</label><br>
      <input type="password" id="password" name="password"><br><br>

      <label for="aio_username">Adafruit IO Username:</label><br>
      <input type="text" id="aio_username" name="aio_username"><br>

      <label for="aio_key">Adafruit IO Key:</label><br>
      <input type="text" id="aio_key" name="aio_key"><br><br>

      <label for="aio_direction">Direction :</label><br>
      <input type="text" id="aio_direction" name="aio_direction"><br><br>

      <input type="submit" value="Save Credentials">
    </form>
  </body>
</html>
)rawliteral";

/** @brief SSID du point d'accès créé par l'ESP32 */
auto ap_ssid = "ESP32-Access-Point";
/** @brief Mot de passe du point d'accès */
auto ap_password = "123456789";

// Credentials
String wifi_ssid;       /**< SSID WiFi enregistré */
String wifi_password;   /**< Mot de passe WiFi enregistré */
String aio_username;    /**< Nom d'utilisateur Adafruit IO */
String aio_direction;   /**< Direction du trafic */
String aio_key;         /**< Clé d'accès Adafruit IO */

/** @brief Adresse du serveur MQTT (Adafruit IO) */
auto mqtt_server = "io.adafruit.com";

/** @brief Serveur Web Asynchrone sur le port 80 */
AsyncWebServer server(80);

//! Instances WiFi et MQTT côté client.
WiFiClient espClient;
PubSubClient client(espClient);


//! Variables pour stocker les valeurs des feeds MQTT
String aio_directionTraffic, aio_directionPriority, aio_directionPedestrian, aio_directionTimer;

//! Topics des feeds MQTT
String feed1, feed2, feed3, feed4;

/** @brief Variable pour suivre le nombre de véhicules détectés. */
int vehicleCount = 7;
/** @brief Variable pour suivre le nombre de piétons détectés */
int pedestrianCount = 0;

// -----------------------------------------------------------------------------
// Fonctions principales
// -----------------------------------------------------------------------------

/**
 * @brief Connecte l'ESP32 au WiFi enregistré.
 *
 * @details Redémarre l'ESP32 après échec de connexion au bout de 10 secondes.
 */
void Wifi_Setup()
{
  Serial.println("Connecting to WiFi...");
  Serial.println("SSID:" + wifi_ssid);
  Serial.println("Password: " + wifi_password);
  WiFi.begin(wifi_ssid, wifi_password);

  const unsigned long startAttemptTimeWifi = millis();
  while (WiFiClass::status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (millis() - startAttemptTimeWifi > 10000) {
      Serial.println("\nFailed to connect to WiFi, clearing Preferences");
      preferences.begin("wifiCreds", false);
      preferences.clear();
      preferences.end();
      delay(1000);
      ESP.restart();
    }
  }

  Serial.println("\nConnected to WiFi");
  Serial.println("IP Address: " + WiFi.localIP().toString());
}

/**
 * @brief Envoie une valeur vers un feed Adafruit IO via HTTP POST.
 *
 * @param feed Nom du feed (ex: "north.timer")
 * @param value Valeur à envoyer
 */
void post_Setup(const String& feed, const String& value) {
  const String link = "https://io.adafruit.com/api/v2/Thorgan/feeds/" + feed + "/data";
  HTTPClient http;
  http.begin(link);
  http.addHeader("X-AIO-KEY", aio_key);
  http.addHeader("Content-Type", "application/json");

  const String json = R"({"value": ")" + value + "\"}";

  const int httpCode = http.POST(json);

  if (httpCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("[HTTP] POST... success");
    } else
    {
      Serial.printf("[HTTP] POST... failed, error: %s\n", HTTPClient::errorToString(httpCode).c_str());
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", HTTPClient::errorToString(httpCode).c_str());
  }

  http.end();
}

/**
 * @brief Récupère la dernière valeur d'un feed Adafruit IO via HTTP GET.
 *
 * @param feed Nom du feed (ex: "north.timer")
 * @return Dernière valeur enregistrée
 */
String get_Setup(const String& feed)
{
  HTTPClient http;

  const String link = "https://io.adafruit.com/api/v2/" + feed + "/data/last";

  http.begin(link);
  http.addHeader("X-AIO-KEY", aio_key);
  const int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();  // Get the response body

    // Create a JsonDocument and parse the response
    JsonDocument doc;  // No StaticJsonDocument needed
    const DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.println("Failed to parse JSON");
      http.end();
      return "";
    }

    // Extract the value from the JSON document
    const String value = doc["value"].as<String>();
    Serial.printf("%s Value: ", feed.c_str());
    Serial.println(value);

    http.end();  // End HTTP request
    return value;
  }

  Serial.println("Error on HTTP request");
  http.end();  // End HTTP request
  return "";
}

/**
 * @brief Démarre un point d'accès WiFi pour permettre la saisie d'identifiants WiFi et Adafruit IO.
 */
void start_access_point()
{
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlForm);
  });

  server.on("/save", HTTP_POST, []( AsyncWebServerRequest *request){
    Serial.println("Request received:");

    if (request->hasParam("ssid", true) && request->hasParam("password", true) && request->hasParam("aio_direction", true) && request->hasParam("aio_username", true) && request->hasParam("aio_key", true)) {
      wifi_ssid = request->getParam("ssid", true)->value();
      wifi_password = request->getParam("password", true)->value();
      aio_direction = request->getParam("aio_direction", true)->value();

      Serial.println("Saving credentials...");
      preferences.begin("wifiCreds", false);
      preferences.putString("wifi_ssid", wifi_ssid);
      preferences.putString("wifi_password", wifi_password);
      preferences.putString("aio_username", aio_username);
      preferences.putString("aio_key", aio_key);
      preferences.putString("aio_direction", aio_direction);
      preferences.end();
      Serial.println("Credentials saved successfully!");
      request->send(200, "text/plain", "Credentials saved successfully!");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("Error: Missing parameters.");
      request->send(400, "text/plain", "Error: Missing parameters");
    }
  });

  server.begin();
}

/**
 * @brief Vérifie si des identifiants WiFi et Adafruit IO existent dans la mémoire.
 *
 * @return true si les identifiants sont valides, false sinon.
 */
bool credentialsExist() {
  preferences.begin("wifiCreds", false);
  const bool valid = preferences.getString("wifi_ssid", "") != "" &&
               preferences.getString("wifi_password", "") != "" &&
               preferences.getString("aio_username", "") != "" &&
                 preferences.getString("aio_direction", "") != "" &&
               preferences.getString("aio_key", "") != "";
  preferences.end();
  return valid;
}

/**
 * @brief Fonction appelée automatiquement à la réception d'un message MQTT.
 *
 * @param topic Sujet du message
 * @param payload Données envoyées
 * @param length Taille des données
 */
void callback(char* topic, const byte* payload, const unsigned int length) {
  const auto incomingTopic = String(topic);
  const String value(reinterpret_cast<const char*>(payload), length);

  Serial.println("Topic: " + incomingTopic);
  Serial.println("Value: " + value);

  if (incomingTopic == aio_username + "/feeds/" + aio_direction + ".timer")
  {
    Serial.println("Timer: " + value);
    aio_directionTimer = value;
  }
}

/**
 * @brief Détecte un véhicule et met à jour les données de trafic pertinentes.
 *
 * Détecte un véhicule, incrémente le compteur de véhicules et envoie les données mises à jour à Adafruit IO.
 * Si le véhicule est marqué comme prioritaire, des actions spécifiques sont également effectuées.
 *
 * @param isPriority Définit si le véhicule détecté est un véhicule prioritaire. Par défaut à false.
 */
void detectVehicle(bool isPriority = false) {
  vehicleCount++;
  aio_directionTraffic = String(vehicleCount);

  if (isPriority) {
    aio_directionPriority = "1";
    post_Setup(feed2, "1");
    Serial.println("Véhicule PRIORITAIRE détecté!");
  }

  Serial.print("Véhicule détecté! Total: ");
  Serial.println(vehicleCount);

  post_Setup(feed1, aio_directionTraffic);
}

/**
 * @brief Détecte un piéton et met à jour les données associées.
 *
 * Cette fonction incrémente le compteur de piétons, met à jour la variable.
 */
void detectPedestrian() {
  pedestrianCount++;
  Serial.println("Pedestrian detected!");
  aio_directionPedestrian = String(pedestrianCount);
}

/**
 * @brief Gère le système de feux de circulation en fonction des données des différents flux.
 *
 * Cette fonction traite les signaux actuels de trafic, de piétons et de priorité
 * reçus sous forme de chaînes de caractères depuis les flux Adafruit IO. En fonction de ces signaux :
 * - Si une traversée piétonne est demandée, elle déclenche une mise à jour du flux correspondant.
 * - Si une condition de priorité (par ex., véhicule d'urgence) est détectée, elle notifie le flux de priorité.
 * - Si la valeur du trafic dépasse un seuil (par ex., >1), elle signale le nombre de véhicules au flux de trafic.
 *
 * Elle utilise la fonction `post_Setup` pour envoyer des mises à jour en effectuant des requêtes HTTP POST
 * vers les flux Adafruit IO désignés.
 */
void controlTrafficLight() {
  Serial.printf("Traffic : %d\n", aio_directionTraffic.toInt());

  if (aio_directionPedestrian.toInt() == 1)
    post_Setup(feed2, "1");

  if (aio_directionPriority.toInt() == 1)
    post_Setup(feed3, "1");

  if (aio_directionTraffic.toInt() > 1)
    post_Setup(feed1, String(vehicleCount));
}

/**
 * @brief Fonction d'initialisation appelée au démarrage de l'ESP32.
 */
void setup() {
  Serial.begin(115200);

  if (credentialsExist()) {
    Serial.println("Credentials found. Connecting to WiFi and Adafruit");

    preferences.begin("wifiCreds", false);
    wifi_ssid = preferences.getString("wifi_ssid");
    wifi_password = preferences.getString("wifi_password");
    aio_username = preferences.getString("aio_username");
    aio_direction = preferences.getString("aio_direction");
    aio_key = preferences.getString("aio_key");
    preferences.end();

    // Rebuild feeds with correct aio_username
    feed1 = aio_username +  "/feeds/" + aio_direction + ".traffic";
    feed2 = aio_username + "/feeds/" + aio_direction + ".priority";
    feed3 = aio_username + "/feeds/" + aio_direction + ".pedestrian";
    feed4 = aio_username + "/feeds/" + aio_direction + ".timer";

    Wifi_Setup();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    Serial.println("Connected to Adafruit");
    Serial.println("Waiting for API to clear up to not overflow free limit");
    delay(30000);
  } else {
    Serial.println("No WiFi credentials found. Starting Access Point.");
    start_access_point();
  }

  // Wait for MQTT connection
  while (!client.connected()) {
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), aio_username.c_str(), aio_key.c_str())) {
      Serial.println("Connected to MQTT Broker!");
      client.subscribe(feed1.c_str());
      client.subscribe(feed2.c_str());
      client.subscribe(feed3.c_str());
      client.subscribe(feed4.c_str());
    } else {
      Serial.print("Failed MQTT connection, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

unsigned long lastUpdate = 0;
constexpr unsigned long updateInterval = 6000; // 6 seconds

/**
 * @brief Boucle principale qui gère la logique de communication MQTT et la mise à jour des timers.
 */
void loop() {
  client.loop();

  // Reconnect if needed
  if (!client.connected()) {
    while (!client.connected()) {
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);

      if (client.connect(clientId.c_str(), aio_username.c_str(), aio_key.c_str())) {
        Serial.println("Reconnected to MQTT Broker!");
        client.subscribe(feed1.c_str());
        client.subscribe(feed2.c_str());
        client.subscribe(feed3.c_str());
        client.subscribe(feed4.c_str());
      } else {
        Serial.print("Failed to reconnect, rc=");
        Serial.print(client.state());
        delay(5000);
      }
    }
  }

  if (aio_directionTimer == "5")
  {
    Serial.println("Update Traffic Light");
    controlTrafficLight();
  }
}