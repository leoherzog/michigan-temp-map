#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

#define PIN 16
#define LEDS 48

Adafruit_NeoPixel pixels(LEDS, PIN, NEO_RGB + NEO_KHZ800);
DynamicJsonDocument doc(4096);

void setup() {

  pixels.begin();
  pixels.setBrightness(63);

  for (int i = 0; i < LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0, 0));
  }
  pixels.show();
  
  WiFi.mode(WIFI_STA);
  Serial.println("Attempting to connect...");
  WiFi.begin("IoT", "humanbeings");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    for (int i = 0; i < LEDS; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 0, 0));
    }
    pixels.show();
    delay(1000);
  }

  Serial.println(WiFi.localIP());
  
  for (int i = 0; i < LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0, 0));
  }
  pixels.show();
  
}

void loop() {

  Serial.println("Fetching...");
  
  String url = "https://script.google.com/macros/s/AKfy....4L4Q/exec";
  
  String response = get(url.c_str());
  Serial.println(response);

  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  pixels.setBrightness(doc["brightness"]);
  
  JsonArray colorsArray = doc["colors"].as<JsonArray>();
  for (int i = 0; i < colorsArray.size(); i++) {
    JsonVariant rgb = colorsArray[i];
    pixels.setPixelColor(i, pixels.Color(rgb[0], rgb[1], rgb[2], 0));
    pixels.show();
  }

  Serial.println("Sleeping for 60 seconds...");
  delay(60 * 1000);
  
}

String get(String serverName) {
  
  WiFiClientSecure client;
  HTTPClient http;

  client.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  http.begin(client, serverName);
  
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    ESP.restart();
  }
   
  http.end();

  return payload;
}
