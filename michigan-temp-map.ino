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

const char* ssid = "IoT";
const char* psk = "humanbeings";
const String stations[] = {"KBEH","KLWA","KAZO","KOEB","KJXN","KARB","KDUH","KDTW","KMTC","KPTK","KOZW","KLAN","KGRR","KBIV","KMKG","KFFX","KRQB","KAMN","KFNT","KD95","KPHN","KP58","KBAX","KCFS","KHYX","KIKW","KOSC","KAPN","KGLR","KHTL","KCAD","KMBL","KLDM","KTVC","KCVX","KSLH","KCIU","KANJ","KERY","KISQ","KP53","KSAW","KESC","KMNM","KIMT","KIWD","KCMX","KP59"};
time_t lastUpdatedTimes[sizeof(stations) / sizeof(stations[0])];

Adafruit_NeoPixel pixels(LEDS, PIN, NEO_RGB + NEO_KHZ800);

double latitude = 42.77;
double longitude = -86.10;
double a_dawn, transit, a_dusk;

// uint32_t tempColors[] = {
//   0x00734669, // #734669
//   0x00caacc3, // #caacc3
//   0x00a24691, // #a24691
//   0x008f59a9, // #8f59a9
//   0x009ddbd9, // #9ddbd9
//   0x006abfb5, // #6abfb5
//   0x0064a6bd, // #64a6bd
//   0x005d85c6, // #5d85c6
//   0x00447d63, // #447d63
//   0x00809318, // #809318
//   0x00f3b704, // #f3b704
//   0x00e85319, // #e85319
//   0x00470e00  // #470e00
// }; // original windy.com scale
uint32_t tempColors[] = {
  0x00853373, // #853373
  0x00d89eca, // #d89eca
  0x00b92fa0, // #b92fa0
  0x008f59a9, // #8f59a9
  0x009840c2, // #9840c2
  0x0055d4c5, // #55d4c5
  0x004eb1d3, // #4eb1d3
  0x004780dc, // #4780dc
  0x00319065, // #319065
  0x008ca407, // #8ca407
  0x00f7b900, // #f7b900
  0x00ff4902, // #ff4902
  0x00470e00  // #470e00
}; // saturated 20% windy.com scale
float tempDomain[] = {-70, -55, -40, -25, -15, -8.33, -3.89, 0, 1, 10, 21, 30, 45};
map2colour m2c(13);

void setup() {

  Serial.begin(115200);
  delay(2000);

  pixels.begin();
  pixels.setBrightness(31);

  for (int i = 0; i < LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0, 0));
  }
  pixels.show();
  
  WiFi.mode(WIFI_STA);
  Serial.println("Attempting to connect...");
  WiFi.begin(ssid, psk);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    if (millis() - startTime >= 60000) {
      Serial.println("Failed to connect to WiFi within 60 seconds. Restarting...");
      ESP.restart();
    }
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.println(WiFi.localIP());

  for (int i = 0; i < LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 0, 0));
  }
  pixels.show();

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

  for (int i = 0; i < LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0, 0));
  }
  pixels.show();

  for (int i = 0; i < sizeof(stations) / sizeof(stations[0]); i++) {
    lastUpdatedTimes[i] = now - 3600; // Initialize our last updated array with times over an hour ago
  }

  m2c.begin(tempDomain, tempColors);

  for (int i = 0; i < LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0, 0));
  }
  pixels.show();
  
}

