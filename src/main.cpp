#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "Preferences.h"
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "ArduinoJson.h"

Preferences preferences;

// HTML form for entering credentials
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
      <label for="aio_direction">Direction :</label><br>
      <input type="text" id="aio_direction" name="aio_direction"><br><br>
      <input type="text" id="aio_username" name="aio_username"><br>
      <label for="aio_key">Adafruit IO Key:</label><br>
      <input type="text" id="aio_key" name="aio_key"><br><br>
      <input type="submit" value="Save Credentials">
    </form>
  </body>
</html>
)rawliteral";

// Define AP credentials
auto ap_ssid = "ESP32-Access-Point";
auto ap_password = "123456789";

// WiFi credentials variables
String wifi_ssid;
String wifi_password;
String aio_username;
String aio_key;
auto mqtt_server = "io.adafruit.com";

// Create an instance of the web server
AsyncWebServer server(80);

// Saving state
String direction = "";  // Initialize Direction
String lastDirection = "";
int countRepeatedDirection = 0;

// Setup MQTT
WiFiClient espClient;
PubSubClient client(espClient);


// mqtt feed
String n_traffic;
String n_priority;
String n_pedestrian;
String n_timer;

String w_traffic;
String w_priority;
String w_pedestrian;
String w_timer;

String feed1;
String feed2;
String feed3;
String feed4;
String feed5;
String feed6;
String feed7;
String feed8;

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

    if (request->hasParam("ssid", true) && request->hasParam("password", true) && request->hasParam("aio_username", true) && request->hasParam("aio_key", true)) {
      wifi_ssid = request->getParam("ssid", true)->value();
      wifi_password = request->getParam("password", true)->value();
      aio_username = request->getParam("aio_username", true)->value();
      aio_key = request->getParam("aio_key", true)->value();

      Serial.println("Saving credentials...");
      preferences.begin("wifiCreds", false);
      preferences.putString("wifi_ssid", wifi_ssid);
      preferences.putString("wifi_password", wifi_password);
      preferences.putString("aio_username", aio_username);
      preferences.putString("aio_key", aio_key);
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

bool credentialsExist() {
  preferences.begin("wifiCreds", false);
  const bool valid = preferences.getString("wifi_ssid", "") != "" &&
               preferences.getString("wifi_password", "") != "" &&
               preferences.getString("aio_username", "") != "" &&
               preferences.getString("aio_key", "") != "";
  preferences.end();
  return valid;
}

void callback(char* topic, const byte* payload, const unsigned int length) {
  const auto incomingTopic = String(topic);
  const String value(reinterpret_cast<const char*>(payload), length);

  Serial.println("Topic: " + incomingTopic);
  Serial.println("Value: " + value);

  if (incomingTopic == aio_username + "/feeds/north.traffic")
  {
    n_traffic = value;
  }
  if (incomingTopic == aio_username + "/feeds/west.traffic")
  {
    w_traffic = value;
  }
  if (incomingTopic == aio_username + "/feeds/north.priority")
  {
    n_priority = value;
  }
  if (incomingTopic == aio_username + "/feeds/west.priority")
  {
    w_priority = value;
  }
  if (incomingTopic == aio_username + "/feeds/north.pedestrian")
  {
    n_pedestrian = value;
  }
  if (incomingTopic == aio_username + "/feeds/west.pedestrian")
  {
    w_pedestrian = value;
  }
  if (incomingTopic == aio_username + "/feeds/north.timer")
  {
    Serial.println("Timer North: " + value);
    n_timer = value;
  }
  if (incomingTopic == aio_username + "/feeds/west.timer")
  {
    Serial.println("Timer West: " + value);
    w_timer = value;
  }
}

void controlTrafficLight() {
  Serial.printf("Traffic North: %d\n", n_traffic.toInt());
  Serial.printf("Traffic West: %d\n", w_traffic.toInt());
  Serial.printf("Timer North: %d\n", n_timer.toInt());
  Serial.printf("Timer West: %d\n", w_timer.toInt());
  Serial.printf("Repeated Count: %d\n",countRepeatedDirection);

  const int n_traffic_count = n_traffic.toInt();
  const int w_traffic_count = w_traffic.toInt();
  const int n_timer_count = n_timer.toInt();
  const int w_timer_count = w_timer.toInt();

  if (n_traffic_count == 0 && w_traffic_count == 0)
  {
    direction = "West";
    post_Setup("west.timer", "0");
    post_Setup("north.timer", "25");
  }

  if (lastDirection == "West" && w_timer_count == 5 && (n_traffic_count > w_traffic_count || countRepeatedDirection == 1)) {
    Serial.println("West To North");
    direction = "North";
    post_Setup("west.timer", "0");
    post_Setup("north.timer", "25");
    countRepeatedDirection = 2;

  } else if (lastDirection == "North" && n_timer_count == 5 && (w_traffic_count > n_traffic_count || countRepeatedDirection == 1))
  {
    Serial.println("North To West");
    direction = "West";
    post_Setup("north.timer", "0");
    post_Setup("west.timer", "25");
    countRepeatedDirection = 2;

  } else if (lastDirection == "West" && w_timer_count == 5 && (n_traffic_count < w_traffic_count || countRepeatedDirection < 1)) {
    Serial.println("West Repeat");
    direction = "West";
    post_Setup("north.timer", "0");
    post_Setup("west.timer", "25");
    countRepeatedDirection += 1;

  } else if (lastDirection == "North" && n_timer_count == 5 && (w_traffic_count < n_traffic_count || countRepeatedDirection < 1)) {
    Serial.println("North Repeat");
    direction = "North";
    post_Setup("west.timer", "0");
    post_Setup("north.timer", "25");
    countRepeatedDirection += 1;
  } else
  {
    Serial.println("No change");
    Serial.println("Last direction: " + lastDirection);
    Serial.println("Current direction: " + direction);
    Serial.println("Traffic North: " + n_traffic);
    Serial.println("Traffic West: " + w_traffic);
    Serial.println("Timer North: " + n_timer);
    Serial.println("Timer West: " + w_timer);
    Serial.println("CountRepeatedDirection: " + String(countRepeatedDirection));
  }

  lastDirection = direction; // Update last direction
  if (countRepeatedDirection >= 2)
  {
    countRepeatedDirection = 0; // Reset count repeated direction
  }
}


void setup() {
  Serial.begin(115200);

  if (credentialsExist()) {
    Serial.println("Credentials found. Connecting to WiFi and Adafruit");

    preferences.begin("wifiCreds", false);
    wifi_ssid = preferences.getString("wifi_ssid");
    wifi_password = preferences.getString("wifi_password");
    aio_username = preferences.getString("aio_username");
    aio_key = preferences.getString("aio_key");
    preferences.end();

    // Rebuild feeds with correct aio_username
    feed1 = aio_username + "/feeds/north.traffic";
    feed2 = aio_username + "/feeds/west.traffic";

    feed3 = aio_username + "/feeds/north.priority";
    feed4 = aio_username + "/feeds/west.priority";

    feed5 = aio_username + "/feeds/north.pedestrian";
    feed6 = aio_username + "/feeds/west.pedestrian";

    feed7 = aio_username + "/feeds/north.timer";
    feed8 = aio_username + "/feeds/west.timer";

    Wifi_Setup();
    client.setServer(mqtt_server, 1883);
    post_Setup("north.timer", "25");
    post_Setup("west.timer", "0");
    direction = "North";
    n_traffic = get_Setup(feed1);
    w_traffic = get_Setup(feed2);
    n_priority = get_Setup(feed3);
    w_priority = get_Setup(feed4);
    n_pedestrian = get_Setup(feed5);
    w_pedestrian = get_Setup(feed6);
    n_timer = get_Setup(feed7);
    w_timer = get_Setup(feed8);
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
      client.subscribe(feed5.c_str());
      client.subscribe(feed6.c_str());
      client.subscribe(feed7.c_str());
      client.subscribe(feed8.c_str());
    } else {
      Serial.print("Failed MQTT connection, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

unsigned long lastUpdate = 0;
constexpr unsigned long updateInterval = 6000; // 6 seconds

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
        client.subscribe(feed5.c_str());
        client.subscribe(feed6.c_str());
        client.subscribe(feed7.c_str());
        client.subscribe(feed8.c_str());
      } else {
        Serial.print("Failed to reconnect, rc=");
        Serial.print(client.state());
        delay(5000);
      }
    }
  }

  if (n_timer == "5" || w_timer  == "5")
  {
    Serial.println("Update Traffic Light");
    controlTrafficLight();
  }
}