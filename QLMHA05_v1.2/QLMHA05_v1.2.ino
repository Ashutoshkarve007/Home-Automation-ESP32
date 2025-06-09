
/*
 changes :-
 Removed web-ui
This code is currently used by Application developers
 */
 
/* Headers  */
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
//#include <HTTPClient.h>
#include <ESP32httpUpdate.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP32Ping.h>
#include <RBDdimmer.h>
#include "RTClib.h" 
#include "certificates.h"
#include "declarations.h"

  /*      Instance creations   */
WebServer server(80);       // server for WiFi config     
WiFiServer Server(80);         // server for local mode
WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);
RTC_DS3231 rtc;
dimmerLamp dimmer(dimmerPin, zcPin);

/******************************************* initial config after device boot successfully *******************************/
void setup() {
  
  Serial.println("+++++++++++++++++++Welcome To Qurolabs+++++++++++++++++");
  delay(1000);
Serial.begin(115200);           //Initialising debug port 
      
/*    relay outputs   */
pinMode(device1, OUTPUT);
pinMode(device2, OUTPUT);
pinMode(device3, OUTPUT);
pinMode(device4, OUTPUT);
pinMode(device5, OUTPUT);
pinMode(testPin,OUTPUT);

digitalWrite(device1,LOW);
digitalWrite(device2,LOW);
digitalWrite(device3,LOW);
digitalWrite(device4,LOW);
digitalWrite(device5,LOW);
digitalWrite(testPin,HIGH);
/*    status led's   */
pinMode(internetStatusLed,OUTPUT);    // Blue led : Status for Internet mode - LED2
pinMode(wifiStatusLed,OUTPUT);        // Orange led : Statud for wifi-Hotspot mode - LED1
pinMode(LED_BUILTIN, OUTPUT);
digitalWrite(LED_BUILTIN,LOW);
digitalWrite(internetStatusLed,LOW);
digitalWrite(wifiStatusLed,LOW);

/*  manual switches   */
pinMode(switchPin1,INPUT_PULLDOWN);
pinMode(switchPin2,INPUT_PULLDOWN);
pinMode(switchPin3,INPUT_PULLDOWN);
pinMode(switchPin4,INPUT_PULLDOWN);
pinMode(switchPin5,INPUT_PULLDOWN);

pinMode(bootPin, INPUT_PULLUP); 

  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(EEPROM_SIZE);       //Initialasing EEPROM
  delay(1000);
    dimmer.begin(NORMAL_MODE, ON);
    dimmer.setState(OFF);

//  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");

//  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  WiFi.begin(esid.c_str(), epass.c_str());
 int retry = 0;
 while ((WiFi.status() != WL_CONNECTED)  && retry < 15)
  {
    Serial.print("Connecting to : ");
    Serial.println(esid.c_str());
    digitalWrite(internetStatusLed,HIGH);
      delay(300);
      digitalWrite(internetStatusLed,LOW);
      delay(300);
      retry++;
   }
                 if(WiFi.status() != WL_CONNECTED)
                  {
                      Serial.println("Wifi setup mode");                      
                      initialise_rtc();
                  }       
                  else
                  {
                     Serial.println("WIFI MODE");
                     Serial.println();
                     Serial.print("Connected to : ");
                     Serial.println(WiFi.SSID());                           
                     Serial.print("Connected with IP: ");
                     Serial.println(WiFi.localIP());      
                     // Configuring MQTT Client
                      httpsClient.setCACert(rootCA);
                      httpsClient.setCertificate(certificate);
                      httpsClient.setPrivateKey(privateKey);
                      mqttClient.setServer(endpoint, port);
                      mqttClient.setCallback(mqttCallback);
                    connectToServer();
                    initialise_rtc();
                    }
               xTaskCreate(keepWiFiAlive," keepWiFiAlive",10000,NULL,1,NULL);    
}

