/*
 changes :
 password change api implemented
 100 - 109 : schedule
 400 : device adress
 500 - 507 : deviceStates
 
 setting time :

 month will be 1 to 12

 while scheduoing month will be 0 to 11
 
Production Server Code  v2.5
    
 features implimented :
 1. wifi cred via ipAddress   - tested
 2. controlling appliances without & with internet   - tested
 3. configuring device id manually via http://192.168.4.1/id?deviceId=COCA-CH5-C0000    -  tested
 4. each appliances will retain it's previous state  -  tested
 5. time fetching from the NTP server & given to internal rtc - tested 
 6. scheduling - implemented but Not tested with Application
 7. wifi reconnecting if the connection lost after every 30 seconds
 8. automatic mode switch depend on the network availability 
 9. time taken from the app in local mode
 10. manual switch feedback updated in app
 11. restored slider state before power off 
 12. schedule in offline mode added   

 1. http://192.168.4.1/codeVersion                           : codeVersion check
 2. http://192.168.4.1/id?deviceId=****                      : device id allocation
 3. http://192.168.4.1/setting?ssid=****&pass=****           : wifi credentials
 4. http://192.168.4.1/switchStatus                          : manual switch status update
 5. http://192.168.4.1/calender?date=**&month=**&year=**&hour=**&minutes=**&seconds=**  : time sync with app
 6. http://192.168.4.1/changePass?pass=**********            : changePassword
 7. http://192.168.4.1/schedule?fdate=**&fmonth=**&fyear=****&fhour=**&fminutes=**&tdate=**&tmonth=**&tyear=****&thour=**&tminutes=**&scheduleId=*&type=*

*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP32httpUpdate.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP32Ping.h>
#include <RBDdimmer.h>

#include "certificates.h"
#include "declarations.h"
#include "internal_rtc.h"

WiFiClientSecure httpsClient =  WiFiClientSecure();
PubSubClient mqttClient(httpsClient);
dimmerLamp dimmer(dimmerPin, zcPin);
WebServer server(80);
internal_rtc rtc;

void setup() {
  Serial.println("+++++++++++++++++++Welcome To Qurolabs+++++++++++++++++");
Serial.begin(115200);           //Initialising debug port 
      
/*    relay outputs   */
pinMode(device1, OUTPUT);
pinMode(device2, OUTPUT);
pinMode(device3, OUTPUT);
pinMode(device4, OUTPUT);
pinMode(device5, OUTPUT);

digitalWrite(device1,LOW);
digitalWrite(device2,LOW);
digitalWrite(device3,LOW);
digitalWrite(device4,LOW);
digitalWrite(device5,LOW);

        pinMode(internetStatusLed,OUTPUT);    // Blue led : Status for Internet mode - LED2
        pinMode(wifiStatusLed,OUTPUT);        // Orange led : Statud for wifi-Hotspot mode - LED1
pinMode(LED_BUILTIN, OUTPUT);
digitalWrite(LED_BUILTIN,LOW);
digitalWrite(internetStatusLed,LOW);
digitalWrite(wifiStatusLed,LOW);

pinMode(switchPin1,INPUT_PULLDOWN);
pinMode(switchPin2,INPUT_PULLDOWN);
pinMode(switchPin3,INPUT_PULLDOWN);
pinMode(switchPin4,INPUT_PULLDOWN);
pinMode(switchPin5,INPUT_PULLDOWN);

pinMode(bootPin, INPUT_PULLUP); 
  EEPROM.begin(EEPROM_SIZE);       //Initialasing EEPROM
  //delay(500);

digitalWrite(device1,EEPROM.read(device1StateAddress));
digitalWrite(device2,EEPROM.read(device2StateAddress));
digitalWrite(device3,EEPROM.read(device3StateAddress));
digitalWrite(device4,EEPROM.read(device4StateAddress));
digitalWrite(device5,EEPROM.read(device5StateAddress));

  configureDeviceId();
/* Topic arrangment */
  strcpy(subTopic,ThingName);
  strcat(subTopic,"/PUB");
  Serial.println("Request Tpoic : " + String(subTopic));    // request received
  
  strcpy(pubTopic,ThingName);
  strcat(pubTopic,"/SUB");
  Serial.println("Response Topic : " + String(pubTopic));     // response topic
 
  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect(true);

               dimmer.begin(NORMAL_MODE, ON);
              if(EEPROM.read(dimmerAddress) <= 3)
                   dimmer.setState(OFF);
              if(EEPROM.read(dimmerAddress) > 1 )
                   dimmer.setState(ON);             
              if(EEPROM.read(dimmerAddress) >= 75)
                   dimmer.setPower(75);
              else
                   dimmer.setPower(EEPROM.read(dimmerAddress));
                
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
                      Serial.println("Localmode Started");                      
                      initialise_rtc();
                      rtc.setTime(30, 24, 14, 6, 9, 2021);  // 17th Jan 2021 15:24:30
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
                      delay(1000);
                     initialise_rtc();
                 }
               xTaskCreate(taskShedule," taskShedule",5000,NULL,1,NULL);
               xTaskCreate(keepWiFiAlive," keepWiFiAlive",5000,NULL,1,NULL);     
}

