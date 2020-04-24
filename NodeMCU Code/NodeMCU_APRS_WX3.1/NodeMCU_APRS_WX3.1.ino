/**
 * NodeMCU_APRS_WX3
 * Version: 3.1.0
 * 
 * An IoT Weather Station based on NodeMCU.
 * Current version support:
 * - DHT22 Sensor (Removed in Version 3)
 * - Deep Sleep (Version 2)
 * - BME280 Sensor (Version 3.0.0)
 * 
 * In this version BME280 is operating in forced mode
 * 
 * Output:
 * - Sending Measurement to APRS Network (Version 1.0.0)
 * - Sending Measurement to ThingSpeak (Version 2.1.0)
 */

//#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Configuration. Should be self explanatory
#define WIFI_SSID "myWiFi"                   // WiFi SSID
#define WIFI_PASS "Sup3rSecr3t"             // WiFi Password
#define STATION_ID "MYCALL-13"                // Station callsign
#define STATION_LAT "0123.45S"              // Lattitude position of Station
#define STATION_LONG "11145.12E"            // Longitude position of Station
#define STATION_TZ 0                             // Time difference from GMT. Set 0 to use GMT Time
#define APRS_SERVER_NAME "rotate.aprs2.net"   // Name for APRS Server
#define APRS_SERVER_PORT 14580                   // Port for APRS Server. Default 14580
#define APRS_PASS "11111"                     // Password for APRS 
#define SCAN_INTERVAL 600   // Inteval pembacaan cuaca dalam detik
#define THINGSPEAK_CLIENT_ID "MYCALL-13"      // Bisa diganti dengan Client ID Sembarang
#define THINGSPEAK_MQTT_API "ABABABABABABA"  // Ganti dengan User MQTT API Key dari ThingSpeak
#define THINGSPEAK_CHANNEL_API "ABABABABABABA"     // Ganti dengan Channel API Key dari ThingSpeak
#define THINGSPEAK_CHANNEL_ID 123131           // Ganti dengan Channel ID dari ThingSpeak
// End Configuration

// Setup for BME 280 Sensor
#define BME_SDA D3
#define BME_SCL D4
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11 
#define BME_CS 10
Adafruit_BME280 bme;

// Setup for NTP
WiFiUDP NtpUdp;
unsigned int localPort = 8888; // local port to listen for UDP packets
void sendNTPpacket(IPAddress &address);