/***************************************************repeated loop******************************************/
void loop() { 

  if(Ping.ping("www.google.com"))
          {
               if(netflag == false)
                  {
                      WiFi.mode(WIFI_STA);           //Internet MODE
                              digitalWrite(wifiStatusLed,LOW);                // Orange led off for wifi-hotspot mode 
                              digitalWrite(internetStatusLed, HIGH);        // Blue led stable for internet mode
                        Serial.println("Switched to Internet mode");
                           delay(1000);                                                  
                             netflag = true;  
                  }
                        if(netflag)
                             {
                                  while(1)
                                  {
                                      scheduleTask();
                                      mqttClient.loop();                                                                                 
                                         if(!mqttClient.connected())
                                            {
                                                Serial.println("Internet Connection Lost");
                                                break;          
                                            }
                                  }
                             }
                  blockflag = false;
          }else
          {
                if(blockflag == false)
                   {
                              digitalWrite(wifiStatusLed,HIGH);                // Orange led stable for wifi-hotspot mode
                              digitalWrite(internetStatusLed, LOW);        // Blue led off for internet mode 
                       Serial.println("Switched to Access point i.e HotSpot mode");
                         WiFi.mode(WIFI_AP_STA);
                           Serial.println("You can Acess Local Server to Control Appliances.........");
                             WiFi.softAP(ThingName, "123456789");               
                               Server.begin();
                                Serial.println("localmode Started.....");
                                  blockflag = true;
                    }
                    if(blockflag == true)
                      {
                           while(1)
                            {                           
                                without_internet();
                                scheduleTask();                      
                                if(mqttClient.connected())
                                {
                                  Serial.println("Internet Connected Successfully");
                                      break;                                  
                                }                                  
                            }    
                     }                     
                     netflag =false;
          }
}

/*************************************************** user button functionality ********************************************/
void readButtonState() 
  {     
   if(currentMillis - previousButtonMillis > intervalButton) 
    {  
        int buttonState = !digitalRead(bootPin);    
          if (buttonState == HIGH && buttonStatePrevious == LOW && !buttonStateLongPress) 
              {
                   buttonLongPressMillis = currentMillis;
                   buttonStatePrevious = HIGH;
                   Serial.println("Button pressed");
              }
                buttonPressDuration = currentMillis - buttonLongPressMillis;
    if (buttonState == HIGH && !buttonStateLongPress && buttonPressDuration >= minButtonLongPressDuration) 
    {
        buttonStateLongPress = true;
        Serial.println("Button long pressed");
        Server.stop();
        delay(1000);
        OpenWeb();
        setupAP();
              digitalWrite(internetStatusLed,HIGH);
              digitalWrite(wifiStatusLed,HIGH);
        while(1)
          {
            server.handleClient();
            Serial.println(".");
            delay(100);
          }
    }
    if (buttonState == LOW && buttonStatePrevious == HIGH) {
      buttonStatePrevious = LOW;
      buttonStateLongPress = false;
      Serial.println("Button released");

      if (buttonPressDuration < minButtonLongPressDuration) {
        Serial.println("Button pressed shortly");
          flash_erase();
          ESP.restart();
    }   
    // store the current timestamp in previousButtonMillis
    previousButtonMillis = currentMillis;

   }
  }
}

/*************************************************** functionality for accesing wifi credentials from a local wifi server  ********************************************/
bool checkWifi(void)         // check wifi connection
{
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 10 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(1000);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}
void OpenWeb()
{
  
   Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
 
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  
    WiFi.mode(WIFI_AP);
  WiFi.disconnect();
  OpenWebServer();
  server.begin();           // Start the server
  Serial.println("Server started");
}
void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);

    st += ")";
    st += "</li>";
  } 
  st += "</ol>";
  delay(100);
  WiFi.softAP("WiFiConfig", "123456789");
  OpenWeb();
}
void OpenWebServer()
{
 {
    server.on("/", []() {

      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Welcome To QuroLabs ";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });

    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");

        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        }
        EEPROM.commit();

        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.restart();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);

    });
  } 
}

/******************************************** RTC initialisation ***********************************/
void initialise_rtc()
{
           if(!rtc.begin())  
              {
                    Serial.println("Couldn't find RTC");
              } 
           if(rtc.lostPower()) 
              {
                    Serial.println("RTC lost power, lets set the time!");
                    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
                    //     rtc.adjust(DateTime(2021, 2, 25, 11, 14, 0));
              }
}