void handleLocalMode()
{
        server.on("/", []() {
          sliderString = server.arg("value");
          sliderValue = sliderString.toInt();
          EEPROM.write(dimmerAddress,sliderValue);
          EEPROM.commit();
          dimmerOn();
      digitalWrite(wifiStatusLed,LOW);
      delay(100);
      digitalWrite(wifiStatusLed,HIGH);
      server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 

    });
      server.on("/id", []() {
          deviceIDString = server.arg("deviceId");        
          EEPROM.writeString(deviceAddress,deviceIDString);
          EEPROM.commit();
          Serial.println("Device Id Written is: " + EEPROM.readString(deviceAddress));
          server.send(404, "text/plain", deviceIDString);
    });

         server.on("/calender", []() {
          date = server.arg("date");    
          Date = date.toInt();
          month = server.arg("month");        // 1 to 12
          Month = month.toInt();
          year = server.arg("year");
          Year = year.toInt();
          hour = server.arg("hour");
          Hour = hour.toInt();
          minutes = server.arg("minutes");
          Minutes = minutes.toInt();
          
          rtc.setTime(Seconds, Minutes, Hour, Date, Month, Year);  // 17th Jan 2021 15:24:30  
          
    });
        server.on("/schedule",[]() {
          String fdate = server.arg("fdate");
          int fDate = fdate.toInt();
          EEPROM.write(100,fDate);
          EEPROM.commit();
          Serial.print("From Date Received : ");
          Serial.println(byte(EEPROM.read(100))); 
                    
          String fmonth = server.arg("fmonth");     // 0 to 11
          int fMonth = fmonth.toInt();
          EEPROM.write(101,fMonth);
          EEPROM.commit();
          Serial.print("From Month Received : ");
          Serial.println(byte(EEPROM.read(101))); 
                    
          String fhour = server.arg("fhour");
          int fHour = fhour.toInt();
          EEPROM.write(102,fHour);  
          EEPROM.commit();
          Serial.print("From Hour Received : ");
          Serial.println(byte(EEPROM.read(102))); 
                    
          String fminutes = server.arg("fminutes");
          int fMinutes = fminutes.toInt();
          EEPROM.write(103,fMinutes);  
          EEPROM.commit();
          Serial.print("From Minutes Received : ");
          Serial.println(byte(EEPROM.read(103)));           
          
          String tdate = server.arg("tdate");
          int tDate = tdate.toInt();
          EEPROM.write(104,tDate);
          EEPROM.commit();
          Serial.print("To Date Received : ");
          Serial.println(byte(EEPROM.read(104))); 
                    
          String tmonth = server.arg("tmonth");
          int tMonth = tmonth.toInt();
          EEPROM.write(105,tMonth);
          EEPROM.commit();
          Serial.print("To Month Received : ");
          Serial.println(byte(EEPROM.read(105)));
                     
          String thour = server.arg("thour");
          int tHour = thour.toInt();
          EEPROM.write(106,tHour);  
          EEPROM.commit();
          Serial.print("To Hour Received : ");
          Serial.println(byte(EEPROM.read(106))); 
                    
          String tminutes = server.arg("tminutes");
          int tMinutes = tminutes.toInt();
          EEPROM.write(107,tMinutes);   
          EEPROM.commit();     
          Serial.print("To Minutes Received : ");
          Serial.println(byte(EEPROM.read(107)));
                        
          String schedule = server.arg("scheduleId");
          int device = schedule.toInt();
          EEPROM.write(108,device);  
          EEPROM.commit();
          Serial.print("Schedule Received : ");
          Serial.println(byte(EEPROM.read(108))); 

          String type = server.arg("type");
          int Type = type.toInt();
          EEPROM.write(109,Type);  
          EEPROM.commit();
          Serial.print("Type Received : ");
          Serial.println(byte(EEPROM.read(109))); 
          
           server.send(404, "text/plain", "schedule Saved");
           
    });
          server.on("/changePass", []() {
          newPass = server.arg("pass");        
          EEPROM.writeString(passwordAddress,newPass);
          EEPROM.commit();
          server.send(200, "text/html", sendpass(newPass));
          Serial.println("Password Changed to : " + EEPROM.readString(passwordAddress));
          
          delay(2000);
          ESP.restart();
    });
    
  server.on("/device1On", handle_device1On);
  server.on("/device1Off", handle_device1Off);
  server.on("/device2On", handle_device2On);
  server.on("/device2Off", handle_device2Off);
  server.on("/device3On", handle_device3On);
  server.on("/device3Off", handle_device3Off);
  server.on("/device4On", handle_device4On);
  server.on("/device4Off", handle_device4Off);
  server.on("/device5On", handle_device5On);
  server.on("/device5Off", handle_device5Off);
  server.on("/allOff", handle_allOff);
  server.on("/allOn",handle_allOn);
  server.onNotFound(handle_NotFound);
   server.on("/switchStatus", handle_Status);  // status of manual switches
    server.on("/codeVersion", handle_Codeversion);
}

String sendpass(String newpass)
{
    String response = "<!DOCTYPE html> <html>\n";
          response +="<body>";
          response += "{";
          response += "Password :";
          response += newPass;
          response += "}";
          response +="</body>\n";
          response +="</html>\n";
       char PayLoad[50];
       response.toCharArray(PayLoad,sizeof(PayLoad));

   Serial.println(PayLoad);
  return PayLoad;
}
/********************** change password ******************************/
void readPass()
{
  EEPROM.readString(passwordAddress).toCharArray(Password,sizeof(Password));
  Serial.println("Current Pass : ");
  Serial.print(Password);
}
/***********************   Device Id Configuration   *************************************/
void configureDeviceId()
{
  EEPROM.readString(deviceAddress).toCharArray(ThingName,sizeof(ThingName));
  Serial.println("Device Id : " + String(ThingName));
}
/******************************************** RTC initialisation ***********************************/
void initialise_rtc()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
        Serial.println("Failed to Obtain the Time");
       // ESP.restart();
  }
}

