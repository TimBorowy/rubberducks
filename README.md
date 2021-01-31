# An event gateway for the Duck Buddies
Connects to the MQTT gateway and is used to commuticate and orchistrate the ESPDuckies

![](https://i.imgur.com/7J5Kn.jpg)

## Install
Run `npm install` in the root directory and run `npm start` to spin 'er up

Copy the settings_example.h to a settings.h file and fill in the WiFi details.

Flash the ESPduck_server.ino on the ESP32's 

## Specific Libraries used:
- ArduinoJSON https://github.com/bblanchon/ArduinoJson
- PubSubClient https://github.com/knolleary/pubsubclient
- MQTT https://www.npmjs.com/package/mqtt




## See data / interaction
On `/` you can see the emotional states of the duckies (anger)  
Checkout `/data` to see the logs and toggle light states.
