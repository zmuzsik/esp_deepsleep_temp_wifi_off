# esp_deepsleep_temp_wifi_off
ESP8622 temperature reporting via MQTT

Uses a DHT sensor to read temperature and humidity values.
Then starts WIFI and pushes the readings to an MQTT topic.
Based on:
https://bitbucket.org/MattHawkinsUK/home-automation/src/master/esp_01_ds18b20_mqtt/
https://www.tech-spy.co.uk/2019/07/battery-powered-esp-01-temperature-sensor/