/******************************************* remote server functionalities *************************************/
void connectToServer() {
    if (!mqttClient.connected()) {                  
        if (mqttClient.connect(ThingName)) {
            Serial.println("Connected To AWS");
            int qos = 0;                              //Make sure the qos is zero in the MQTT broker of AWS
            mqttClient.subscribe(TopicName, qos);
            Serial.println("Subscribed.");
            mqttClient.setCallback(mqttCallback);
        } else {

        }
    }
}
void mqttLoop() {
    if (!mqttClient.connected()) {
        connectToServer();
        vTaskDelay(10000 / portTICK_RATE_MS);
    }else
      {
          mqttClient.loop();    
      }
}
void mqttCallback (char* topic, byte *payload, unsigned int length) {
    strncpy(rcvdPayload,(char *)payload,length);
    Serial.print("\n");
    Serial.println(rcvdPayload);
    with_internet();    
 // sendDeviceState();
}

/****************************************** functionality when device is connected to Server **********************************************/
void with_internet()
{
     StaticJsonDocument<256> doc;
     DeserializationError error = deserializeJson(doc,rcvdPayload);          
      if(error)
          return;
          Serial.print("with_internet");
  char myData[8] = {
                        doc["device1"],doc["device2"],doc["device3"],doc["device4"],doc["device5"],doc["deviceId"],doc["sliderValue"],doc["firmwareUpdate"]     // device1  && device2 && device3 && device4 && device5
                   };
  char schedule[11] =  {  
                          doc["date"],doc["month"],doc["hour"],doc["minutes"],doc["sch1"],doc["sch2"],doc["sch3"],doc["sch4"],doc["sch5"],doc["duration"],doc["allOff"]                  
                       };      
    
/* Control the applicances */
      if(myData[0])  {
           device1State = "ON";
           digitalWrite(device1,HIGH);
            switch1PreviousState = LOW;
      }           
      else if(!myData[0]){ 
           device1State = "OFF";
           digitalWrite(device1,LOW);
           switch1PreviousState = HIGH;
      }   
      
      if(myData[1])  { 
           device2State = "ON"; 
           digitalWrite(device2,HIGH);
            switch2StatePrevious = LOW;
      }            
      else if (!myData[1]){
           device2State = "OFF";  
           digitalWrite(device2,LOW);
           switch2StatePrevious = HIGH;          
      }      
      
      if(myData[2])  {
           device3State = "ON";
           digitalWrite(device3,HIGH);
           switch3StatePrevious = LOW;
      }                    
      else if(!myData[2]){
           device3State = "OFF";
           digitalWrite(device3,LOW);
           switch3StatePrevious = HIGH;
      }
      
      if(myData[3])  {
            device4State = "ON";             
           digitalWrite(device4,HIGH);
           switch4StatePrevious = LOW;
      }     
      else if (!myData[3]){
            device4State = "OFF";
           digitalWrite(device4,LOW);
            switch4StatePrevious = HIGH;
      }       
                       
      if(myData[4])  {          
            device5State = "ON";
           digitalWrite(device5,HIGH);
           switch5StatePrevious = LOW;
      }      
      else if(!myData[4]) {
             device5State = "OFF";
           digitalWrite(device5,LOW);
           switch5StatePrevious = HIGH;
      } 
      if(myData[6] != 0)
      {
          buf = myData[6];
          dimmerOn();
      }

      if(myData[7])
      {
          firmwareUpdate();
      }
    if(schedule[0] != 0){  
                  EEPROM.write(100,schedule[0]);      //  date -> 100
                  EEPROM.commit();
                  Serial.print("Date Received : ");
                  Serial.println(byte(EEPROM.read(100)));  
                  }
                  if(schedule[1] != 0){
                  EEPROM.write(101,schedule[1]);        // month -> 101
                  EEPROM.commit();  
                  Serial.println("Month Received : ");
                  Serial.println(byte(EEPROM.read(101)));
                  }
                  if(schedule[2] != 0){
                  EEPROM.write(102,schedule[2]);        // hour -> 102
                  EEPROM.commit();  
                  Serial.print("Hours Received : ");
                  Serial.println(byte(EEPROM.read(102)));
                  }
                  if(schedule[3] != 0){
                  EEPROM.write(103,schedule[3]);        // minutes -> 103
                  EEPROM.commit();  
                  Serial.print("Minutes Received : ");
                  Serial.println(byte(EEPROM.read(103)));
                  }
                  
                  if(schedule[4] != 0){
                  EEPROM.write(104,schedule[4]);        // schedule1 -> 104
                  EEPROM.commit();  
                  Serial.print("Schedule-1 Trigger : ");
                  Serial.println(byte(EEPROM.read(104)));
                  }
                  
                  if(schedule[5] != 0){
                  EEPROM.write(105,schedule[5]);        // schedule2 -> 105
                  EEPROM.commit();  
                  Serial.print("Schedule-2 Trigger : ");
                  Serial.println(byte(EEPROM.read(105)));
                  }

                  if(schedule[6] != 0){
                  EEPROM.write(106,schedule[6]);        // schedule3 -> 106
                  EEPROM.commit();  
                  Serial.print("Schedule-3 Trigger : ");
                  Serial.println(byte(EEPROM.read(106)));                    
                  }

                  if(schedule[7] != 0){
                  EEPROM.write(107,schedule[7]);        // schedule4 -> 107
                  EEPROM.commit();  
                  Serial.print("Schedule-4 Trigger : ");
                  Serial.println(byte(EEPROM.read(107)));                    
                  }
                  
                  if(schedule[8] != 0){
                  EEPROM.write(108,schedule[8]);        // schedule5 -> 108
                  EEPROM.commit();  
                  Serial.print("Schedule-5 Trigger : ");
                  Serial.println(byte(EEPROM.read(108)));                                        
                  }

                  if(schedule[9] != 0){
                    EEPROM.write(109,schedule[9]);        // address of duration -> 109
                    EEPROM.commit();
                    Serial.print("Minutes Of Schedule : ");
                    Serial.println(byte(EEPROM.read(109)));                                        
                  }

                if(schedule[10])                                  //  Trigger to Off all devices
                   {
                         digitalWrite(device1,LOW);
                         digitalWrite(device2,LOW);              
                         digitalWrite(device3,LOW);
                         digitalWrite(device4,LOW);
                         digitalWrite(device5,LOW);
                   }     
             sendDeviceState();
/*
                              String relay_state =  "{";
                                  relay_state += "device1:";
                                  relay_state += device1State;
                                      relay_state += ", ";
                                          relay_state += "device2:";
                                              relay_state += device2State;
                                                  relay_state += ", ";
                                                       relay_state += "device3:";
                                                            relay_state += device3State;
                                                                 relay_state += ", ";
                                                            relay_state += "device4:";
                                                        relay_state += device4State;
                                                      relay_state += ", ";                 
                                                  relay_state += "device5:";
                                              relay_state += device5State;
                                           relay_state += "}";
                                      
                                char response[150];
                                relay_state.toCharArray(response,sizeof(response));
                    mqttClient.publish(TopicName, response,sizeof(response));
                    
                     sendDeviceState();
                     
*/
/* memset(rcvdPayload,0,sizeof(rcvdPayload));
 memset(schedule,0,sizeof(schedule)); */
}
void sendDeviceState()
{    
 StaticJsonDocument<256> jsonDoc;
   JsonObject stateObj = jsonDoc.createNestedObject("state");
  JsonObject reportedObj = stateObj.createNestedObject("reported");
  
  reportedObj["deviceId"] = ThingName;
  reportedObj["device1"] = digitalRead(device1);
  reportedObj["device2"] = digitalRead(device2);
  reportedObj["device3"] = digitalRead(device3);
  reportedObj["device4"] = digitalRead(device4);
  reportedObj["device5"] = digitalRead(device5);
  reportedObj["deviceStatus"] = "Online";
//  serializeJson(jsonDoc, Serial);
  char Buffer[140];
  serializeJson(reportedObj, Buffer,sizeof(Buffer));
  
  Serial.println("Publishing message to AWS...");
  mqttClient.publish(TopicName, Buffer,sizeof(Buffer));
 // vTaskDelay(2000 / portTICK_RATE_MS);      // publish the state every 10 minutes
}

