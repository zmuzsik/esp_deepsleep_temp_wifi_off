#include "DHT.h"

// Your device details
#define MQTT_TOPIC_PREFIX "home/"
#define DEVICE_SLEEP      10
#define RETRY_COUNT       600

// Sensor definition & init
#define DHTPIN 2
#define DHTTYPE DHT11

#define IPNR 251
