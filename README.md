# SolarMonitorUtil
Using ESP8266 Wemos with ADS1015 ADC, 128 x 32 OLED display, WCS1600 [ non contact hall current sensor] to monitor Solar power utilization. 
Used ESPAsyncOTA, NTP lib for time sync, Simple timer for tasking , Wi-Fi manager, web server library. 
Locally connects to router , display ip address on OLED display and starts a webserver. 
Posts messages to 'thing speak channel'
ESP8266 on chip flash as EEPROM to store energy value 