/******************************************************* schedule devices ************************************/
void scheduleTask()
{
//  manualSwitch();  
    DateTime now = rtc.now();
   
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    delay(300);
    
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-1 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102) && now.minute() == EEPROM.read(103) && EEPROM.read(104) && EEPROM.read(109) != 0 )      // device1 on    
         {
            Serial.println("Schedule-1 Successfully played......");
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device1,HIGH);
              delay(sec);                   
             digitalWrite(device1,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-2 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102) && now.minute() == EEPROM.read(103) && EEPROM.read(105) && EEPROM.read(109) != 0 )      // device1 on    
         {
            Serial.println("Schedule-2 Successfully played......");
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device2,HIGH);
              delay(sec);                     
             digitalWrite(device2,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-3 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102) && now.minute() == EEPROM.read(103) && EEPROM.read(106) && EEPROM.read(109) != 0 )      // device1 on    
         {
            Serial.println("Schedule-3 Successfully played......");
            long unsigned sec = EEPROM.read(109)*60000;
              Serial.println("I am In Schedule 3");
             digitalWrite(device3,HIGH);
              delay(sec);
             digitalWrite(device3,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-4 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102) && now.minute() == EEPROM.read(103) && EEPROM.read(107) && EEPROM.read(109) != 0 )      // relay1 on    
         {
            Serial.println("Schedule-4 Successfully played......");
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device4,HIGH);
              delay(sec);
             digitalWrite(device4,LOW);
              schedule_erase();
         }
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-5 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102)&& now.minute() == EEPROM.read(103) && EEPROM.read(108) && EEPROM.read(109) != 0 )      // relay1 on    
         {
            Serial.println("Schedule-5 Successfully playing......");
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device5,HIGH);
              delay(sec);
             digitalWrite(device5,LOW);
              schedule_erase();
         }          
         connectToServer();  
}
void schedule_erase()
{
    for(int i = 100; i < 110; i++)
    {
      EEPROM.write(i,0);
    }
       EEPROM.commit();
        delay(500);
}

