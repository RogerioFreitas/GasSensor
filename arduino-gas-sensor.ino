#include <FS.h>  //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>   //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <ArduinoJson.h>         //https://github.com/bblanchon/ArduinoJson
#include <DHT.h>
#include <MQTTClient.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "192.168.1.105";
char mqtt_port[6] = "1883";


//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

MQTTClient mqttClient;
WiFiClient netClient;

bool debug = true;

//Constants and Variables
DHT dht(D4, DHT22); 
int pinSensor = A0; //Pino Sensor
int Rele = D3; //Pino Relé
int buzzer = D2; //Pino Buzzer
int led = D1;
//int var = 0;
char ValDesarm = 30; //Percent of gas or smoke that will be considered by the sensor  //define your default values here, if there are different values in config.json, they are overwritten.
int value = 0;
  
void setup() {
 Serial.begin(115200);

 randomSeed(analogRead(0));
 
 pinMode(pinSensor, INPUT);
 pinMode(Rele, OUTPUT);
 pinMode(buzzer, OUTPUT);
 pinMode(led, OUTPUT);
 
  //clean FS, for testing
   //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

    if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
   //       strcpy(ValDesarm, json["valDesarm"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
 // WiFiManagerParameter custom_val_desarm("ValDesarm", "ValDesarm", ValDesarm, 4);
  
  ESP8266WebServer server(80);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager; //default custom static IP

  //reset saved settings
  //wifiManager.resetSettings();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

 //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
 //wifiManager.addParameter(&custom_val_desarm);

  //for some reason that I can't understood why at my home, I only have sucess to connect to wifi AP using the code below :(
  if(!wifiManager.autoConnect("gas_sensor")){
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  
  Serial.println("local ip:");
  Serial.println(WiFi.localIP());

  if (debug) {
    Serial.println("");
    Serial.print("Connected to:"); 
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");    
  }

  //read updated parameters (if applicable)
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  //strcpy(ValDesarm, custom_val_desarm.getValue());


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
   // json["ValDesarm"] = ValDesarm;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    } else {
      json.printTo(Serial);
      json.printTo(configFile);
    }
    configFile.close();
  }

  dht.begin();

    Serial.print("MQTT Server:"); 
    Serial.println(mqtt_server);
    Serial.print("MQTT Port:");
    Serial.println(mqtt_port);
    Serial.println("Local ip: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway:");
    Serial.println(WiFi.gatewayIP());
    Serial.print("subnetMask:");
    Serial.println(WiFi.subnetMask());
    Serial.print("Value Alarm:");
    Serial.println(ValDesarm);

    mqttClient.begin(mqtt_server, netClient);
    Serial.print("\On the Setup()! connecting to MQTT...");
    while (!mqttClient.connect("gas_sensor")) {
      Serial.print(".");
      delay(1000);
    }
}
  
void loop(){
  mqttClient.loop();
  if (!mqttClient.connected()) {
    mqttClient.connect("gas_sensor");
    Serial.print("\n On the loop()! connecting to MQTT...");
    while (!mqttClient.connect("gas_sensor")) {
      Serial.print(".");
      delay(1000);
    }
  } 
  
  GasSensor(analogRead(pinSensor));
  Thermometer();
  delay(1000);
}

// Function to measure the temperature and humidity
void Thermometer() {
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  send_data("home/kitchen/humidity", h);
  send_data("home/kitchen/temperature", t);
  send_data("home/kitchen/heat_index", hic);
}


void GasSensor(long AnalogPinSensorValue) {
  value = analogRead(AnalogPinSensorValue); //Read value of the sensor at the moment.
  value = map(value, 0, 1023, 0, 100); //convert value to percentage
  Serial.print("Value: ");
  Serial.println(value); // write the value on serial
  if(value<=ValDesarm){
     //digitalWrite(buzzer, LOW); //TURN OFF the buzzer
     digitalWrite(led, LOW); // TURN OFF the Led
     digitalWrite(Rele, LOW); // TURN OFF the rele/solenoide.
     send_data("home/kitchen/gas_smoke", "false");
     send_data("home/kitchen/gas_percent", ValDesarm);
     Serial.println("Vazamento de Gas ou Fumaca nao detectado.");
     delay(1000); // Time....
  }else{
     digitalWrite(Rele, HIGH); // Turn ON the rele/solenoide
//     digitalWrite(buzzer, HIGH); 
     tone(buzzer, random(500, 2500), 5000); //Buzzzz ON tho show that have a situation!!!!
     digitalWrite(led, HIGH); // Turn ON Red Led... we are in a situation!!!
     send_data("home/kitchen/gas_smoke", "true"); // Send data to MQTT
     send_data("home/kitchen/gas_percent", ValDesarm);
     Serial.println("Alarm ON!!!"); // Write messages in the serial port
     Serial.println("Rele/Solenoide TURN OFF GAS!!!");
     Serial.println("Led - GAS or SMOKE detected in the room!!");
     delay(1000); //Time....
  }
}
 
void send_data(String topic, long data) {
  send_data(topic, String(data));
}

void send_data(String topic, String data) {
  Serial.print(topic);
  Serial.print(":");
  Serial.println(data);
  mqttClient.publish(topic, data);
}
