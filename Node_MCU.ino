#include "FS.h"
#define pulsePin A0
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

TinyGPSPlus gps;  // The TinyGPS++ object

SoftwareSerial ss(4, 5); // The serial connection to the GPS device

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const char* ssid = "Enter your Wifi"; 
const char* password = "Enter your password";

const char* AWS_endpoint = "Enter your AWS_endpoint";

const int postingInterval = 10 * 1000; 

int rate[10];                    
unsigned long sampleCounter = 0; 
unsigned long lastBeatTime = 0;  
unsigned long lastTime = 0, N;
int BPM = 0;
int IBI = 0;
int P = 512;
int T = 512;
int thresh = 512;  
int amp = 100;                   
int Signal;
boolean Pulse = false;
boolean firstBeat = true;          
boolean secondBeat = true;
boolean QS = false;   
float latitude , longitude;
int year , month , date, hour , minute , second;
String date_str , time_str , lat_str , lng_str;
int pm; 


void callback(char* topic, byte* payload, unsigned int length) {
Serial.print("Message arrived [");
Serial.print(topic);
Serial.print("] ");
for (int i = 0; i < length; i++) {
Serial.print((char)payload[i]);
}
Serial.println();

}
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient); 
long lastMsg = 0;
char msg[50];
int value = 0;

void setup_wifi() {

delay(10);
// We start by connecting to a WiFi network
espClient.setBufferSizes(512, 512);
Serial.println();
Serial.print("Connecting to ");
Serial.println(ssid);

WiFi.begin(ssid, password);

while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}

Serial.println("");
Serial.println("WiFi connected");
Serial.println("IP address: ");
Serial.println(WiFi.localIP());

timeClient.begin();
while(!timeClient.update()){
timeClient.forceUpdate();
}

espClient.setX509Time(timeClient.getEpochTime());

}

void reconnect() {
// Loop until we're reconnected
while (!client.connected()) {
Serial.print("Attempting MQTT connection..");
// Attempt to connect
if (client.connect("ESPthing")) {
Serial.println("connected");

client.publish("outTopic", "Heart Rate");

client.subscribe("inTopic");
} else {
Serial.print("failed, rc=");
Serial.print(client.state());
Serial.println(" try again in 5 seconds");

char buf[256];
espClient.getLastSSLError(buf,256);
Serial.print("WiFiClientSecure SSL error: ");
Serial.println(buf);

// Wait 5 seconds before retrying
delay(5000);
}
}
}

void setup() {
  Serial.begin(9600);
  ss.begin(9600);
  
  setup_wifi();
delay(1000);
if (!SPIFFS.begin()) {
Serial.println("Failed to mount file system");
return;
}
Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

File cert = SPIFFS.open("/cert.der", "r"); //replace cert.crt eith your uploaded file name
if (!cert) {
Serial.println("Failed to open cert file");
}
else
Serial.println("Success to open cert file");

delay(1000);

if (espClient.loadCertificate(cert))
Serial.println("cert loaded");
else
Serial.println("cert not loaded");

// Load private key file
File private_key = SPIFFS.open("/private.der", "r"); //replace private eith your uploaded file name
if (!private_key) {
Serial.println("Failed to open private cert file");
}
else
Serial.println("Success to open private cert file");

delay(1000);

if (espClient.loadPrivateKey(private_key))
Serial.println("private key loaded");
else
Serial.println("private key not loaded");

// Load CA file
File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
if (!ca) {
Serial.println("Failed to open ca ");
}
else
Serial.println("Success to open ca");

delay(1000);

if(espClient.loadCACert(ca))
Serial.println("ca loaded");
else
Serial.println("ca failed");

Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

}

