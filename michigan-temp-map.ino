#include <cstdlib>
#include <time.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <SolarCalculator.h>
#include <map2colour.h>

#define PIN 16
#define LEDS 48

time_t now;
struct tm *timeinfo;

const String stations[] = {"KBEH","KLWA","KAZO","KOEB","KJXN","KARB","KDUH","KDTW","KMTC","KPTK","KOZW","KLAN","KGRR","KBIV","KMKG","KFFX","KRQB","KAMN","KFNT","KD95","KPHN","KP58","KBAX","KCFS","KHYX","KIKW","KOSC","KAPN","KGLR","KHTL","KCAD","KMBL","KLDM","KTVC","KCVX","KSLH","KCIU","KANJ","KERY","KISQ","KP53","KSAW","KESC","KMNM","KIMT","KIWD","KCMX","KP59"};
time_t lastUpdatedTimes[sizeof(stations) / sizeof(stations[0])];
Adafruit_NeoPixel pixels(LEDS, PIN, NEO_RGB + NEO_KHZ800);
DynamicJsonDocument doc(6144);

double latitude = 42.77;
double longitude = -86.10;
double transit, sunrise, sunset, n_dawn, n_dusk;

uint32_t tempColors[] = {
  0x00734669, // #734669
  0x00caacc3, // #caacc3
  0x00a24691, // #a24691
  0x008f59a9, // #8f59a9
  0x009ddbd9, // #9ddbd9
  0x006abfb5, // #6abfb5
  0x0064a6bd, // #64a6bd
  0x005d85c6, // #5d85c6
  0x00447d63, // #447d63
  0x00809318, // #809318
  0x00f3b704, // #f3b704
  0x00e85319, // #e85319
  0x00470e00  // #470e00
}; // windy.com scale
float tempDomain[] = {-70, -55, -40, -25, -15, -8.33, -3.89, 0, 1, 10, 21, 30, 45};
map2colour m2c(13);

void setup() {

  Serial.begin(115200);
  delay(2000);

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

  Serial.println("Fetching NTP time...");

  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  // setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1); // Set to the Eastern time zone
  // tzset(); // Apply the time zone settings

  while (now < 946684800) { // Wait until a valid time is received (after 1-Jan-2000)
    Serial.print(".");
    time(&now);
    delay(1000);
  }
  timeinfo = gmtime(&now);

  Serial.print("Fetched! Current time: ");
  char time_str[9];
  sprintf(time_str, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  Serial.println(time_str);

  for (int i = 0; i < sizeof(stations) / sizeof(stations[0]); i++) {
    lastUpdatedTimes[i] = now - 3600; // Initialize our last updated array with times over an hour ago
  }

  for (int i = 0; i < LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0, 0));
  }
  pixels.show();

  m2c.begin(tempDomain, tempColors);
  
}

