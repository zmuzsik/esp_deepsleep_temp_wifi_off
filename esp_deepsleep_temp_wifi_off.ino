/*

  ESP-01 DHT Sensor (MQTT)

  Device sends MQTT temperature update via WiFi.
  Deepsleep is used to put the device to sleep and
  wake it every 5 minutes.

  Assume ESP-01 has been modified to allow waking
  from deepslep.

  device.h:
  Configure timing, sensor type, last digit of IP address

*/

/* Update these two files with your details  */
#include "secrets.h"
#include "device.h"

/* Required for WiFi and MQTT messages       */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define DEBUG_MODE 0


// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.
struct {
  uint32_t crc32;   // 4 bytes
  uint8_t channel;  // 1 byte,   5 in total
  uint8_t bssid[6]; // 6 bytes, 11 in total
  uint8_t padding;  // 1 byte,  12 in total
} rtcData;


DHT dht(DHTPIN, DHTTYPE);



WiFiClient wifiClient;
PubSubClient client(wifiClient);

void debugprint(String message) {
  if (DEBUG_MODE != 0) {
    Serial.println(message);
  }

}


uint32_t calculateCRC32( const uint8_t *data, size_t length ) {
  uint32_t crc = 0xffffffff;
  while ( length-- ) {
    uint8_t c = *data++;
    for ( uint32_t i = 0x80; i > 0; i >>= 1 ) {
      bool bit = crc & 0x80000000;
      if ( c & i ) {
        bit = !bit;
      }

      crc <<= 1;
      if ( bit ) {
        crc ^= 0x04c11db7;
      }
    }
  }

  return crc;
}

int connect() {

  int counter = 0;
  int result = 0;

  debugprint("Retrieve WiFi settings... ");

  // Try to read WiFi settings from RTC memory
  bool rtcValid = false;
  if ( ESP.rtcUserMemoryRead( 0, (uint32_t*)&rtcData, sizeof( rtcData ) ) ) {
    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
    if ( crc == rtcData.crc32 ) {
      rtcValid = true;
    }
  }

  debugprint("Connecting to ");
  debugprint(WIFI_SSID);

  IPAddress ip( 192, 168, 1, IPNR );
  IPAddress gateway( 192, 168, 1, 1 );
  IPAddress subnet( 255, 255, 255, 0 );

  /* enable wifi */
  WiFi.forceSleepWake();
  delay(1);
  WiFi.persistent(false);


  /* Set to station mode and connect to network */
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);

  if ( rtcValid ) {
    // The RTC data was good, make a quick connection
    debugprint("WiFi quick connect data found");
    WiFi.begin(WIFI_SSID, WIFI_PASSWD, rtcData.channel, rtcData.bssid, true);
  }
  else {
    // The RTC data was not valid, so make a regular connection
    debugprint("NO WiFi quick connect data found");
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  }

  /* Wait for connection */
  while ((WiFi.status() != WL_CONNECTED) && (counter < RETRY_COUNT)) {
    counter++;

    if ( counter == 100 ) {
      // Quick connect is not working, reset WiFi and try regular connection
      debugprint("Quick connect NOT working, normal connect");
      WiFi.disconnect();
      delay(10);
      WiFi.forceSleepBegin();
      delay(10);
      WiFi.forceSleepWake();
      delay(10);
      WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    }
    if ( counter == 600 ) {
      // Giving up after 30 seconds and going back to sleep
      debugprint("Giving up on WiFi, back to sleep!");
      WiFi.disconnect(true);
      delay(1);
      WiFi.mode(WIFI_OFF);
      ESP.deepSleep(DEVICE_SLEEP * 60 * 1000000, WAKE_RF_DISABLED);

    }


    debugprint(".");
    delay(50);
  }

  /* Check if we obtained an IP address */
  if (WiFi.status() != WL_CONNECTED) {
    debugprint("Failed to connect. Sleeping!");
    result = 0;
  } else {
    debugprint("WiFi connected");

    // Write current connection info back to RTC
    rtcData.channel = WiFi.channel();
    memcpy( rtcData.bssid, WiFi.BSSID(), 6 ); // Copy 6 bytes of BSSID (AP's MAC address)
    rtcData.crc32 = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
    ESP.rtcUserMemoryWrite( 0, (uint32_t*)&rtcData, sizeof( rtcData ) );
    result = 1;
  }

  return result;

}

void setup() {
  // turn off wifi on (re-)start to save battery
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);


  int result = 0;
  int counter = 0;

  /* Serial connection */
  if (DEBUG_MODE != 0) {
    Serial.begin(115200);
  }


  // Initialize DHT sensor and wait 1 second for initialization

  dht.begin();
  delay(1000);

  /* Get ChipID and convert to char array */
  char strChipID[33];
  itoa(ESP.getChipId(), strChipID, 10);

  /* Concatenate to form MQTT topic */
  String strTopic = MQTT_TOPIC_PREFIX;
  strTopic = strTopic + "ESP" + strChipID;

  /* Convet to char array */
  char charTopic[strTopic.length() + 1];
  strTopic.toCharArray(charTopic, strTopic.length() + 1);
  debugprint(charTopic);



  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();


  if (isnan(h) || isnan(t)) {
    debugprint("ERROR: Failed to read from DHT sensor!");
    //      return;
  } else {
    debugprint((String)h);
    debugprint((String)t);
  }




  DynamicJsonDocument root(200);

  // INFO: the data must be converted into a string; a problem occurs when using floats...
  root["temperature"] = (String)t;
  root["humidity"] = (String)h;

  if (DEBUG_MODE != 0) {
    debugprint("JSON:");
    serializeJson(root, Serial);
    debugprint("");
  }

  char data[200];
  serializeJson(root, data);


  /* Attempt to connect to WiFi */
  result = connect();

  if (result == 1) {

    /* Connect to MQTT broker */
    client.setServer(MQTT_SERVER, MQTT_PORT);

    if (client.connect(strChipID, MQTT_USER, MQTT_PASSWD))
    {
      debugprint("Publishing ...");
      if (client.publish(charTopic, data, 1)) {
        debugprint("Topic: " + strTopic);
        debugprint("Message published");
      } else {
        debugprint("Error publishing message");
      }

    } else {
      debugprint("Error connecting to MQTT broker");
    }

    /* Close MQTT client cleanly */
    client.disconnect();


  }
  /* Close WiFi connection */
  WiFi.disconnect(true);
  delay(1);

  /* Put device into deep sleep.
     Although without hardware mod it will never wake up from this! */
  debugprint("Time to sleep!");
  ESP.deepSleep(DEVICE_SLEEP * 60 * 1000000, WAKE_RF_DISABLED);

}

void loop() {

}
