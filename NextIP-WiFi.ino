#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "ACS712.h"

#define D1    5
#define D2    4
#define D3    0
#define D4    2
#define D5    14
#define D7    13

#define DHTTYPE DHT22
const char* ssid = "PI";
const char* password = "ezzahiRobot";
const char* mqtt_server = "192.168.43.131";

WiFiClient espClient;
PubSubClient client(espClient);

const int DHTPin = D7;
DHT dht(DHTPin, DHTTYPE);
long now = millis();
long lastMeasure = 0;

ACS712 sensor(ACS712_05B, A0);

//--------------------------------------
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  if(topic=="nextIP"){
      Serial.print("Changing Room lamp to ");
      if(messageTemp == "Lon"){
        digitalWrite(D1, LOW);
        Serial.print("Lighting is On");
      }
      else if(messageTemp == "Loff"){
        digitalWrite(D1, HIGH);
        Serial.print("Lighting is Off");
      }
      /* if((messageTemp == "Hon") ||(messageTemp == "Con")){
        digitalWrite(D2, HIGH);
        Serial.print("Heating/Climatisation is On");
      } 
      else if((messageTemp == "Hoff") ||(messageTemp == "Coff")){
        digitalWrite(D2, LOW);
        Serial.print("Heating/Climatisation is Off");
      }*/
      if(messageTemp == "Hon"){
        digitalWrite(D2, LOW);
        Serial.print("Heating is On");
      }
      else if(messageTemp == "Hoff"){
        digitalWrite(D2, HIGH);
        Serial.print("Heating is Off");
      }
       if(messageTemp == "Pon"){
        digitalWrite(D3, LOW);
        digitalWrite(D4, HIGH);
        digitalWrite(D5, LOW);
        Serial.print("Prise is On");
      } 
      else if(messageTemp == "Poff"){
        digitalWrite(D3, HIGH);
        digitalWrite(D4, LOW);
        digitalWrite(D5, HIGH);
        Serial.print("Prise is Off");
      }
      //--------- battery control ---------------
        if(messageTemp == "off"){
        digitalWrite(D1, HIGH);
        digitalWrite(D2, HIGH);
      } 

  
  Serial.println();
}}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");  
      client.subscribe("nextIP");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//--------------------------------------

void setup() {
  Serial.begin(9600);
  Serial.println("Calibrating... Ensure that no current flows through the sensor at this moment");
  sensor.calibrate();
  Serial.println("Done!");
   pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  digitalWrite(D1, LOW); 
  digitalWrite(D2, LOW); 
  digitalWrite(D3, LOW); 
  digitalWrite(D4, LOW); 
  digitalWrite(D5, HIGH); 

  dht.begin();  
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  if(!client.loop())
    client.connect("ESP8266Client");
  //*****************************
    
  now = millis();
  if (now - lastMeasure > 2000) {
    lastMeasure = now;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float f = dht.readTemperature(true);

    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    float hic = dht.computeHeatIndex(t, h, false);
    static char temperatureTemp[7];
    dtostrf(hic, 6, 2, temperatureTemp);
    
    static char humidityTemp[7];
    dtostrf(h, 6, 2, humidityTemp);

    client.publish("nextIP/temperature", temperatureTemp);
    client.publish("nextIP/humidity", humidityTemp);
    
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t Temperature: ");
    Serial.print(t);
    Serial.print(" *C ");
    Serial.print(f);
    Serial.print(" *F\t Heat index: ");
    Serial.print(hic);
    Serial.println(" *C ");
  }
  //****************************
  
  float U = 230;
  
  float I = sensor.getCurrentAC();
  float P = U * I;

  Serial.println(String("I = ") + I + " A");
  Serial.println(String("P = ") + P + " Watts");
  
  float I_AC;
  float P_AC;

  if(I <=0.08){
    
    I_AC = 0;
    P_AC = 0;
    
  }else {
      I_AC = I - 0.08;
      P_AC = P - 17.6;
  }

  //----- La protection des équipements qui y sont connectés à la prise NextIP---

  if(I >= 3){
    //éteindre la prise
    digitalWrite(D3, HIGH);
    }

 // Caclcul de l'énergie pendant toute la journée
 
  float E = P_AC * I_AC * 24;
  
  Serial.println(String("E = ") + E + " Wh");


  static char courant[7];
  dtostrf(I_AC, 6, 2, courant);

  static char puissance[7];
  dtostrf(P_AC, 6, 2, puissance);

  static char energie[7];
  dtostrf(E, 6, 2, energie);

  client.publish("nextIP/current", courant);
  client.publish("nextIP/power", puissance);
  client.publish("nextIP/energie", energie);

  delay(1000);
}
