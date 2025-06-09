
/*
  1. Wifi credentials can update from the local mode
  2. NTP time given to internal rtc
  3. Integrated for the android application
  Status Indecators :
  lcoal Mode indecation - yellow led stable
  internet mode indication - blue led on
  command received to the device in localmode - yellow led blink
  command received to the device via internet - blue led blink
  *** manualswitch functionality is disabled for android developers ***(scheduleTask)
 */
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
#include "certificates.h"
#include "declarations.h"
#include "internal_rtc.h"

WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);
dimmerLamp dimmer(dimmerPin, zcPin);
WebServer server(80);
internal_rtc rtc;


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

  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(EEPROM_SIZE);       //Initialasing EEPROM
  delay(1000);
    dimmer.begin(NORMAL_MODE, ON);
    dimmer.setState(OFF);
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
                    initialise_rtc();
                    }
               xTaskCreate(keepWiFiAlive," keepWiFiAlive",10000,NULL,1,NULL);    

      server.on("/", []() {
          request = server.arg("value");
          sliderValue = request.toInt();
          dimmerOn();
      digitalWrite(wifiStatusLed,LOW);
      delay(100);
      digitalWrite(wifiStatusLed,HIGH);
      server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 

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
  server.onNotFound(handle_NotFound);
}
/******************************************** RTC initialisation ***********************************/
void initialise_rtc()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
  if (!getLocalTime(&timeinfo)){
  Serial.println("Failed to Obtain the Time");
  return;
  }
}
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
                             WiFi.softAP(ThingName, Password);               
                                //server.begin();
                                          OpenWeb();
                                          setupAP();
                                   Serial.println("HTTP server started");
                                  blockflag = true;
                    }
                    if(blockflag == true)
                      {
                           while(1)
                            {         
                                server.handleClient();                  
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
  WiFi.softAP(ThingName, Password);
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
        //server.begin();
              digitalWrite(internetStatusLed,HIGH);
              digitalWrite(wifiStatusLed,HIGH);
        while(1)
          {
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
 /********************************************* AC dimmer functionality *******************************/
void dimmerOn()
{
  int preVal = outVal;
    if(sliderValue != 0 || sliderValue == 0)
      {
        outVal = sliderValue;
            if(outVal <= 5)
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

/******************************************************* schedule devices ************************************/
void scheduleTask()
{
 // manualSwitch();  
    Serial.println(rtc.getTimeDate());
    delay(300);
    
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-1 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */
      if( rtc.getDay() == EEPROM.read(100) && rtc.getMonth() == EEPROM.read(101) && rtc.getYear() > 2020 && rtc.getHour(true) == EEPROM.read(102) && rtc.getMinute() == EEPROM.read(103) && EEPROM.read(104) && EEPROM.read(109) != 0 )      // device1 on    
         {
            Serial.println("Schedule-1 Successfully played......");
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device1,HIGH);
              delay(sec);                   
             digitalWrite(device1,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-2 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( rtc.getDay() == EEPROM.read(100) && rtc.getMonth() == EEPROM.read(101) && rtc.getYear() > 2020 && rtc.getHour(true) == EEPROM.read(102) && rtc.getMinute() == EEPROM.read(103) && EEPROM.read(105) && EEPROM.read(109) != 0 )      // device1 on    
         {
            Serial.println("Schedule-2 Successfully played......");
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device2,HIGH);
              delay(sec);                     
             digitalWrite(device2,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-3 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( rtc.getDay() == EEPROM.read(100) && rtc.getMonth() == EEPROM.read(101) && rtc.getYear() > 2020 && rtc.getHour(true) == EEPROM.read(102) && rtc.getMinute() == EEPROM.read(103) && EEPROM.read(106) && EEPROM.read(109) != 0 )      // device1 on    
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
      if( rtc.getDay() == EEPROM.read(100) && rtc.getMonth() == EEPROM.read(101) && rtc.getYear() > 2020 && rtc.getHour(true) == EEPROM.read(102) && rtc.getMinute() == EEPROM.read(103) && EEPROM.read(107) && EEPROM.read(109) != 0 )      // relay1 on    
         {
            Serial.println("Schedule-4 Successfully played......");
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device4,HIGH);
              delay(sec);
             digitalWrite(device4,LOW);
              schedule_erase();
         }
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-5 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( rtc.getDay() == EEPROM.read(100) && rtc.getMonth() == EEPROM.read(101) && rtc.getYear() > 2020 && rtc.getHour(true) == EEPROM.read(102)&& rtc.getMinute() == EEPROM.read(103) && EEPROM.read(108) && EEPROM.read(109) != 0 )      // relay1 on    
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
}

/****************************************** functionality when device is connected to Server **********************************************/
void with_internet()
{
     StaticJsonDocument<256> doc;
     DeserializationError error = deserializeJson(doc,rcvdPayload);          
      if(error)
          return;
          
  char myData[8] = {
                        doc["device1"],doc["device2"],doc["device3"],doc["device4"],doc["device5"],doc["deviceId"],doc["sliderValue"],doc["firmwareUpdate"]     // device1  && device2 && device3 && device4 && device5
                   };
  char schedule[11] =  {  
                          doc["date"],doc["month"],doc["hour"],doc["minutes"],doc["sch1"],doc["sch2"],doc["sch3"],doc["sch4"],doc["sch5"],doc["duration"],doc["allOff"]                  
                       };      
    digitalWrite(internetStatusLed,LOW);
    delay(100);
    digitalWrite(internetStatusLed,HIGH);
    
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
          sliderValue = myData[6];
          dimmerOn();
                digitalWrite(wifiStatusLed,HIGH);
                 delay(100);
                digitalWrite(wifiStatusLed,LOW);
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
/* memset(rcvdPayload,0,sizeof(rcvdPayload));
 memset(schedule,0,sizeof(schedule)); */
}
void sendDeviceState()
{    
 StaticJsonDocument<512> jsonDoc;
 //  JsonObject stateObj = jsonDoc.createNestedObject("state");
 // JsonObject reportedObj = stateObj.createNestedObject("reported");
  
  jsonDoc["deviceId"] = ThingName;
  jsonDoc["device1"] = digitalRead(device1);
  jsonDoc["device2"] = digitalRead(device2);
  jsonDoc["device3"] = digitalRead(device3);
  jsonDoc["device4"] = digitalRead(device4);
  jsonDoc["device5"] = digitalRead(device5);
  jsonDoc["sliderValue"] = sliderValue;
  jsonDoc["deviceStatus"] = "Online";
//  serializeJson(jsonDoc, Serial);
  char Buffer[200];   //160 for normanl json
  serializeJsonPretty(jsonDoc, Buffer,sizeof(Buffer));
  
  Serial.println("Publishing message to AWS...");
  mqttClient.publish(TopicName, Buffer,sizeof(Buffer));
 // vTaskDelay(1000 / portTICK_RATE_MS);      // publish the state every 1 minute
  
    /*     int state = digitalRead(device1);   
           int state1 = digitalRead(device2);
               int state2 = digitalRead(device3);
                   int state3 = digitalRead(device4);
                       int state4 = digitalRead(device5);
                       String relay_state = "{";
                               relay_state += "device1:";
                                  relay_state += String(state);
                                      relay_state += ", ";
                                          relay_state += "device2:";
                                              relay_state += String(state1);
                                                  relay_state += ", ";
                                                       relay_state += "device3:";
                                                            relay_state += String(state2);
                                                                 relay_state += ", ";
                                                            relay_state += "device4:";
                                                        relay_state += String(state3);
                                                      relay_state += ", ";                 
                                                  relay_state += "device5:";
                                              relay_state += String(state4);
                                           relay_state += "}";
                            char payload[100];
                         relay_state.toCharArray(payload,sizeof(payload));
                     Serial.println("Publishing : ");
                 Serial.print(payload);                           
           if(mqttClient.publish(TopicName, payload) == 0)
          {
              Serial.println("Success\n");
          }
         else
       Serial.println("Failed\n");
*/
}
void handle_device1On() {
  device1State = "ON";
  digitalWrite(device1,HIGH);
  Serial.println("Device1 Status: ON");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device1Off() {
  device1State = "OFF";
  digitalWrite(device1,LOW);
  Serial.println("Device1 Status: OFF");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device2On() {
  device2State = "ON";
  digitalWrite(device2,HIGH);
  Serial.println("Device2 Status: ON");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device2Off() {
  device2State = "OFF";
  digitalWrite(device2,LOW);
  Serial.println("Device2 Status: OFF");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device3On() {
  device3State = "ON";
  digitalWrite(device3,HIGH);
  Serial.println("Device3 Status: ON");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device3Off() {
  device3State = "OFF";
  digitalWrite(device3,LOW);
  Serial.println("Device3 Status: OFF");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}
void handle_device4On() {
  device4State = "ON";
  digitalWrite(device4,HIGH);
  Serial.println("Device4 Status: ON");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device4Off() {
  device4State = "OFF";
  digitalWrite(device4,LOW);
  Serial.println("Device3 Status: OFF");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device5On() {
  device5State = "ON";
  digitalWrite(device5,HIGH);
  Serial.println("Device5 Status: ON");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}

void handle_device5Off() {
  device5State = "OFF";
  digitalWrite(device5,LOW);
  Serial.println("Device5 Status: OFF");
  server.send(200, "text/html", SendHTML(device1State,device2State,device3State,device4State,device5State,request)); 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(String device1State,String device2State,String device3State,String device4State,String device5State,String request){
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
               response += request;
               response += "}";
            char PayLoad[150];
            response.toCharArray(PayLoad,sizeof(PayLoad));
            
  response +="</body>\n";
  response +="</html>\n";
  return PayLoad;

/*  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>LED Control</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
 */
    /*
  ptr +="<h1>ESP32 Web Server</h1>\n";
  ptr +="<h3>Using Access Point(AP) Mode</h3>\n";
  
   if(led1stat)
  {ptr +="<p>LED1 Status: ON</p><a class=\"button button-off\" href=\"/led1off\">OFF</a>\n";}
  else
  {ptr +="<p>LED1 Status: OFF</p><a class=\"button button-on\" href=\"/led1on\">ON</a>\n";}

  if(led2stat)
  {ptr +="<p>LED2 Status: ON</p><a class=\"button button-off\" href=\"/led2off\">OFF</a>\n";}
  else
  {ptr +="<p>LED2 Status: OFF</p><a class=\"button button-on\" href=\"/led2on\">ON</a>\n";}
*/
    
  
}