/**************************************** functionality when internet connection lost **********************************************/
void without_internet()
{

    WiFiClient client = Server.available(); // Listen for incoming clients
manualSwitch();
if (client)
{ // If a new client connects,  
String header = client.readStringUntil('\r');
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
           
            // turns the GPIOs on and off
            if (header.indexOf("GET /device1On") >= 0) {
              Serial.println("device1 ON");
              device1State = "ON";
              digitalWrite(device1, HIGH);
            } else if (header.indexOf("GET /device1Off") >= 0) {
              Serial.println("device1 OFF");
              device1State = "OFF";
              digitalWrite(device1, LOW);
            } else if (header.indexOf("GET /device2On") >= 0) {
              Serial.println("device2 ON");
              device2State = "ON";
              digitalWrite(device2, HIGH);
            } else if (header.indexOf("GET /device2Off") >= 0) {
              Serial.println("device2 OFF");
              device2State = "OFF";
              digitalWrite(device2, LOW);
            } else if (header.indexOf("GET /device3On") >= 0) {
              Serial.println("device3 ON");
              device3State = "ON";
              digitalWrite(device3, HIGH);
            } else if (header.indexOf("GET /device3Off") >= 0) {
              Serial.println("device3 OFF");
              device3State = "OFF";
              digitalWrite(device3, LOW);
            } else if (header.indexOf("GET /device4On") >= 0) {
              Serial.println("device4 ON");
              device4State = "ON";
              digitalWrite(device4, HIGH);
            } else if (header.indexOf("GET /device4Off") >= 0) {
              Serial.println("device4 OFF");
              device4State = "OFF";
              digitalWrite(device4, LOW);
            } else if (header.indexOf("GET /device5On") >= 0) {
              Serial.println("device5 ON");
              device5State = "ON";
              digitalWrite(device5, HIGH);
            } else if (header.indexOf("GET /device5Off") >= 0) {
              Serial.println("device5 OFF");
              device5State = "OFF";
              digitalWrite(device5, LOW);
            }     
            client.println("<!DOCTYPE HTML><html>");
/*            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; background-color: #4CAF50;");
            client.println("border: none; color: white; padding: 12px 28px; text-decoration: none; font-size: 26px; margin: 1px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page        
           // client.println("<p><a href=\"/forward\"><button class=\"button\" onclick=\"moveForward()\">Welcome To CoCa</button></a></p>");
            client.println("</head><body><h1>Welcome To COCA</h1>");
            client.println("<p>fanSlider <span id=\"\"></span></p>");
          /*  client.println("<p>SliderValue <span id=\"motorSlider\"></span></p>");
             client.println("<p>Speed: <span id=\"motorspeedpos\"></span></p>");
           
           client.println("<input type=\"range\" min=\"0\" max=\"100\" step=\"1\" id=\"motorSlider\" onchange=\"motorSpeed(this.value)\" value=\"" + valueString + "\"/>");
            
            client.println("<script> function motorSpeed(pos) { ");
            client.println("var xhr = new XMLHttpRequest();"); 
            client.println("xhr.open('GET', \"/?value=\" + pos + \"&\", true);"); 
            client.println("xhr.send(); } </script>");

        client.println("<p>device1 - " + device1State + "</p>");
                              
        if (device1State == "OFF") {
              client.println("<p><a href=\"/device1On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device1Off\"><button class=\"button button2\">OFF</button></a></p>");
            }  
           
            client.println("<p>device2 - " + device2State + "</p>");

            if (device2State == "OFF") {
              client.println("<p><a href=\"/device2On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device2Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device3 - " + device3State + "</p>");
            
            if (device3State == "OFF") {
              client.println("<p><a href=\"/device3On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device3Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device4 - " + device4State + "</p>");

            if (device4State == "OFF") {
              client.println("<p><a href=\"/device4On\"><button class=\"button\">ON</button></a></p>");
            } else {
             client.println("<p><a href=\"/device4Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device5 - " + device5State + "</p>");

            if (device5State == "OFF") {
              client.println("<p><a href=\"/device5On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device5Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
        */       
                String response = "{";
               response += "device1:";
               response += device1State;
               response += ",";
               response += "device2:";
               response += device2State;
               response += ",";
               response += "device3:";
               response += device3State;
               response += ",";
               response += "device4:";
               response += device4State;
               response += ",";
               response += "device5:";
               response += device5State;
//               response += ",";
  //             response += "Status";
    //           response += "OFFLINE";
               response += "}";
 char PayLoad[65];
 response.toCharArray(PayLoad,sizeof(PayLoad));              
client.print(PayLoad);
           client.println("</html>"); 
           
           if(header.indexOf("GET /?value=")>=0) 
            {
              positon1 = header.indexOf('=');
              positon2 = header.indexOf('&');
              valueString = header.substring(positon1+1, positon2);
              buf = valueString.toInt();
              Serial.println(buf);
              dimmerOn(); 
             } 

  header = "";
      manualSwitch();

client.stop();
Serial.println("Client disconnected.");
Serial.println("");
  }
}

 /********************************************* AC dimmer functionality *******************************/
