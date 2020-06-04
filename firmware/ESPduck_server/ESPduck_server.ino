
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
WiFiMulti wifiMulti;
#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);

const int MPU_addr=0x68;  // I2C address of the MPU-6050
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
int shakeCount = 0;
int shakeThreshhold = 6; // Amount of shakes required for a detection

const char* ssid = "bobonet 3.6";
const char* pass = "Hetisjouwwachtwoord!";
String deviceId = "duck_one";
bool lightState = true;

int counter = 0;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(7, 12, NEO_GRB + NEO_KHZ800);


void setup() {

  Serial.begin(115200);
  
  // MPU setup
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);

  
  // WIFI setup
  Serial.print("Connecting");
  
  wifiMulti.addAP(ssid, pass);
  
  // While wifi not connected yet, print '.'
  while (wifiMulti.run() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
  }
  
  // Connected
  Serial.println("OK!");
  
  // Access Point (SSID).
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  
  // IP adres
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
 
  // Signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signaal sterkte (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println("");

  pixels.begin();


  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/toggle_light", handleLightChange);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // Clear pixels
  for(int i=0;i<7;i++){
    pixels.setPixelColor(i, pixels.Color(0,0,0));
  }
  
  if(lightState){
    counter++;
    // Make pixel red
    pixels.setPixelColor(counter%7, pixels.Color(255,0,211));
  } else {
    // Make green
    for(int i=0;i<7;i++){
      pixels.setPixelColor(i, pixels.Color(0,255,0));
    }
  }
  
  pixels.show();
  
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)


  // Shake detection logic
  if(
      (GyX > 20000 || GyX < -20000) ||
      (GyY > 20000 || GyY < -20000) ||
      (GyY > 20000 || GyY < -20000)
    ){
    // Shaking is detected in an axis

    // Up the count and check if count is over threshhold
    if(shakeCount++ > shakeThreshhold){
      //Send request
      sendRequest();
      // Reset shakecount
      shakeCount = 0;
    }
  } else {
    // Reset shakecount
    shakeCount = 0;
  }

  // Preform detection every 100ms
  delay(100);
}

void handleRoot() {
  server.send(200, "text/plain", "Hello from ESPduck! DeviceId: " + deviceId);
}

void handleLightChange() {
  lightState = !lightState;
  const size_t capacity = JSON_OBJECT_SIZE(5);
    DynamicJsonDocument doc(capacity);
    
    doc["shake"] = "false";
    doc["signal"] = WiFi.RSSI();
    doc["device_id"] = deviceId;
    doc["light_state"] = lightState;

    char buffer[capacity];
    serializeJson(doc, buffer);
  server.send(200, "application/json", buffer);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void sendRequest(){
  if((wifiMulti.run() == WL_CONNECTED)) {
    
    HTTPClient http;

    http.begin("http://192.168.1.21:1337/log_action"); //Specify request destination
    http.addHeader("Content-Type", "application/json"); //Specify content-type header

    const size_t capacity = JSON_OBJECT_SIZE(5);
    DynamicJsonDocument doc(capacity);
    
    doc["shake"] = "true";
    doc["signal"] = WiFi.RSSI();
    doc["device_id"] = deviceId;
    doc["light_state"] = lightState;

    char buffer[capacity];
    serializeJson(doc, buffer);
    
    Serial.println(buffer);
    int httpCode = http.POST(buffer); //Send the request
    String payload = http.getString();  //Get the response payload

    Serial.println("StatusCode: ");
    Serial.println(httpCode); //Print HTTP return code
    Serial.println("Response Payload: ");
    Serial.println(payload); //Print request response payload
 
    http.end();  //Close connection
  } else {
    Serial.println("Error in WiFi connection");   
  }
}