void auto_mode()
{
  
    if(mqttClient.connected())     
          {
               if(netflag == false)
                  {
                          //   connectToServer();     
                           digitalWrite(LED_BUILTIN,HIGH);
                              digitalWrite(wifiStatusLed,LOW);                // Orange led off for wifi-hotspot mode 
                              digitalWrite(internetStatusLed, HIGH);        // Blue led stable for internet mode
                        Serial.println("Switched to Internet mode");                                                  
                             netflag = true;  
                  }
                        if(netflag)
                             {
                                  while(1)
                                  {
                                      manualSwitch();                                                                              
                                      mqttLoop();
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
                      digitalWrite(LED_BUILTIN,LOW);
                              digitalWrite(wifiStatusLed,HIGH);                // Orange led stable for wifi-hotspot mode
                              digitalWrite(internetStatusLed, LOW);        // Blue led off for internet mode 
                       Serial.println("Switched to Access point i.e HotSpot mode");
                           Serial.println("You can Acess Local Server to Control Appliances.........");
                           readPass();
                             WiFi.softAP(ThingName, Password);
                                delay(100);                                              
                                OpenWebServer();
                                   handleLocalMode();    
                                    server.begin();
                                   Serial.println("HTTP server started");
                                  blockflag = true;
                    }
                    if(blockflag == true)
                      {
                           while(1)
                            {       
                                server.handleClient();                  
                                switchFeedbackLocal();         
                                if(Ping.ping("www.google.com"))
                                {
                                  Serial.println("Internet Connected Successfully"); 
                                  connectToServer();
                                  break;                                      
                                }                                  
                            }    
                     }                     
                     netflag =false;
          }
}
void loop() {
  
     auto_mode(); 
       
 }
/*************************************************** functionality for accesing wifi credentials from a local wifi server  ********************************************/

void OpenWebServer()
{
 {
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
              digitalWrite(internetStatusLed,HIGH);
              digitalWrite(wifiStatusLed,HIGH);
               flash_erase();
               ESP.restart();
    }
    if (buttonState == LOW && buttonStatePrevious == HIGH) {
      buttonStatePrevious = LOW;
      buttonStateLongPress = false;
      Serial.println("Button released");

      if (buttonPressDuration < minButtonLongPressDuration) {
        Serial.println("Button pressed shortly");

    }   
    // store the current timestamp in previousButtonMillis
    previousButtonMillis = currentMillis;

   }
  }
}

/******************************************************* functionality for manual buttons for appliances **************************************/
void manualSwitch()
{
         switchPin1State = digitalRead(switchPin1);    
         switchPin2State = digitalRead(switchPin2);    
         switchPin3State = digitalRead(switchPin3);
         switchPin4State = digitalRead(switchPin4);    
         switchPin5State = digitalRead(switchPin5); 
         
/*      Switch1 pressed   */           
        if (switchPin1State == HIGH)
          {
                if(switch1PreviousState == HIGH)
                {
                   Serial.println("Button 1 pressed");
                  device1State = "ON";
                  digitalWrite(device1,HIGH);
                  EEPROM.write(device1StateAddress,switch1PreviousState);
                  EEPROM.commit();
                   sendResponse();
                   switch1PreviousState = LOW;
                }
          }      
        if (switchPin1State == LOW) 
          {
                if(switch1PreviousState == LOW)
                {
                  Serial.println("Button 1 released");
                  device1State = "OFF";
                  digitalWrite(device1,LOW);
                  EEPROM.write(device1StateAddress,switch1PreviousState);
                  EEPROM.commit();                
                  sendResponse();
                  switch1PreviousState = HIGH;
                }
          }
          
/*      Switch2 Pressed   */
      if (switchPin2State == HIGH)
         {
               if(switch2PreviousState == HIGH)
               {
                  Serial.println("Button 2 pressed");
                  device2State = "ON";
                  digitalWrite(device2,HIGH);
                  EEPROM.write(device2StateAddress,switch2PreviousState);
                  EEPROM.commit();               
                  sendResponse();
                  switch2PreviousState = LOW;
               }
         }      
      if (switchPin2State == LOW)
        {
              if(switch2PreviousState == LOW)
              {
                 Serial.println("Button 2 released");
                 device2State = "OFF";
                 digitalWrite(device2,LOW);
                 EEPROM.write(device2StateAddress,switch2PreviousState);
                 EEPROM.commit();
                 sendResponse();
                 switch2PreviousState = HIGH;
              }  
        }

/*      Switch3 Pressed     */
    if (switchPin3State == HIGH) 
      {
            if(switch3PreviousState == HIGH)
            {
                Serial.println("Button 3 pressed");
                device3State = "ON";
                digitalWrite(device3,HIGH);
                EEPROM.write(device3StateAddress,switch3PreviousState);
                EEPROM.commit();
                sendResponse();
                switch3PreviousState = LOW;
            }
      }      
    if (switchPin3State == LOW)
       {
            if(switch3PreviousState == LOW)
            {
               Serial.println("Button 3 released");
               device3State = "OFF";
               digitalWrite(device3,LOW);
               EEPROM.write(device3StateAddress,switch3PreviousState);
               EEPROM.commit();
               sendResponse();
               switch3PreviousState = HIGH;
            }    
      }

/*      Switch4 Pressed     */
    if (switchPin4State == HIGH) 
     {
          if(switch4PreviousState == HIGH)
          {
              Serial.println("Button 4 pressed");
              device4State = "ON";
              digitalWrite(device4,HIGH); 
              EEPROM.write(device4StateAddress,switch4PreviousState);
              EEPROM.commit();
              sendResponse();  
              switch4PreviousState= LOW;
          } 
    }      
    if (switchPin4State == LOW)
    {
          if(switch4PreviousState == LOW)
          {
             Serial.println("Button 4 released");
             device4State = "OFF";
             digitalWrite(device4,LOW);
             EEPROM.write(device4StateAddress,switch4PreviousState);
             EEPROM.commit();       
             sendResponse();
             switch4PreviousState = HIGH;
          }
    }
    
/*      Switch5 Pressed   */    
    if (switchPin5State == HIGH) 
    {
         if(switch5PreviousState == HIGH)
         {
            Serial.println("Button 5 pressed");
            device5State = "ON";
            digitalWrite(device5,HIGH);
            EEPROM.write(device5StateAddress,switch5PreviousState);
            EEPROM.commit();      
            sendResponse();
            switch5PreviousState = LOW;
         }
    }
      
    if (switchPin5State == LOW) 
    {
        if(switch5PreviousState == LOW)
        {
            Serial.println("Button 5 released");
            device5State = "OFF";
            digitalWrite(device5,LOW);
            EEPROM.write(device5StateAddress,switch5PreviousState);
            EEPROM.commit();      
            sendResponse();
            switch5PreviousState = HIGH;
        }
    }
        currentMillis = millis();
    readButtonState();
}