void dimmerOn()
{
  int preVal = outVal;
    if(buf != 0 || buf == 0)
      {
        outVal = buf;
            if(outVal < 2)
              dimmer.setState(OFF);
           if(outVal > 1 )
              dimmer.setState(ON);             
             if(outVal >= 75)
                dimmer.setPower(75);
              else
                dimmer.setPower(outVal);
      //     delay(5);
           delayMicroseconds(50);     
        }
    if (preVal != outVal)
      {
        Serial.print("SliderValue -> ");
        CurrentValue(dimmer.getPower());
        Serial.print(dimmer.getPower());
        Serial.println("%");
        Serial.println(dimmer.getState());
      }
      //delay(50);
      digitalWrite(wifiStatusLed,LOW);
      delay(100);
      digitalWrite(wifiStatusLed,HIGH);
      
}
void CurrentValue(int val)
{
  if ((val / 100) == 0) 
      Serial.print(" ");
  if ((val / 10) == 0) 
      Serial.print(" ");

}

/*************************************************** check if wifi connection lost ****************************************/
void keepWiFiAlive(void * pvParameter){
   while(1)
   {
        if(WiFi.status() == WL_CONNECTED)
          {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
          }
            Serial.println("[WIFI] Connecting");
             WiFi.begin(esid.c_str(), epass.c_str());
              unsigned long startAttemptTime = millis();
        // Keep looping while we're not connected and haven't reached the timeout
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000);

        // If we failed to connect within the timeout period, retry in 30 seconds.
        if(WiFi.status() != WL_CONNECTED){
            Serial.println("[WIFI] FAILED");
            vTaskDelay(30000 / portTICK_PERIOD_MS);
       continue;
        }
        Serial.println("[WIFI] Connected: " + WiFi.localIP());
   }
}