void loop() {

  time(&now);
  timeinfo = gmtime(&now);
  
  calcSunriseSunset(now, latitude, longitude, transit, sunrise, sunset);
  calcNauticalDawnDusk(now, latitude, longitude, transit, n_dawn, n_dusk);
  if (n_dusk >= 24) {
    n_dusk -= 24;
  }

  int hourUTC = timeinfo->tm_hour;
  int minuteUTC = timeinfo->tm_min;
  int secondUTC = timeinfo->tm_sec;
  double timeInDecimalHours = hourUTC + (minuteUTC / 60.0) + (secondUTC / 3600.0);

  if (timeInDecimalHours >= n_dawn && timeInDecimalHours <= transit) {
    int brightness = mapPercentage(timeInDecimalHours, n_dawn, transit, 0, 127);
    Serial.println("Setting brightness to " + String(brightness));
    pixels.setBrightness(brightness);
    pixels.show();
  } else if (timeInDecimalHours >= transit && timeInDecimalHours <= n_dusk) {
    int brightness = mapPercentage(timeInDecimalHours, transit, n_dusk, 127, 0);
    Serial.println("Setting brightness to " + String(brightness));
    pixels.setBrightness(brightness);
    pixels.show();
  } else {
    Serial.println("Setting brightness to 0");
    pixels.setBrightness(0);
    pixels.show();
    delay(5 * 60 * 1000);
    return;
  }

  Serial.println("Fetching stations...");

  int numberOfStations = sizeof(stations) / sizeof(stations[0]);
  for (int stationIndex = 0; stationIndex < numberOfStations; stationIndex++) {

    time(&now);
    if (difftime(now, lastUpdatedTimes[stationIndex]) > (60 * 60)) { // if it's been over an hour since last successful fetch for this one
      pixels.setPixelColor(stationIndex, pixels.Color(0, 0, 0, 0));
      pixels.show();
    }

    Serial.println("Fetching " + stations[stationIndex] + "...");

    String url = "https://api.weather.gov/stations/" + String(stations[stationIndex]) + "/observations/latest";

    String response = get(url.c_str());

    if (response == "0") { // http response error
      Serial.println("HTTP error");
      continue;
    }

    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.println("JSON parse error: " + String(error.c_str()));
      continue;
    }

    time(&now);
    String timestamp = doc["properties"]["timestamp"];
    time_t observationTime = parseTimestamp(timestamp);
    if (difftime(now, observationTime) > (4 * 60 * 60)) { // four hours
      Serial.println("Latest observation is over 4 hours old (" + timestamp + ")");
      pixels.setPixelColor(stationIndex, pixels.Color(0, 0, 0, 0));
      pixels.show();
      continue;
    }

    String unit = doc["properties"]["temperature"]["unitCode"];
    
    float temp = doc["properties"]["temperature"]["value"];
    if (unit != "wmoUnit:degC") {
      temp = (temp - 32) * 5.0 / 9.0;
    }
    
    Serial.println("Temp: " + String(temp) + "°C");

    uint32_t rgbColor = m2c.map2RGB(temp); // convert to rgb color on the scale
    byte r = (rgbColor >> 16) & 0xFF;
    byte g = (rgbColor >> 8) & 0xFF;
    byte b = rgbColor & 0xFF;
    Serial.print("Setting to #");
    if (r < 16) Serial.print("0");
    Serial.print(r, HEX);
    if (g < 16) Serial.print("0");
    Serial.print(g, HEX);
    if (b < 16) Serial.print("0");
    Serial.print(b, HEX);
    Serial.println();
    pixels.setPixelColor(stationIndex, pixels.Color(r, g, b, 0));

    lastUpdatedTimes[stationIndex] = now;
    
    pixels.show();

  }

  Serial.println("Sleeping for 5 minutes...");
  delay(5 * 60 * 1000);
  
}

int mapPercentage(double x, double in_min, double in_max, int out_min, int out_max) {
  double percentage = (x - in_min) / (in_max - in_min);
  int mapped_value = (int)(out_min + percentage * (out_max - out_min));
  return mapped_value;
}

time_t parseTimestamp(const String &timestamp) {
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  strptime(timestamp.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
  time_t time = mktime(&tm);
  return time;
}

String get(String serverName) {
  
  WiFiClientSecure client;
  HTTPClient http;

  client.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  http.begin(client, serverName);

  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "(https://herzog.tech, xd1936@gmail.com)");
  
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode > 0) {
    if (httpResponseCode == 200) {
      payload = http.getString();
    } else if (httpResponseCode >= 300 && httpResponseCode < 400) {
      // Handle redirection
      Serial.print("Redirection: ");
      Serial.println(httpResponseCode);
      return "0";
    } else if (httpResponseCode >= 400 && httpResponseCode < 500) {
      // Handle client-side errors
      Serial.print("Client error: ");
      Serial.println(httpResponseCode);
      return "0";
    } else if (httpResponseCode >= 500 && httpResponseCode < 600) {
      // Handle server-side errors
      Serial.print("Server error: ");
      Serial.println(httpResponseCode);
      return "0";
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    return "0";
  }
   
  http.end();

  return payload;

}