/*******************switch feedback in local mode*************/
String switchFeedback(String device1State,String device2State,String device3State,String device4State,String device5State,String sliderString)
{
  String response = "<!DOCTYPE html> <html>\n";
          response +="<body>";
          response += "{";     
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
               response += ",";
               response += "sliderValue:";
               response += sliderValue;
               response += "}";
            char PayLoad[150];
            response.toCharArray(PayLoad,sizeof(PayLoad));
  response +="</body>\n";
  response +="</html>\n";
   Serial.println(PayLoad);
  return PayLoad;
}
void switchFeedbackLocal()
{
         switchPin1State = digitalRead(switchPin1);    
         switchPin2State = digitalRead(switchPin2);    
         switchPin3State = digitalRead(switchPin3);
         switchPin4State = digitalRead(switchPin4);    
         switchPin5State = digitalRead(switchPin5); 
         
/*      Switch1 pressed   */           
        if (switchPin1State == HIGH)
          {
                if(switch1PreviousState == HIGH)
                {
                    Serial.println("Button 1 pressed");
                    device1State = "ON";
                    digitalWrite(device1,HIGH);
                    EEPROM.write(device1StateAddress,switch1PreviousState);
                    EEPROM.commit();   
                    switch1PreviousState = LOW;
                }             
        //       server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));
          }      
        if (switchPin1State == LOW ) 
          {
                if(switch1PreviousState == LOW)
                {
                     Serial.println("Button 1 released");
                     device1State = "OFF";
                      digitalWrite(device1,LOW);
                      EEPROM.write(device1StateAddress,switch1PreviousState);
                      EEPROM.commit();           
                      switch1PreviousState = HIGH;
                }      
           //     server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));
          }
          
/*      Switch2 Pressed   */
      if (switchPin2State == HIGH)
         {
               if(switch2PreviousState == HIGH)
               {
                   Serial.println("Button 2 pressed");
                   device2State = "ON";
                    digitalWrite(device2,HIGH);
                      EEPROM.write(device2StateAddress,switch2PreviousState);
                      EEPROM.commit();    
                       switch2PreviousState = LOW;
               }           
           //    server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));
         }      
      if (switchPin2State == LOW)
        {
             if(switch2PreviousState == LOW)
             {
                 Serial.println("Button 2 released");
                 device2State = "OFF";
                  digitalWrite(device2,LOW);
                  EEPROM.write(device2StateAddress,switch2PreviousState);
                  EEPROM.commit();     
                  switch2PreviousState = HIGH;  
             }        
          //    server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));
        }

/*      Switch3 Pressed     */
    if (switchPin3State == HIGH) 
      {
            if(switch3PreviousState == HIGH)
            {
               Serial.println("Button 3 pressed");
               device3State = "ON";
               digitalWrite(device3,HIGH);
                EEPROM.write(device3StateAddress,switch3PreviousState);
                EEPROM.commit();  
                switch3PreviousState = LOW;
            }           
         //   server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));
      }      
    if (switchPin3State == LOW)
       {
            if(switch3PreviousState == LOW)
            {
                Serial.println("Button 3 released");
                 device3State = "OFF";
                digitalWrite(device3,LOW);
                 EEPROM.write(device3StateAddress,switch3PreviousState);
                 EEPROM.commit(); 
                switch3PreviousState = HIGH;
            }            
        //   server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));     
      }

/*      Switch4 Pressed     */
    if (switchPin4State == HIGH) 
     {
          if(switch4PreviousState == HIGH)
          {
                Serial.println("Button 4 pressed");
                 device4State = "ON";
                digitalWrite(device4,HIGH); 
                   EEPROM.write(device4StateAddress,switch4PreviousState);
                   EEPROM.commit();    
                  switch4PreviousState = LOW;
          }
       //   server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));   
    }      
    if (switchPin4State == LOW) 
    {
          if(switch4PreviousState == LOW)
          {
              Serial.println("Button 4 released");
              device4State = "OFF";
              digitalWrite(device4,LOW);
               EEPROM.write(device4StateAddress,switch4PreviousState);
               EEPROM.commit();       
               switch4PreviousState = HIGH;
          }
     //  server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));   
    }
    
/*      Switch5 Pressed   */    
    if (switchPin5State == HIGH) 
    {
          if(switch5PreviousState == HIGH)
          {
             Serial.println("Button 5 pressed");
             device5State = "ON";
             digitalWrite(device5,HIGH);
              EEPROM.write(device5StateAddress,switch5PreviousState);
              EEPROM.commit();      
              switch5PreviousState = LOW;
          }
  //    server.send(200, "text/html",switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));
    }
      
    if (switchPin5State == LOW)
    {
          if(switch5PreviousState == LOW)
          {
              Serial.println("Button 5 released");
               device5State = "OFF";
                 digitalWrite(device5,LOW);
                  EEPROM.write(device5StateAddress,switch5PreviousState);
                  EEPROM.commit(); 
                  switch5PreviousState = HIGH;
          }     
    //  server.send(200, "text/html", switchFeedback(switch1PreviousState,switch2PreviousState, switch3PreviousState,switch4PreviousState, switch5PreviousState));
    }
        currentMillis = millis();
    readButtonState();
}
void flash_erase()
{
    for(int address = 0; address <= 300; address++)
    {
      EEPROM.write(address,0);  
    }
    EEPROM.commit();
    for(int address = 500; address <= 510; address++)
      {
        EEPROM.write(address,0); 
      }
      EEPROM.writeString(passwordAddress,defaultPass);
      EEPROM.commit();
    delay(500);
}
void firmwareUpdate()
{         
         t_httpUpdate_return ret = ESPhttpUpdate.update(ota_url);

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
/*************************************************** check if wifi connection lost ****************************************/
void keepWiFiAlive(void * pvParameter){
   while(1)
   {
        if(WiFi.status() == WL_CONNECTED)
          {
            continue; 
          }
            Serial.println("[WIFI] Connecting");
             WiFi.begin(esid.c_str(), epass.c_str());
              unsigned long startAttemptTime = millis();
        // Keep looping while we're not connected and haven't reached the timeout
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000);
        // If we failed to connect within the timeout period, retry in 30 seconds.
        if(WiFi.status() != WL_CONNECTED){
            Serial.println("[WIFI] FAILED");
            vTaskDelay(30000 / portTICK_PERIOD_MS);
             continue;
        }
        Serial.println("[WIFI] Connected: " + WiFi.localIP());
   }
}
void taskShedule(void * pvParameter){
   while(1)
   {
        scheduleTask();
   }
}
 /********************************************* AC dimmer functionality *******************************/
