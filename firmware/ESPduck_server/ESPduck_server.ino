/*********
 Rubberduck Firmware version 2.1
 31-01-2021 Tim Borowy

 ToDo: add mqtt password protection and SSL
*********/
#include <Wire.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include "Settings.h"
// Number of leds
CRGB leds[7];

const uint16_t MPU_addr=0x68;  // I2C address of the MPU-6050
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
int shakeCount = 0;
int shakeCountThreshhold = 3; // Amount of shakes required for a detection
int last_x,last_y,last_z;

const char* ssid = CONFIG_SSID;
const char* password = CONFIG_PASS;
const char* deviceId = CONFIG_DEVICE_ID;
const char* mqtt_server = CONFIG_MQTT_SERVER;
bool lightState = true;

WiFiClient espClient;
PubSubClient mqtt(espClient);
long lastMsg = 0;

void setup() {
  Serial.begin(115200);

  // MPU setup
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);

  // WIFI setup with manager
  setup_wifi();

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);

  // ledpin = 12, num of leds = 7 
  FastLED.addLeds<NEOPIXEL, 12>(leds, 7);  // GRB ordering is assumed

}

void setup_wifi() {
  WiFiManager wm;
  // Resets WiFi setting for development
  //wm.resetSettings();

  bool res;
  res = wm.autoConnect("Duck setup AP");

  if(!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
  }
}

// Receiving messages
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String msg;

  // process MQTT message
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    msg += (char)message[i];
  }
  Serial.println();

  // Changes the lights according to the message
  if (String(topic) == "ducks/flash") {
    lightState = !lightState;
    Serial.println("lightstate changed");
    Serial.println(msg);
  }
  if (String(topic) == "ducks//flash") { // todo add sub to personal device id
    lightState = !lightState;
    Serial.println("lightstate changed");
    Serial.println(msg);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(deviceId, "ducks", "Aremadefromrubber")){
      Serial.println("connected");
      // Subscribe
      mqtt.subscribe("ducks/flash");
      char channel[20];
      sprintf(channel, "ducks/%s/flash", deviceId);
      mqtt.subscribe(channel);
    } else {
      // Serial.print("failed to connect to: ");
      // Serial.print(CONFIG_MQTT_SERVER);
      // Serial.print(" Using user/pass: ");
      // Serial.print("rc=");
      // Serial.print(mqtt.state());
      char buffer[40];
      // this is untested
      sprintf(buffer, "failed to connect to: %s Using user/pass: %s rc= %d", mqtt_server, "ducks",  mqtt.state());
      Serial.println(" trying again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
int counter = 0;
void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  // ?
  mqtt.loop();

  // Clear pixels
  for(int i=0;i<7;i++){
    leds[i] = CRGB::Black;
  }
  
  if(lightState){
    counter++;
    // Make pixel red
    leds[counter%7].setRGB( 255, 0, 221);
  } else {
    // Make green
    for(int i=0;i<7;i++){
      leds[i].setRGB( 0, 255, 0);
    }
  }
  FastLED.show();

  // Detect shakes every 100ms without delay
  long now = millis();
  if (now - lastMsg > 100) {
    lastMsg = now;

    detectShake();
  }
}

bool detectShake(){
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  uint8_t stackSize = 14;
  Wire.requestFrom(MPU_addr,stackSize,true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)


  // Shake detection logic
  int val = abs(AcX+AcY+AcZ - last_x - last_y - last_z);
  Serial.println(val);
  if(val > 25000){
    // Shaking is detected in an axis

    // Up the count and check if count is over threshhold
    if(shakeCount++ > shakeCountThreshhold){
      //Should send request
      sendShakeDetectionRequest();
      // Reset shakecount
      shakeCount = 0;
    }
  } else {
    // Reset shakecount
    shakeCount = 0;
  }
  
  last_x = AcX;
  last_y = AcY;
  last_z = AcZ;
}

void sendShakeDetectionRequest(){
  const size_t capacity = JSON_OBJECT_SIZE(5);
  DynamicJsonDocument doc(capacity);
  
  doc["shake"] = true;
  doc["signal"] = WiFi.RSSI();
  doc["device_id"] = deviceId;
  doc["light_state"] = lightState;

  char buffer[capacity];
  serializeJson(doc, buffer);
  
  Serial.println(buffer);

  mqtt.publish("ducks/shake", buffer);
}