void connect () {
  // Connect to Wifi.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // WiFi fix: https://github.com/esp8266/Arduino/issues/2186
  // WiFi.persistent(false);
  // WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long wifiConnectStart = millis();

  while (WiFi.status() != WL_CONNECTED) {
    // Check to see if
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Failed to connect to WIFI. Please verify credentials: ");
      Serial.println();
      Serial.print("SSID: ");
      Serial.println(WIFI_SSID);
      Serial.print("Password: ");
      Serial.println(WIFI_PASS);
      Serial.println();
    }

    delay(500);
    Serial.println(WiFi.status());
    // Only try for 5 seconds.
    if(millis() - wifiConnectStart > 10000) {
      Serial.println("Failed to connect to WiFi");
      Serial.println("Please attempt to send updated configuration parameters.");
      return;
    }
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

WiFiClient APRSConnect() {
  WiFiClient myclient;
  Serial.print("Connecting to APRS Server: ");
  Serial.print(APRS_SERVER_NAME);
  Serial.print(":");
  Serial.println(APRS_SERVER_PORT);
  if (!myclient.connect(APRS_SERVER_NAME, APRS_SERVER_PORT)) {
    Serial.println("connection failed");
    return(myclient);
  }
  Serial.print("Connected! Sending login credential for ");
  Serial.println(STATION_ID);
  myclient.print("user ");
  myclient.print(STATION_ID);
  myclient.print(" pass ");
  myclient.print(APRS_PASS);
  myclient.print(" vers NodeMCU_APRS_WX_v2.1.0");
  myclient.print("\n");

  // Waiting for Response from APRS Server
  int i=0;
  while((!myclient.available()) && (i<500)){
    delay(100);
    i++;
  }

  // Receiving Responses
  while(myclient.available()){
    String line = myclient.readStringUntil('\r');
    Serial.print(line);  
  }

  return (myclient);
}

void APRSTransmit(WiFiClient myclient, float h, float f, float p) {
  myclient.print(STATION_ID);
  myclient.print(">APRS,TCPIP*:@");
  if (day() < 10) {
    myclient.print("0");
  }
  myclient.print(day());
  if (hour() < 10) {
    myclient.print("0");
  }
  myclient.print(hour());
  if (minute() < 10) {
    myclient.print("0");
  }
  myclient.print(minute());
  myclient.print("z");
  myclient.print(STATION_LAT);
  myclient.print("/");
  myclient.print(STATION_LONG);
  myclient.print("_");
  myclient.print("...");          // Wind Direction from True North
  myclient.print("/");
  myclient.print("...");          // Wind speed in mph
  myclient.print("g");
  myclient.print("...");          // Wind Gust
  myclient.print("t");
  if (f<10) {
    myclient.print("00");
  } else if (f<100) {
    myclient.print("0");
  }
  myclient.print(round(f));          // Temperature in Farenheit
  myclient.print("r");
  myclient.print("...");          // Rain in hundred of inch in last 1 hour
  myclient.print("p");
  myclient.print("...");          // Rain in hundred of inch in last 24 hour
  myclient.print("P");
  myclient.print("...");          // Rain in hundred of inch since midnight
  myclient.print("b");
  if (p<100000) {
    myclient.print("0");
  }
  myclient.print(round(p/10));        // Pressure in tenths of milibar
  myclient.print("h");
  myclient.print(round(h));          // Humidity in Percentage
  myclient.print("\n");  
}

void setup() {
  Serial.begin(115600);
  Serial.setTimeout(2000);

  // Wait for serial to initialize.
  while(!Serial) { }

  Serial.println("Device Started");
  Serial.println("-------------------------------------");
  Serial.println("Running NodeMCU_APRS_WX3!");
  Serial.println("-------------------------------------");

  connect();      // Connecting to WiFi

  // Getting time from NTP
  Serial.println("Starting UDP");
  NtpUdp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(NtpUdp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  // Reading sensor data from BME280
  //Wire.begin(BME_SDA, BME_SCL);
  if (!bme.begin()) {  
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    delay(1000);
    while (1);
  }
  // Set BME Mode to Forced Mode
  // Code from Adafruit sample library
  Serial.println("-- Weather Station Scenario --");
  Serial.println("forced mode, 1x temperature / 1x humidity / 1x pressure oversampling,");
  Serial.println("filter off");
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF   );
                     
  int BMEReadResult = 0;
  float p = 0;
  float h = 0;
  float f = 0;
  float t = 0;
  while (BMEReadResult == 0) {
    Serial.println("Reading BME280");
    p = bme.readPressure();
    if (isnan(p)) {
      Serial.println("Failed to read from Pressure from BME sensor!");
      delay(100);
      continue;
    }
    t = bme.readTemperature();
    if (isnan(t)) {
      Serial.println("Failed to read from Temperature from BME sensor!");
      delay(100);
      continue;
    }
    f = round(t * 9/5 + 32);
    h = bme.readHumidity();
    if (isnan(h)) {
      Serial.println("Failed to read from Humidity from BME sensor!");
      delay(100);
      continue;
    }
    BMEReadResult = 1;
    /*
    Serial.print("Pressure (Pa): ");
    Serial.println(p);
    Serial.print("Pressure (mBar): ");
    Serial.println(p/100);
    Serial.print("Temperature (C/F): ");
    Serial.print(t);
    Serial.print("/");
    Serial.println(f);
    Serial.print("Humidity (%): ");
    Serial.println(h);
    */
  }

  // Connecting to APRS Server
  WiFiClient wifiClient = APRSConnect();
  APRSTransmit(wifiClient, h, f, p);
  wifiClient.stop();

  // Sending data to ThingSpeak
  //int resultThingSpeak = sendThingSpeak(t, h, p/100);

  delay(1000);

  // Entering Deep Sleep
  Serial.println("Entering Deep Sleep");
  ESP.deepSleep(SCAN_INTERVAL*1000000);
}

void loop() {

}