void dimmerOn()
{
  int preVal = outVal;
    if(sliderValue != 0 || sliderValue == 0)
      {
        outVal = sliderValue;
            if(outVal <= 3)
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
}
void CurrentValue(int val)
{
  if ((val / 100) == 0) 
      Serial.print(" ");
  if ((val / 10) == 0) 
      Serial.print(" ");

}

/****************************************** functionality when device is connected to Server **********************************************/
void with_internet()
{
     StaticJsonDocument<256> doc;
     DeserializationError error = deserializeJson(doc,rcvdPayload);          
      if(error)
          return;
          
  char request[8] = {
                        doc["device1"],doc["device2"],doc["device3"],doc["device4"],doc["device5"],doc["deviceId"],doc["sliderValue"],doc["firmwareUpdate"]     // device1  && device2 && device3 && device4 && device5
                    };
  char schedule[12] =  {  
                          doc["fdate"],doc["fmonth"],doc["fhour"],doc["fminutes"],doc["tdate"],doc["tmonth"],doc["thour"],doc["tminutes"],doc["scheduleId"],doc["type"],doc["allOff"],doc["allOn"]                  
                       };        
    
/* Control the applicances */
      if(request[0])  {
           device1State = "ON";
           device1PreviousState = HIGH;
           digitalWrite(device1,device1PreviousState);
             EEPROM.write(device1StateAddress,device1PreviousState);
             EEPROM.commit();     
      }           
      else if(!request[0]){ 
           device1State = "OFF";
           device1PreviousState = LOW;
           digitalWrite(device1,device1PreviousState);
            EEPROM.write(device1StateAddress,device1PreviousState);
            EEPROM.commit();      
      }   
      
      if(request[1])  { 
           device2State = "ON"; 
           device2PreviousState = HIGH;
           digitalWrite(device2,device2PreviousState);
             EEPROM.write(device2StateAddress,device2PreviousState);
             EEPROM.commit();
      }            
      else if (!request[1]){
           device2State = "OFF";  
           device2PreviousState = LOW;
           digitalWrite(device2,device2PreviousState);
            EEPROM.write(device2StateAddress,device2PreviousState);
            EEPROM.commit();          
      }      
      
      if(request[2])  {
           device3State = "ON";
           device3PreviousState = HIGH;
           digitalWrite(device3,device3PreviousState);
            EEPROM.write(device3StateAddress,device3PreviousState);
            EEPROM.commit();
      }                    
      else if(!request[2]){
           device3State = "OFF";
           device3PreviousState = LOW;
           digitalWrite(device3,device3PreviousState);
            EEPROM.write(device3StateAddress,device3PreviousState);
            EEPROM.commit();
      }
      
      if(request[3])  {
           device4State = "ON";             
           device4PreviousState = HIGH;
           digitalWrite(device4,device4PreviousState);
            EEPROM.write(device4StateAddress,device4PreviousState);
            EEPROM.commit();
      }     
      else if (!request[3]){
           device4State = "OFF";
           device4PreviousState = LOW;
           digitalWrite(device4,device4PreviousState);
            EEPROM.write(device4StateAddress,device4PreviousState);
            EEPROM.commit();
      }       
                       
      if(request[4])  {          
           device5State = "ON";
           device5PreviousState = HIGH;
           digitalWrite(device5,device5PreviousState);
            EEPROM.write(device5StateAddress,device5PreviousState);
            EEPROM.commit();
           
      }      
      else if(!request[4]) {
           device5State = "OFF";
           device5PreviousState = LOW;
           digitalWrite(device5,device5PreviousState);
            EEPROM.write(device5StateAddress,device5PreviousState);
            EEPROM.commit();
           
      } 
      if(request[6] != 0)
      {
          sliderValue = request[6];
          EEPROM.write(dimmerAddress,sliderValue);
          EEPROM.commit();
          dimmerOn();
                digitalWrite(wifiStatusLed,HIGH);
                 delay(100);
                digitalWrite(wifiStatusLed,LOW);
      }

      if(request[7])
      {
          firmwareUpdate();
      }
                  if(schedule[0] != 0){  
                  EEPROM.write(100,schedule[0]);      //  fdate -> 100
                  EEPROM.commit();
                  Serial.print("From Date Received : ");
                  Serial.println(byte(EEPROM.read(100)));  
                  }
                  if(schedule[1] != 0){
                  EEPROM.write(101,schedule[1]);        // fmonth -> 101
                  EEPROM.commit();  
                  Serial.println("From Month Received : ");
                  Serial.println(byte(EEPROM.read(101)));
                  }
                  if(schedule[2] != 0){
                  EEPROM.write(102,schedule[2]);        // fhour -> 102
                  EEPROM.commit();  
                  Serial.print("From Hours Received : ");
                  Serial.println(byte(EEPROM.read(102)));
                  }
                  if(schedule[3] != 0){
                  EEPROM.write(103,schedule[3]);        // fminutes -> 103
                  EEPROM.commit();  
                  Serial.print("From Minutes Received : ");
                  Serial.println(byte(EEPROM.read(103)));
                  }
                  
                  if(schedule[4] != 0){
                  EEPROM.write(104,schedule[4]);        // tdate -> 104
                  EEPROM.commit();  
                  Serial.print("To Date Received : ");
                  Serial.println(byte(EEPROM.read(104)));
                  }
                  
                  if(schedule[5] != 0){
                  EEPROM.write(105,schedule[5]);        // tmonth -> 105
                  EEPROM.commit();  
                  Serial.print("To Month Received : ");
                  Serial.println(byte(EEPROM.read(105)));
                  }

                  if(schedule[6] != 0){
                  EEPROM.write(106,schedule[6]);        // thour -> 106
                  EEPROM.commit();  
                  Serial.print("To Hour Recived : ");
                  Serial.println(byte(EEPROM.read(106)));                    
                  }

                  if(schedule[7] != 0){
                  EEPROM.write(107,schedule[7]);        // tminutes -> 107
                  EEPROM.commit();  
                  Serial.print("To Minutes Received : ");
                  Serial.println(byte(EEPROM.read(107)));                    
                  }
                  
                  if(schedule[8] != 0){
                  EEPROM.write(108,schedule[8]);        // scheduleId -> 108
                  EEPROM.commit();  
                  Serial.print("Schedule Trigger : ");
                  Serial.println(byte(EEPROM.read(108)));                                        
                  }

                  if(schedule[9] != 0)
                  {
                       EEPROM.write(109,schedule[9]);        // schedule -> 108
                       EEPROM.commit();    
                  }
                if(schedule[10])                                  //  Trigger to Off all devices
                   {
                         digitalWrite(device1,LOW);
                         digitalWrite(device2,LOW);              
                         digitalWrite(device3,LOW);
                         digitalWrite(device4,LOW);
                         digitalWrite(device5,LOW);
                   } 
                if(schedule[11])                                  //  Trigger to On all devices
                   {
                         digitalWrite(device1,HIGH);
                         digitalWrite(device2,HIGH);              
                         digitalWrite(device3,HIGH);
                         digitalWrite(device4,HIGH);
                         digitalWrite(device5,HIGH);
                         sliderValue = EEPROM.read(dimmerAddress);
                         dimmer.setPower(sliderValue);
                   }                   
 sendResponse();
// memset(rcvdPayload,0,sizeof(rcvdPayload));
// memset(schedule,0,sizeof(schedule)); 
}
void sendResponse()
{    
 StaticJsonDocument<256> response;
  
  response["deviceId"] = ThingName;
  response["device1"] = digitalRead(device1);
  response["device2"] = digitalRead(device2);
  response["device3"] = digitalRead(device3);
  response["device4"] = digitalRead(device4);
  response["device5"] = digitalRead(device5);
  response["sliderValue"] = sliderValue;
  response["deviceStatus"] = "Online";
  
  char Buffer[200];
  Serial.println("Publishing message to AWS...");
  serializeJsonPretty(response, Buffer);               
//  serializeJsonPretty(response, Serial);
  mqttClient.publish(pubTopic,Buffer);  //,sizeof(Buffer)/sizeof(Buffer[0]));
//  vTaskDelay(2000 / portTICK_RATE_MS);      // publish the state every 1 minute
}

/******************************************************* schedule devices ************************************/
void scheduleTask()
{
   
    Serial.println(rtc.getTimeDate());
    delay(1000);

    if(EEPROM.read(109) == 1)
         single_schedule();
    if(EEPROM.read(109) == 2)
        repeat_schedule();
}
void repeat_schedule()
{
  
      if((rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 1)) 
      {
           Serial.println("Schedule-1 Successfully played......");
            //long unsigned sec = (((tdate - fdate)*86400) + 0 + ((thour - fhour)*3600) + ((tminutes - fminutes)*60))*1000;
              long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
           Serial.println(sec);
            digitalWrite(device1,HIGH);
             delay(sec);                   
            digitalWrite(device2,LOW);        
      }
      if((rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 2)) 
      {
            Serial.println("Schedule-2 Successfully played......");
             long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
            /*   long unsigned sec = (((tdate - fdate)*86400) + 0 + ((thour - fhour)*3600) + ((tminutes - fminutes)*60))*1000  */
             digitalWrite(device2,HIGH);
              delay(sec);                     
             digitalWrite(device2,LOW);      
      }
      if((rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 3)) 
      {
            Serial.println("Schedule-3 Successfully played......");
             long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
             digitalWrite(device3,HIGH);
              delay(sec);
             digitalWrite(device3,LOW);        
      }
      if((rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 4)) 
      {
            Serial.println("Schedule-4 Successfully played......");
             long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
             digitalWrite(device4,HIGH);
              delay(sec);
             digitalWrite(device4,LOW);        
      }
      if((rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 5)) 
      {
            Serial.println("Schedule-5 Successfully playing......");
            long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
             digitalWrite(device5,HIGH);
              delay(sec);
             digitalWrite(device5,LOW);        
      }
      connectToServer();
}
void single_schedule()
{
  /*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-1 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */
      if((rtc.getDay() == EEPROM.read(100)) && (rtc.getMonth() == EEPROM.read(101)) && (rtc.getYear() > 2020) && (rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 1))      // device1 on    
         {
            Serial.println("Schedule-1 Successfully played......");
            //long unsigned sec = (((tdate - fdate)*86400) + 0 + ((thour - fhour)*3600) + ((tminutes - fminutes)*60))*1000;
              long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
            Serial.println(sec);
             digitalWrite(device1,HIGH);
              delay(sec);                   
             digitalWrite(device2,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-2 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
    if((rtc.getDay() == EEPROM.read(100)) && (rtc.getMonth() == EEPROM.read(101)) && (rtc.getYear() > 2020) && (rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 2))      // device2 on    
         {
            Serial.println("Schedule-2 Successfully played......");
             long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
            /*   long unsigned sec = (((tdate - fdate)*86400) + 0 + ((thour - fhour)*3600) + ((tminutes - fminutes)*60))*1000  */
             digitalWrite(device2,HIGH);
              delay(sec);                     
             digitalWrite(device2,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-3 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
          if((rtc.getDay() == EEPROM.read(100)) && (rtc.getMonth() == EEPROM.read(101)) && (rtc.getYear() > 2020) && (rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 3))      // device3 on    
         {
            Serial.println("Schedule-3 Successfully played......");
             long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
             digitalWrite(device3,HIGH);
              delay(sec);
             digitalWrite(device3,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-4 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
         if((rtc.getDay() == EEPROM.read(100)) && (rtc.getMonth() == EEPROM.read(101)) && (rtc.getYear() > 2020) && (rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 4))      // device4 on    
         {
            Serial.println("Schedule-4 Successfully played......");
             long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
             digitalWrite(device4,HIGH);
              delay(sec);
             digitalWrite(device4,LOW);
              schedule_erase();
         }
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-5 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
          if((rtc.getDay() == EEPROM.read(100)) && (rtc.getMonth() == EEPROM.read(101)) && (rtc.getYear() > 2020) && (rtc.getHour(true) == EEPROM.read(102)) && (rtc.getMinute() == EEPROM.read(103)) && (EEPROM.read(108) == 5))      // device5 on    
         {
            Serial.println("Schedule-5 Successfully playing......");
            long unsigned sec = (((EEPROM.read(104) - EEPROM.read(100))*86400) + 0 + ((EEPROM.read(106) - EEPROM.read(102))*3600) + ((EEPROM.read(107) - EEPROM.read(103))*60))*1000;
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

/******************************************* remote server functionalities *************************************/
void connectToServer() {
    if (!mqttClient.connected()) {                  
        if (mqttClient.connect(ThingName)) {
            Serial.println("Connected To AWS");
            int qos = 0;                              //Make sure the qos is zero in the MQTT broker of AWS
            mqttClient.subscribe(subTopic, qos);
            Serial.println("Subscribed To " + String(subTopic));
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
}
void handle_Status()
{
  server.send(200, "text/html", switchFeedback(device1State,device2State,device3State,device4State,device5State,sliderString));
}
void handle_Codeversion()
{
  server.send(200, "text/html", sendCodeVersion(codeVersion));
}
String sendCodeVersion(String codeVersion)
{
    String response = "<!DOCTYPE html> <html>\n";
          response +="<body>";
          response += "{";
          response += "codeVersion:";
          response += codeVersion;
          response += "}";
          response +="</body>\n";
          response +="</html>\n";
           char PayLoad[100];
            response.toCharArray(PayLoad,sizeof(PayLoad));
     Serial.println(PayLoad);
  return PayLoad;
}
void handle_device1On() {
  device1State = "ON";
  device1PreviousState = HIGH;
  digitalWrite(device1,device1PreviousState);
  Serial.println("Device1 Status: ON");
    EEPROM.write(device1StateAddress,device1PreviousState);
    EEPROM.commit();
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device1Off() {
  device1State = "OFF";
  device1PreviousState = LOW;
  digitalWrite(device1,device1PreviousState);
  Serial.println("Device1 Status: OFF");
    EEPROM.write(device1StateAddress,device1PreviousState);
    EEPROM.commit();
   server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device2On() {
  device2State = "ON";
  device2PreviousState = HIGH;
  digitalWrite(device2,device2PreviousState);
  Serial.println("Device2 Status: ON");
    EEPROM.write(device2StateAddress,device2PreviousState);
    EEPROM.commit();  
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device2Off() {
  device2State = "OFF";
  device2PreviousState = LOW;
  digitalWrite(device2,device2PreviousState);
  Serial.println("Device2 Status: OFF");
    EEPROM.write(device2StateAddress,device2PreviousState);
    EEPROM.commit();  
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device3On() {
  device3State = "ON";
  device3PreviousState = HIGH;
  digitalWrite(device3,device3PreviousState);
  Serial.println("Device3 Status: ON");
    EEPROM.write(device3StateAddress,device3PreviousState);
    EEPROM.commit();  
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device3Off() {
  device3State = "OFF";
   device3PreviousState = LOW;
  digitalWrite(device3,device3PreviousState);
  Serial.println("Device3 Status: OFF");
    EEPROM.write(device3StateAddress,device3PreviousState);
    EEPROM.commit();  
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}
void handle_device4On() {
  device4State = "ON";
  device4PreviousState = HIGH;
  digitalWrite(device4,device4PreviousState);
  Serial.println("Device4 Status: ON");
    EEPROM.write(device4StateAddress,device4PreviousState);
    EEPROM.commit();    
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device4Off() {
  device4State = "OFF";
  device4PreviousState = LOW;
  digitalWrite(device4,device4PreviousState);
  Serial.println("Device3 Status: OFF");
    EEPROM.write(device4StateAddress,device4PreviousState);
    EEPROM.commit();      
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device5On() {
  device5State = "ON";
  device5PreviousState = HIGH; 
  digitalWrite(device5,device5PreviousState);
  Serial.println("Device5 Status: ON");
    EEPROM.write(device5StateAddress,device5PreviousState);
    EEPROM.commit();      
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}

void handle_device5Off() {
  device5State = "OFF";
    device5PreviousState = LOW;
  digitalWrite(device5,device5PreviousState);
  Serial.println("Device5 Status: OFF");
    EEPROM.write(device5StateAddress,device5PreviousState);
    EEPROM.commit();      
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}
void handle_allOff()
{
  device1State = "OFF";
  device2State = "OFF";
  device3State = "OFF";
  device4State = "OFF";
  device5State = "OFF";
  sliderString = "";        // added
  digitalWrite(device1,LOW);
  digitalWrite(device2,LOW);
  digitalWrite(device3,LOW);
  digitalWrite(device4,LOW);
  digitalWrite(device5,LOW);
  dimmer.setPower(0);       // added
  Serial.println("All Off Request Received");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}
void handle_allOn()       // added
{
  device1State = "ON";
  device2State = "ON";
  device3State = "ON";
  device4State = "ON";
  device5State = "ON";
  sliderString = "";
  digitalWrite(device1,HIGH);
  digitalWrite(device2,HIGH);
  digitalWrite(device3,HIGH);
  digitalWrite(device4,HIGH);
  digitalWrite(device5,HIGH);
  dimmer.setPower(EEPROM.read(dimmerAddress));
  Serial.println("All Off Request Received");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,sliderString)); 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(String device1State,String device2State,String device3State,String device4State,String device5State,String sliderString){
  String response = "<!DOCTYPE html> <html>\n";
          response +="<body>";
          response += "{";
          response += "deviceId:";
          response += ThingName;
          response += ",";
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
               response += ",";
               response += "sliderValue:";
               response += sliderString;
               response += "}";
            char PayLoad[150];
            response.toCharArray(PayLoad,sizeof(PayLoad));
  response +="</body>\n";
  response +="</html>\n";
   Serial.println(PayLoad);
  return PayLoad;
}