/******************************************************* functionality for manual buttons for appliances **************************************/
void manualSwitch()
{
        int switchPin1State = digitalRead(switchPin1);    
        int switchPin2State = digitalRead(switchPin2);    
        int switchPin3State = digitalRead(switchPin3);
        int switchPin4State = digitalRead(switchPin4);    
        int switchPin5State = digitalRead(switchPin5); 
         
/*      Switch1 pressed   */           
        if (switchPin1State == HIGH && switch1PreviousState == LOW)
          {
                switch1PreviousState = HIGH;
                Serial.println("Button 1 pressed");
                device1State = "ON";
                digitalWrite(device1,HIGH);
          }      
        if (switchPin1State == LOW && switch1PreviousState == HIGH) 
          {
                switch1PreviousState = LOW;
                Serial.println("Button 1 released");
                device1State = "OFF";
                digitalWrite(device1,LOW);
          }
          
/*      Switch2 Pressed   */
      if (switchPin2State == HIGH && switch2StatePrevious == LOW)
         {
               switch2StatePrevious = HIGH;
               Serial.println("Button 2 pressed");
               device2State = "ON";
               digitalWrite(device2,HIGH);
         }      
      if (switchPin2State == LOW && switch2StatePrevious == HIGH)
        {
              switch2StatePrevious = LOW;
              Serial.println("Button 2 released");
              device2State = "OFF";
              digitalWrite(device2,LOW);
        }

/*      Switch3 Pressed     */
    if (switchPin3State == HIGH && switch3StatePrevious == LOW) 
      {
            switch3StatePrevious = HIGH;
            Serial.println("Button 3 pressed");
            device3State = "ON";
            digitalWrite(device3,HIGH);
      }      
    if (switchPin3State == LOW && switch3StatePrevious == HIGH)
       {
            switch3StatePrevious = LOW;
            Serial.println("Button 3 released");
            device3State = "OFF";
            digitalWrite(device3,LOW);    
      }

/*      Switch4 Pressed     */
    if (switchPin4State == HIGH && switch4StatePrevious == LOW) 
     {
          switch4StatePrevious = HIGH;
          Serial.println("Button 4 pressed");
          device4State = "ON";
          digitalWrite(device4,HIGH);    
    }      
    if (switchPin4State == LOW && switch4StatePrevious == HIGH) {
      switch4StatePrevious = LOW;
      Serial.println("Button 4 released");
       device4State = "OFF";
       digitalWrite(device4,LOW);
    }
    
/*      Switch5 Pressed   */    
    if (switchPin5State == HIGH && switch5StatePrevious == LOW) {
      switch5StatePrevious = HIGH;
      Serial.println("Button 5 pressed");
      device5State = "ON";
      digitalWrite(device5,HIGH);
    }
      
    if (switchPin5State == LOW && switch5StatePrevious == HIGH) {
      switch5StatePrevious = LOW;
      Serial.println("Button 5 released");
      device5State = "OFF";
      digitalWrite(device5,LOW);
    }
        currentMillis = millis();
    readButtonState();
}
void flash_erase()
{
    for(int address = 0; address <= EEPROM_SIZE; address++)
    {
      EEPROM.write(address,0);  
    }
    EEPROM.commit();
    delay(500);
}
void firmwareUpdate()
{         
         t_httpUpdate_return ret = ESPhttpUpdate.update(ota_url,"", "CC AA 48 48 66 46 0E 91 53 2C 9C 7C 23 2A B1 74 4D 29 9D 33");

        switch(ret) {
            case HTTP_UPDATE_FAILED:
                Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;

            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("HTTP_UPDATE_NO_UPDATES");
                break;

            case HTTP_UPDATE_OK:
                Serial.println("HTTP_UPDATE_OK");
                break;
        }
}
void pubState()
{
  String response = "device1 : ";
          response += String(device1State);
          response += ",";
          response += "device2";
          response += String(device2State);
          response += ",";
          response += "device3";
          response += String(device3State);
          response += ",";
          response += "device4";
          response += String(device4State);
          response += ",";
          response += "device5";
          response += String(device5State);
 char PayLoad[60];
 response.toCharArray(PayLoad,sizeof(PayLoad));                
          
}
