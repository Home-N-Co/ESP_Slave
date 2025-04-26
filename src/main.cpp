//
// Created by Kantin FAGNIART on 24/03/2025.
//
#include "WiFi.h"
#include "AdafruitIO_WiFi.h"
#include "ESPAsyncWebServer.h"
#include "Preferences.h"

Preferences preferences;

// HTML form for entering credentials
const char* htmlForm = R"rawliteral(
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
      <input type="submit" value="Save Credentials">
    </form>
  </body>
</html>
)rawliteral";

// Define AP credentials
const char *ap_ssid = "ESP32-Access-Point";
const char *ap_password = "123456789";

// WiFi credentials variables
String wifi_ssid;
String wifi_password;
String aio_username;
String aio_key;

// Create an instance of the web server
AsyncWebServer server(80);

AdafruitIO_WiFi io("", "", "", "");  // will be reinitialized in AdafruitIO_Setup()

AdafruitIO_Feed *north;
AdafruitIO_Feed *n_pedestrian;
AdafruitIO_Feed *n_priority;

AdafruitIO_Feed *south;
AdafruitIO_Feed *s_pedestrian;
AdafruitIO_Feed *s_priority;

AdafruitIO_Feed *east;
AdafruitIO_Feed *e_pedestrian;
AdafruitIO_Feed *e_priority;

AdafruitIO_Feed *west;
AdafruitIO_Feed *w_pedestrian;
AdafruitIO_Feed *w_priority;


void AdafruitIO_Setup()
{
  // If credentials are saved, connect to WiFi

  Serial.println("Connecting to WiFi...");
  Serial.println("SSID:" + wifi_ssid);
  Serial.println("Password: " + wifi_password);
  WiFi.begin(wifi_ssid, wifi_password);

  const unsigned long startAttemptTimeWifi = millis(); // Record when we started trying
  // Waiting for connection
  while (WiFiClass::status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (millis() - startAttemptTimeWifi > 10000) { // If we've waited 10 seconds, give up
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

  // Connect to Adafruit IO
  AdafruitIO_WiFi io(aio_username.c_str(), aio_key.c_str(), wifi_ssid.c_str(), wifi_password.c_str());

  // Start sync with all feeds
  AdafruitIO_Feed *north = io.feed("north_traffic");
  AdafruitIO_Feed *south = io.feed("south_traffic");
  AdafruitIO_Feed *east  = io.feed("east_traffic");
  AdafruitIO_Feed *west  = io.feed("west_traffic");
  AdafruitIO_Feed *n_pedestrian = io.feed("pedestrian_btn");
  AdafruitIO_Feed *s_pedestrian = io.feed("pedestrian_btn");
  AdafruitIO_Feed *e_pedestrian = io.feed("pedestrian_btn");
  AdafruitIO_Feed *w_pedestrian = io.feed("pedestrian_btn");
  AdafruitIO_Feed *n_priority = io.feed("priority_vehicle");
  AdafruitIO_Feed *s_priority = io.feed("priority_vehicle");
  AdafruitIO_Feed *e_priority = io.feed("priority_vehicle");
  AdafruitIO_Feed *w_priority = io.feed("priority_vehicle");

  io.connect();
  const unsigned long startAttemptTimeIO = millis();
  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(1000);
    if (millis() - startAttemptTimeIO > 10000) { // If we've waited 10 seconds, give up
      Serial.println("\nFailed to connect to AdafruitIO, clearing Preferences");
      preferences.begin("wifiCreds", false);
      preferences.clear();
      preferences.end();
      delay(1000);
      ESP.restart();
    }
  }
  Serial.println("Connected to Adafruit IO!");

  // Start syncing with all feeds
  north->get();
  n_pedestrian->get();
  n_priority->get();
  south->get();
  s_pedestrian->get();
  s_priority->get();
  east->get();
  e_pedestrian->get();
  e_priority->get();
  west->get();
  w_pedestrian->get();
  w_priority->get();

}

void start_access_point()
{
  // Start the Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Serve the HTML form to the user
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlForm);
  });

  // Save credentials when the form is submitted
  server.on("/save", HTTP_POST, []( AsyncWebServerRequest *request){
    // Print request details for debugging
    Serial.println("Request received:");

    // Print request URL
    Serial.println("URL: " + request->url());

    // Print HTTP method (GET, POST, etc.)
    Serial.println("Method: " + String(request->methodToString()));

    // Print headers
    Serial.println("Headers:");
    for (size_t i = 0; i < request->headers(); i++) {
      const AsyncWebHeader* header = request->getHeader(i);
      Serial.print(header->name());
      Serial.print(": ");
      Serial.println(header->value());
    }

    // Print parameters (if they exist)
    Serial.println("Parameters:");
    for (size_t i = 0; i < request->params(); i++) {
      const AsyncWebParameter* p = request->getParam(i);
      Serial.print(p->name());
      Serial.print(": ");
      Serial.println(p->value());
    }
    // Check if the necessary parameters are present
 if (request->hasParam("ssid", true) && request->hasParam("password", true) && request->hasParam("aio_username", true) && request->hasParam("aio_key", true)) {
   // Extract parameters
    wifi_ssid = request->getParam("ssid", true)->value();
    wifi_password = request->getParam("password", true)->value();
    aio_username = request->getParam("aio_username", true)->value();
    aio_key = request->getParam("aio_key", true)->value();

   // Print credentials (for debugging purposes)
   Serial.println("Saving credentials...");
   Serial.println("SSID: " + wifi_ssid);
   Serial.println("Password: " + wifi_password);
   Serial.println("AIO Username: " + aio_username);
   Serial.println("AIO Key: " + aio_key);

   // Open the Preferences for reading and writing
   preferences.begin("wifiCreds", false);
   // Save the credentials to Preferences
   preferences.putString("wifi_ssid", wifi_ssid);
   preferences.putString("wifi_password", wifi_password);
   preferences.putString("aio_username", aio_username);
   preferences.putString("aio_key", aio_key);
   // Close the Preferences
   Serial.println("Credentials saved successfully!");
   preferences.end();
   // Respond to the client
   request->send(200, "text/plain", "Credentials saved successfully!");
   delay(1000);
   ESP.restart();
 } else {
   // If parameters are missing
   Serial.println("Error: Missing parameters.");
   request->send(400, "text/plain", "Error: Missing parameters");
 }
  });

  // Start the server
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

    AdafruitIO_Setup();
  } else {
    Serial.println("No WiFi credentials found. Starting Access Point.");
    start_access_point();
  }
}

void giveGreen(const String& direction) {
  Serial.println(">>>> GREEN Light Given to " + direction);
  // Example:
  // if (direction == "NORTH") digitalWrite(northLight, HIGH);
  // else if (direction == "SOUTH") ...
  // Don't forget to turn off all others before setting the right one to HIGH
}