void loop() {

  time(&now);
  timeinfo = gmtime(&now);
  
  calcAstronomicalDawnDusk(now, latitude, longitude, transit, a_dawn, a_dusk);

  int hourUTC = timeinfo->tm_hour;
  int minuteUTC = timeinfo->tm_min;
  int secondUTC = timeinfo->tm_sec;
  double timeInDecimalHours = hourUTC + (minuteUTC / 60.0) + (secondUTC / 3600.0);

  Serial.println(timeInDecimalHours);
  Serial.println(a_dawn);
  Serial.println(transit);
  Serial.println(a_dusk);

  double effectiveTimeInDecimalHours = timeInDecimalHours;
  if (a_dusk > 24 && timeInDecimalHours + 24 <= a_dusk) {
    effectiveTimeInDecimalHours += 24;
  }
  if (timeInDecimalHours >= a_dawn && timeInDecimalHours <= transit) {
    // if between a_dawn and transit
    int brightness = mapPercentage(effectiveTimeInDecimalHours, a_dawn, transit, 0, 127);
    Serial.println("Setting brightness to " + String(brightness));
    pixels.setBrightness(brightness);
    pixels.show();
} else if ((timeInDecimalHours >= transit && timeInDecimalHours <= a_dusk) ||
           (effectiveTimeInDecimalHours >= transit && effectiveTimeInDecimalHours <= a_dusk)) {
    // if between transit and a_dusk
    int brightness = mapPercentage(effectiveTimeInDecimalHours, transit, a_dusk, 127, 0);
    Serial.println("Setting brightness to " + String(brightness));
    pixels.setBrightness(brightness);
    pixels.show();
  } else {
    // must be after a_dusk or before a_dawn
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
    if (difftime(now, lastUpdatedTimes[stationIndex]) > (2 * 60 * 60)) { // if it's been over two hours since last successful fetch for this one
      pixels.setPixelColor(stationIndex, pixels.Color(0, 0, 0, 0));
      pixels.show();
    }

    String station = stations[stationIndex];

    Serial.println("Fetching " + station + "...");

    float temp = getStationTemp(station);
    if (temp == -999) { // error fetching actual station. fall back on nws json api.
      Serial.println("No live " + station + " observations. Falling back on nearby station observations...");
      temp = getNearestStationTemp(station);
    }
    if (temp == -999) { // error fetching actual station. fall back on nws json api.
      Serial.println("No nearby station observations either. Falling back on forecast data...");
      temp = getForecastTemp(station);
    }
    
    if (temp == -999) {
      // don't update if we failed all above
      continue;
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

    time(&now);
    lastUpdatedTimes[stationIndex] = now;
    
    pixels.show();

  }

  Serial.println("Sleeping for 5 minutes...");
  delay(5 * 60 * 1000);
  
}

float getStationTemp(String stationId) {

  JsonDocument doc;
  
  String url = "https://api.weather.gov/stations/" + stationId + "/observations?limit=5";

  String response = get(url.c_str());

  if (response == "{}") { // http response error
    Serial.println("HTTP error");
    return -999;
  }

  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    Serial.println(String(response));
    return -999;
  }

  time(&now);
  JsonArray observations = doc["features"];
  for(JsonVariant observation : observations) {

    String timestamp = observation["properties"]["timestamp"];
    time_t observationTime = parseTimestamp(timestamp);
    if (difftime(now, observationTime) > (2 * 60 * 60)) { // two hours
      continue;
    }
    
    if (observation["properties"]["temperature"]["value"].isNull()) {
      continue;
    }

    String unit = observation["properties"]["temperature"]["unitCode"];
    float temp = observation["properties"]["temperature"]["value"];
    if (unit != "wmoUnit:degC") {
      temp = (temp - 32) * 5.0 / 9.0;
    }
    
    return temp;

  }

  return -999;

}

float getNearestStationTemp(String stationId) {

  JsonDocument doc;

  String stationUrl = "https://api.weather.gov/stations/" + stationId;

  String response = get(stationUrl.c_str());

  if (response == "{}") { // http response error
    Serial.println("HTTP error");
    return -999;
  }

  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    Serial.println(String(response));
    return -999;
  }

  String lat = doc["geometry"]["coordinates"][1];
  String lon = doc["geometry"]["coordinates"][0];

  String pointUrl = "https://api.weather.gov/points/" + lat + "," + lon;

  response = get(pointUrl.c_str());

  if (response == "{}") { // http response error
    Serial.println("HTTP error");
    return -999;
  }

  error = deserializeJson(doc, response);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    Serial.println(String(response));
    return -999;
  }

  String stationsUrl = doc["properties"]["observationStations"];
  stationsUrl = stationsUrl + "?limit=5";

  response = get(stationsUrl.c_str());

  if (response == "{}") { // http response error
    Serial.println("HTTP error");
    return -999;
  }

  error = deserializeJson(doc, response);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    Serial.println(String(response));
    return -999;
  }

  JsonArray stations = doc["features"];
  
  for (JsonVariant station : stations) {
    String nearbyStationId = station["properties"]["stationIdentifier"];
    float temp = getStationTemp(nearbyStationId);
    if (temp != -999) {
      return temp;
    }
  }

  return -999;

}

float getForecastTemp(String stationId) {

  JsonDocument doc;

  String stationUrl = "https://api.weather.gov/stations/" + stationId;

  String response = get(stationUrl.c_str());

  if (response == "{}") { // http response error
    Serial.println("HTTP error");
    return -999;
  }

  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    Serial.println(String(response));
    return -999;
  }

  String lat = doc["geometry"]["coordinates"][1];
  String lon = doc["geometry"]["coordinates"][0];

  String pointUrl = "https://api.weather.gov/points/" + lat + "," + lon;

  response = get(pointUrl.c_str());

  if (response == "{}") { // http response error
    Serial.println("HTTP error");
    return -999;
  }

  error = deserializeJson(doc, response);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    Serial.println(String(response));
    return -999;
  }

  String forecastUrl = doc["properties"]["forecast"];
  forecastUrl = forecastUrl + "?units=si";

  response = get(forecastUrl.c_str());

  if (response == "{}") { // http response error
    Serial.println("HTTP error");
    return -999;
  }

  error = deserializeJson(doc, response);
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    Serial.println(String(response));
    return -999;
  }

  float temp = doc["properties"]["periods"][0]["temperature"].as<float>();

  Serial.println(temp);

  return temp;

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

  Serial.println("GET: " + serverName);
  
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
      return "{}";
    } else if (httpResponseCode >= 400 && httpResponseCode < 500) {
      // Handle client-side errors
      Serial.print("Client error: ");
      Serial.println(httpResponseCode);
      return "{}";
    } else if (httpResponseCode >= 500 && httpResponseCode < 600) {
      // Handle server-side errors
      Serial.print("Server error: ");
      Serial.println(httpResponseCode);
      return "{}";
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    return "{}";
  }
   
  http.end();

  return payload;

}