void loop() {
 
  if (!client.connected()) {
  reconnect();
  }
  client.loop();
  
  if (QS == true) {
   Serial.println("BPM: "+ String(BPM));
   QS = false;
   } else if (millis() >= (lastTime + 2)) {
     readPulse(); 
     lastTime = millis();
   } 
  
  if(int(BPM)>=90)
  {
    Serial.println("Heart Rate");
    delay(100);
  }
  else
  {
    while (ss.available() > 0)
      if (gps.encode(ss.read()))
      {
        if (gps.location.isValid())
        {
          latitude = gps.location.lat();
          lat_str = String(latitude , 6);
          longitude = gps.location.lng();
          lng_str = String(longitude , 6);
        }

        if (gps.date.isValid())
        {
          date_str = "";
          date = gps.date.day();
          month = gps.date.month();
          year = gps.date.year();

          if (date < 10)
            date_str = '0';
          date_str += String(date);

          date_str += " / ";

          if (month < 10)
            date_str += '0';
          date_str += String(month);

          date_str += " / ";

          if (year < 10)
            date_str += '0';
          date_str += String(year);
      }

      if (gps.time.isValid())
      {
        time_str = "";
        hour = gps.time.hour();
        minute = gps.time.minute();
        second = gps.time.second();

        minute = (minute + 30);
        if (minute > 59)
        {
          minute = minute - 60;
          hour = hour + 1;
        }
        hour = (hour + 5) ;
        if (hour > 23)
          hour = hour - 24;

        if (hour >= 12)
          pm = 1;
        else
          pm = 0;

        hour = hour % 12;

        if (hour < 10)
          time_str = '0';
        time_str += String(hour);

        time_str += " : ";

        if (minute < 10)
          time_str += '0';
        time_str += String(minute);

        time_str += " : ";

        if (second < 10)
          time_str += '0';
        time_str += String(second);

        if (pm == 1)
          time_str += " PM ";
        else
          time_str += " AM ";

      }

    }

  delay(100);
   }
}



void readPulse() {

  Signal = analogRead(pulsePin);              
  sampleCounter += 2;                           
  int N = sampleCounter - lastBeatTime;   

  detectSetHighLow();

  if (N > 250) {  
    if ( (Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3) )
      pulseDetected();
  }

  if (Signal < thresh && Pulse == true) {  
    Pulse = false;
    amp = P - T;
    thresh = amp / 2 + T;  
    P = thresh;
    T = thresh;
  }

  if (N > 2500) {
    thresh = 512;
    P = 512;
    T = 512;
    lastBeatTime = sampleCounter;
    firstBeat = true;            
    secondBeat = true;           
  }

}

void detectSetHighLow() {

  if (Signal < thresh && N > (IBI / 5) * 3) {
    if (Signal < T) {                       
      T = Signal;                         
    }
  }

  if (Signal > thresh && Signal > P) {    
    P = Signal;                           
  }                                       

}

void pulseDetected() {
  Pulse = true;                           
  IBI = sampleCounter - lastBeatTime;     
  lastBeatTime = sampleCounter;           

  if (firstBeat) {                       
    firstBeat = false;                 
    return;                            
  }
  if (secondBeat) {                    
    secondBeat = false;                
    for (int i = 0; i <= 9; i++) {   
      rate[i] = IBI;
    }
  }

  word runningTotal = 0;                   

  for (int i = 0; i <= 8; i++) {          
    rate[i] = rate[i + 1];            
    runningTotal += rate[i];          
  }

  rate[9] = IBI;                      
  runningTotal += rate[9];            
  runningTotal /= 10;                 
  BPM = 60000 / runningTotal;         
  QS = true;
  
  long now = millis();
  if (now - lastMsg > 2000) {
  lastMsg = now;
  snprintf (msg, 75, "{\"message\": \"Heart Rate #%ld\"; \"latitude #%f\"; \"longitude #%f\"}", BPM, lat_str.toFloat(), lng_str.toFloat());
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("outTopic", msg);
  //client.publish("outTopic", l_at);
  //client.publish("outTopic", l_ng);
  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap()); //Low heap can cause problems
  }
  //delay(postingInterval);                              
}
