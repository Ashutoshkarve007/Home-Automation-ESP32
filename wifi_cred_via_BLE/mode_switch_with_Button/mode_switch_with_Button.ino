/*  Functionalities working 
 1. Scheduling for both Internet & without internet mode
 2. manual switch operations 
 3. accessing devices through local server
 4. status led's according to wifi mode


  Tasks pending
 1. dimmer integration
 2. mode switch integration
 3. scene creation

*/
 
/* Headers  */
#include <Wire.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ESP32Ping.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>  
#include <RBDdimmer.h>
#include "RTClib.h" 

/*    GPIO Configurations       */
#define device1 12                
#define device2 14    
#define device3 27  
#define device4 26
#define device5 25

#define switchPin1 23
#define switchPin2 19
#define switchPin3 18
#define switchPin4 5
#define switchPin5 17

#define internetStatusLed 33        // blue LED
#define wifiStatusLed 32         // orange LED

#define zcPin 16     
#define dimmerPin 4    

#define bootPin 0

#define SDA 21            
#define SCL 22

#define RTC_INT 15
#define RTC_RST 2

#define WIFI_TIMEOUT_MS 20000 // 20 second WiFi connection timeout
#define WIFI_RECOVER_TIME_MS 30000 // Wait 30 seconds after a failed connection attempt

#define EEPROM_SIZE 512

String valueString = String(0);
int positon1 = 0;
int positon2 = 0;
int outVal = 0;
int buf;
/*   Multifunctional switch variables   */
int buttonStatePrevious = LOW;                      // previousstate of the switch
unsigned long minButtonLongPressDuration = 3000;    // Time we wait before we see the press as a long press
unsigned long buttonLongPressMillis;                // Time in ms when we the button was pressed
bool buttonStateLongPress = false;                  // True if it is a long press
const int intervalButton = 50;                      // Time between two readings of the button state
unsigned long previousButtonMillis;                 // Timestamp of the latest reading
unsigned long buttonPressDuration;                  // Time the button is pressed in ms
unsigned long currentMillis;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


/*  Amazon Web Server Credentials  */  
char *TopicName = "COCA0x02";
char *ThingName = "COCA0x02";
const char *endpoint = "a1z3mi6l817ak1-ats.iot.ap-south-1.amazonaws.com";
const int port = 8883;

/*  State variables for WiFiServer  */      
String device1State = "OFF";
String device2State = "OFF";
String device3State = "OFF";
String device4State = "OFF";
String device5State = "OFF";

/*   Manual switch-1 variables   */
bool switch1PreviousState = LOW;              // previousstate of the switch
const int switchInterval = 50;               // Time between two readings of the button state
unsigned long switch1PreviousMillis;        // Timestamp of the latest reading
unsigned long switch1PressDuration;        // Time the button is pressed in ms
unsigned long currentMillisSwitch1;       // Variabele to store the number of milleseconds since the Arduino has started

/*   Manual switch-2 variables   */
bool switch2StatePrevious = LOW;                      
unsigned long switch2PreviousMillis;                 
unsigned long switch2PressDuration;                  
unsigned long currentMillisSwitch2;          

/*   Manual switch-3 variables   */
bool switch3StatePrevious = LOW;             
unsigned long switch3PreviousMillis;                 
unsigned long switch3PressDuration;                  
unsigned long currentMillisSwitch3;          

/*   Manual switch-4 variables   */
bool switch4StatePrevious = LOW;                       
unsigned long switch4PreviousMillis;                  
unsigned long switch4PressDuration;                  
unsigned long currentMillisSwitch4;         

/*   Manual switch-5 variables   */
bool switch5StatePrevious = LOW;            
unsigned long switch5PreviousMillis;        
unsigned long switch5PressDuration;         
unsigned long currentMillisSwitch5;         

String header;                               // handle request from the client

bool blockflag = false;
bool netflag = false;

char rcvdPayload[512];          /*  To receive the json packets from aws   */
String ssid[1];
String pass[1];
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

/*         Function prototypes       */
void sendDeviceState();
void subscribeFromAWS();
void localMode();
void initialise_rtc();
void connectAWSIoT();
void mqttCallback (char *,byte *,unsigned int);
void mqttLoop();
//void readButtonState();
void bleTask();
void wifiTask();
void manualSwitch();
void scheduleTask();
void schedule_erase();

const char* rootCA = \
{"-----BEGIN CERTIFICATE-----\n\
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n\
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n\
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n\
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n\
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n\
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n\
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n\
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n\
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n\
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n\
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n\
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n\
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n\
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n\
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n\
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n\
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n\
rqXRfboQnoZsG4q5WTP468SQvvG5\n\
-----END CERTIFICATE-----\n"};

const char* certificate = \
{"-----BEGIN CERTIFICATE-----\n\
MIIDWTCCAkGgAwIBAgIUR8KLlpXd7nvJ8/WOK9nY6SPHcxAwDQYJKoZIhvcNAQEL\n\
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n\
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTIwMDMzMDExMjA1\n\
M1oXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n\
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANxiVZH9RYoECpU7hOY7\n\
+n2h7oaM0wZS8dMa6bvG0MKNOOxj2RtF2dVkbryPDoxSglz60bBEkDwVwbvgzW1n\n\
kczAI2fZJ1udyQtFPE/7wO2kaptFNnfgCOtQkRaDq6fz8ySS7jmStTQ480Q5yQRX\n\
+rnuM30x8l58xWFi5v6Z2pplb+GkhfdC63vdv2UMp+EkFzPp7XRjmF1JE2DubJ/Q\n\
2IFalpJGrhNKY0goz7O0BXmZaqm7rUfwk2ta2JX8NGr4KsLauGCBPfuCOR9zadCK\n\
3BJWm5BUoAGzynXiIuHvYl0dMGLA03DLsUfJkn8ob+ZtmBPA4vY7ycb8cd5VmGjg\n\
GTcCAwEAAaNgMF4wHwYDVR0jBBgwFoAU5p3CnnNMxwIT7OJevgKNROuOQcMwHQYD\n\
VR0OBBYEFJgps6VF3ieqtnnquLU8OLjOH0+OMAwGA1UdEwEB/wQCMAAwDgYDVR0P\n\
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQCQCSPQ2QoQ7fm4YQjWO4yDjgWP\n\
WCG5UIAAWcbt7uVWZUeV8K7B+LvqJ70V1aJlGSQQ6sT/5brBF4898Wx1bhnW6U+r\n\
f+iRk2rP256TMjKRAPDL4eUaSdjPvkCL/Vgk4QEzkC8CWQcacbtMqO2LsqEydL/t\n\
Jzf+dQ9JYd/dQYFg3kXAcx1CLLXExObwoGR3SlssJiYl3v3qtzuYnFGGkxkobcD6\n\
8CvrUiVUiqocZTDNdTPvI0UVgPHAYNDvNLdzex7o3VW9l+X26Qu2nPJWERyD+hw+\n\
yP9m4NdlITXVt+Y3FsuxfUNz5BESZUMK3W8bo5b7Dq5FdUdFrkIkGMpITyDz\n\
-----END CERTIFICATE-----\n"};

const char* privateKey =  \
{"-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpAIBAAKCAQEA3GJVkf1FigQKlTuE5jv6faHuhozTBlLx0xrpu8bQwo047GPZ\n\
G0XZ1WRuvI8OjFKCXPrRsESQPBXBu+DNbWeRzMAjZ9knW53JC0U8T/vA7aRqm0U2\n\
d+AI61CRFoOrp/PzJJLuOZK1NDjzRDnJBFf6ue4zfTHyXnzFYWLm/pnammVv4aSF\n\
90Lre92/ZQyn4SQXM+ntdGOYXUkTYO5sn9DYgVqWkkauE0pjSCjPs7QFeZlqqbut\n\
R/CTa1rYlfw0avgqwtq4YIE9+4I5H3Np0IrcElabkFSgAbPKdeIi4e9iXR0wYsDT\n\
cMuxR8mSfyhv5m2YE8Di9jvJxvxx3lWYaOAZNwIDAQABAoIBADWiwQysxVXXsfOr\n\
7qZSBp644GJit6EcYrpsHGKU+o2+7RGrI1Wd5Gwo60J81p+UHSIf8RSjOy9EZEgj\n\
aBuuTy+zu2o00X1co2dYzFry/HtZvpBXgfAe2Ezc6NK/7PENUCmgkNX2PJ+fFKWQ\n\
Irop512E+YcItuIEH82Z0no4W5mjjr713Yt9kN5aeCSnjMFTa13FoQNoPIdU88Xi\n\
PLfqCuy9tgBb+H01JRXB0JPpr5N2ecvrG2tYYkIGpFfq864gNwBkt1lYd/dzvFOk\n\
19iLjc4yq/pG51e/FyNT6mvQy/jwtbOMENMhIjTZZaTabikfmRzPbNmwlb3nRNVx\n\
JALvZbECgYEA7qLip2wFXIy1POTw4ByaarhV2Qz4vuVTeT+s5cUvsAanNCC6WlPA\n\
GN05vIjQtAljgDrPBZMN4zYuOn+P75kWvQPRtTeV/chszNECzroS2gPAcrHoZ6xU\n\
rnKHetKOzT71nLZcDVtdCAQRzgc9BwqamM07EbMnzNu6YPfI5HULD2sCgYEA7Gt2\n\
lgHt+dRdd6HqQlAK/qwpjuoLrE91Bcfrrk2JU4Njw4u+Eaq2hS3M6i4HkUysuxxx\n\
uLZlumi2g7loU7J0wEQxHBnNeQS0GTVqOd2g9mBe2RWWyoo72V27UIbeAHcoZrJe\n\
OCYoPmchf3sYp585AhgAMHQi7HnMoZjudQjeDGUCgYBYZvL7/qJF1MJXGqC5BR+X\n\
JU54J7wGS8IOiq4sOE4gMXbDctRWHextyZZX538ZEtlPaXnDoORmJW4esZ7KrbWk\n\
s8N+FGd2KkdT4KHfn0LYjMdANfPYZBjCvx69Oz83fXlTYqLbN9tQ2uEVp8zNzPnU\n\
XULinbHCzCtRPLmpGErOnQKBgQDIqM0VtL2O9bf++eYiMl2ime0L54nQzf+80Fow\n\
ro6H21Spe8nupL4VezIY8MhrgnB4v0OmSuk8tfNzCcKKh0Sgi0BDYYML2/ogCz4F\n\
rx4W/uSBy9kYPwtdCjkZt95k9r3LrEhbz8cIb+/2izv5ySJRrJ08gtXBb/9GiM6b\n\
dtNAIQKBgQDg1L7izyE505tmsAxeqeSwHbawuK9eb3VGISpO94qZZxD9JxJ5jneZ\n\
PamXkCg7R8voCM/eVZsiITjMdMGgfLF7YaELp49jAAANNqFEmjMARH+jthl09vNr\n\
88ntXWXqVhPMB0NqOiekDXQzHk/a3rvbmBCbipa4ZWIaw9jYFt4n6Q==\n\
-----END RSA PRIVATE KEY-----\n"};

  /*      Instance creations   */
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);
WiFiMulti wifiMulti;
RTC_DS3231 rtc;
WiFiServer Server(80);         // server for local mode
dimmerLamp dimmer(dimmerPin, zcPin);

bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
      Serial.println("Client Connected Successfully");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Client Disconnected");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();

    if(value.length() > 0)
    {
      Serial.print("Received Data : ");
      Serial.println(value.c_str());
      writeString(0, value.c_str()); 
      ESP.restart(); 
    }
  }

  void writeString(int add, String data)
  {
    int _size = data.length();
    for(int i=0; i<_size; i++){
      EEPROM.write(add+i, data[i]);
    }
    EEPROM.write(add+_size, '\0');
    EEPROM.commit();
  }
};
void CurrentValue(int val)
{
  if ((val / 100) == 0) 
      Serial.print(" ");
  if ((val / 10) == 0) 
      Serial.print(" ");

}
void setup() {
  
  Serial.println("+++++++++++++++++++Welcome To Qurolabs+++++++++++++++++");
  delay(1000);
Serial.begin(115200);           //Initialising Serial Monitor
      
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

/*    status led's   */
pinMode(internetStatusLed,OUTPUT);    // Blue led : Status for Internet mode
pinMode(wifiStatusLed,OUTPUT);        // Orange led : Statud for wifi-Hotspot mode
pinMode(LED_BUILTIN, OUTPUT);
digitalWrite(LED_BUILTIN,LOW);

/*  manual switches   */

pinMode(switchPin1,INPUT_PULLDOWN);
pinMode(switchPin2,INPUT_PULLDOWN);
pinMode(switchPin3,INPUT_PULLDOWN);
pinMode(switchPin4,INPUT_PULLDOWN);
pinMode(switchPin5,INPUT_PULLDOWN);
pinMode(bootPin, INPUT);

  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(EEPROM_SIZE);       //Initialasing EEPROM
  delay(1000);
    for(int i = 104; i < 109; i++)
    {
      Serial.print("EEPROM Address : ");
      Serial.println(i);
      Serial.print("EEPROM value : ");
      Serial.println(EEPROM.read(i));
    }
    dimmer.begin(NORMAL_MODE, ON);
    dimmer.setState(OFF);
                  wifiTask();
                  wifiMulti.addAP(ssid[0].c_str(),pass[0].c_str());
                  
                  int retry = 0;
                  while(wifiMulti.run() != WL_CONNECTED && retry < 6)
                  {
                    Serial.println("*");
                    delay(300);
                      digitalWrite(internetStatusLed,HIGH);         // blue led blinking while connecting to internet
                   //   digitalWrite(wifiStatusLed,HIGH);
                      delay(100);
                      digitalWrite(internetStatusLed,LOW);
                   //   digitalWrite(wifiStatusLed,LOW);
                      delay(100);
                      digitalWrite(wifiStatusLed,HIGH);
                      retry++;
                  }
                  if(WiFi.status() != WL_CONNECTED)
                  {
                     //BLE MODE
                      Serial.println("BLE MODE");
                      bleTask();
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
                    connectAWSIoT();
                    initialise_rtc();
                    }
               xTaskCreate(keepWiFiAlive," keepWiFiAlive",10000,NULL,1,NULL);    
}
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
void connectAWSIoT() {
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
void mqttCallback (char* topic, byte *payload, unsigned int length) {
    strncpy(rcvdPayload,(char *)payload,length);
    Serial.print("\n");
    Serial.println(rcvdPayload);
    subscribeFromAWS();    
}
void mqttLoop() {
    if (!mqttClient.connected()) {
        connectAWSIoT();
        vTaskDelay(10000 / portTICK_RATE_MS);
    }else
      {
          mqttClient.loop();    
      }
}
void readButtonState() {
       
  if(currentMillis - previousButtonMillis > intervalButton) {
    
    // Read the digital value of the button (LOW/HIGH)
    int buttonState = !digitalRead(bootPin);    

    if (buttonState == HIGH && buttonStatePrevious == LOW && !buttonStateLongPress) {
      buttonLongPressMillis = currentMillis;
      buttonStatePrevious = HIGH;
      Serial.println("Button pressed");
    }
    buttonPressDuration = currentMillis - buttonLongPressMillis;

    if (buttonState == HIGH && !buttonStateLongPress && buttonPressDuration >= minButtonLongPressDuration) {
      buttonStateLongPress = true;
      Serial.println("Button long pressed");
        bleTask();
    }
    if (buttonState == LOW && buttonStatePrevious == HIGH) {
      buttonStatePrevious = LOW;
      buttonStateLongPress = false;
      Serial.println("Button released");

      if (buttonPressDuration < minButtonLongPressDuration) {
        Serial.println("Button pressed shortly");
      }
    }
    
    // store the current timestamp in previousButtonMillis
    previousButtonMillis = currentMillis;

  }
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

void bleTask()
{
  // Create the BLE Device
  BLEDevice::init("ESP32 THAT PROJECT");
  
  // Create the BLE Server 
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection to notify...");
                   digitalWrite(internetStatusLed,HIGH);         // blue led blinking while wifi configuration
                   digitalWrite(wifiStatusLed,HIGH);
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
                             WiFi.softAP("COCA0x01", "12345678");               
                               Server.begin();
                                Serial.println("localmode Started.....");
                                  blockflag = true;
                    }
                    if(blockflag == true)
                      {
                           while(1)
                            {                           
                                localMode();
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

String getValue(String data, char separator, int index){
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found <=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
      found++;
      strIndex[0] = strIndex[1]+1;
      strIndex[1] = (i==maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
void wifiTask() 
{
                  String receivedData;
                  receivedData = EEPROM.readString(0);
                      if(receivedData.length() > 0)
                        {
                             String wifiName = getValue(receivedData, ',', 0);
                             String wifiPassword = getValue(receivedData, ',', 1);
                                  if(wifiName.length() > 0 && wifiPassword.length() > 0)
                                    {
                                         Serial.print("WifiName : ");
                                         Serial.println(wifiName);
                                         Serial.print("wifiPassword : ");
                                         Serial.println(wifiPassword);
                                         ssid[0] = wifiName;
                                         pass[0] = wifiPassword;                                         
                                    }
                        }
} 
void subscribeFromAWS()
{
     StaticJsonDocument<256> doc;
     DeserializationError error = deserializeJson(doc,rcvdPayload);          
      if(error)
          return;
          
  char myData[6] = {
                        doc["device1"],doc["device2"],doc["device3"],doc["device4"],doc["device5"],doc["deviceId"]      // device1  && device2 && device3 && device4 && device5
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

    if(schedule[0] != 0){  
                  EEPROM.write(100,schedule[0]);      // address of DD -> 100
                  EEPROM.commit();
                  Serial.println("EEPROM date :");
                  Serial.println(byte(EEPROM.read(100)));  
                  }
                  if(schedule[1] != 0){
                  EEPROM.write(101,schedule[1]);        // address of MM -> 101
                  EEPROM.commit();  
                  Serial.println("EEPROM month :");
                  Serial.println(byte(EEPROM.read(101)));
                  }
                  if(schedule[2] != 0){
                  EEPROM.write(102,schedule[2]);        // address of hh -> 102
                  EEPROM.commit();  
                  Serial.println("EEPROM hours :");
                  Serial.println(byte(EEPROM.read(102)));
                  }
                  if(schedule[3] != 0){
                  EEPROM.write(103,schedule[3]);        // address of mm -> 103
                  EEPROM.commit();  
                  Serial.println("EEPROM minutes :");
                  Serial.println(byte(EEPROM.read(103)));
                  }
                  
                  if(schedule[4] != 0){
                  EEPROM.write(104,schedule[4]);        // address of schedule1 -> 104
                  EEPROM.commit();  
                  Serial.println("EEPROM schedule1 :");
                  Serial.println(byte(EEPROM.read(104)));
                  }
                  
                  if(schedule[5] != 0){
                  EEPROM.write(105,schedule[5]);        // address of schedule2 -> 105
                  EEPROM.commit();  
                  Serial.println("EEPROM schedule2 :");
                  Serial.println(byte(EEPROM.read(105)));
                  }

                  if(schedule[6] != 0){
                  EEPROM.write(106,schedule[6]);        // address of schedule3 -> 106
                  EEPROM.commit();  
                  Serial.println("EEPROM schedule3 :");
                  Serial.println(byte(EEPROM.read(106)));                    
                  }

                  if(schedule[7] != 0){
                  EEPROM.write(107,schedule[7]);        // address of schedule4 -> 107
                  EEPROM.commit();  
                  Serial.println("EEPROM schedule4 :");
                  Serial.println(byte(EEPROM.read(107)));                    
                  }
                  
                  if(schedule[8] != 0){
                  EEPROM.write(108,schedule[8]);        // address of schedule5 -> 108
                  EEPROM.commit();  
                  Serial.println("EEPROM schedule5: ");
                  Serial.println(byte(EEPROM.read(108)));                                        
                  }

                  if(schedule[9] != 0){
                    EEPROM.write(109,schedule[9]);        // address of duration -> 109
                    EEPROM.commit();
                    Serial.println("EEPROM TaskDuration_In_Minutes :");
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
 memset(rcvdPayload,0,sizeof(rcvdPayload));
 memset(schedule,0,sizeof(schedule));
 
}
void sendDeviceState()
{    
 StaticJsonDocument<256> jsonDoc;

  jsonDoc["deviceId"] = "COCA0x01";
  jsonDoc["device1"] = device1State;
  jsonDoc["device2"] = device2State;
  jsonDoc["device3"] = device3State;
  jsonDoc["device4"] = device4State;
  jsonDoc["device5"] = device5State;
  jsonDoc["deviceStatus"] = "Online";
  serializeJson(jsonDoc, Serial);
  char Buffer[128];
  serializeJson(jsonDoc, Buffer);
  
  Serial.println("Publishing message to AWS...");
  //mqttClient.publish(TopicName, Buffer);
 vTaskDelay(5000 / portTICK_RATE_MS);
}
void scheduleTask()
{
  manualSwitch();  
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
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device1,HIGH);
              delay(sec);                   // schedule played
             digitalWrite(device1,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-2 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102) && now.minute() == EEPROM.read(103) && EEPROM.read(105) && EEPROM.read(109) != 0 )      // device1 on    
         {
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device2,HIGH);
              delay(sec);                     // schedule not played
             digitalWrite(device2,LOW);
              schedule_erase();
         } 
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-3 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102) && now.minute() == EEPROM.read(103) && EEPROM.read(106) && EEPROM.read(109) != 0 )      // device1 on    
         {
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
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device4,HIGH);
              delay(sec);
             digitalWrite(device4,LOW);
              schedule_erase();
         }
/*   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ Schedule For device-5 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */         
      if( now.day() == EEPROM.read(100) && now.month() == EEPROM.read(101) && now.year() >= 2020 && now.hour() == EEPROM.read(102)&& now.minute() == EEPROM.read(103) && EEPROM.read(108) && EEPROM.read(109) != 0 )      // relay1 on    
         {
            long unsigned sec = EEPROM.read(109)*60000;
             digitalWrite(device5,HIGH);
              delay(sec);
             digitalWrite(device5,LOW);
              schedule_erase();
         }          
         connectAWSIoT();  
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
void localMode()
{
  /*
  manualSwitch(); 
  WiFiClient client = Server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {
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
       
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Title of The Page
            client.println("<body><h1>Welcome To QuroLabs</h1>");
            
            
            // Display current state, and ON/OFF buttons all relays 
            client.println("<p>device1 - State " + device1State + "</p>");
            // If the green LED is off, it displays the ON button       
            if (device1State == "OFF") {
              client.println("<p><a href=\"/device1On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device1Off\"><button class=\"button button2\">OFF</button></a></p>");
            }  
           
            client.println("<p>device2 - State " + device2State + "</p>");

            if (device2State == "OFF") {
              client.println("<p><a href=\"/device2On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device2Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device3 - State " + device3State + "</p>");
            
            if (device3State == "OFF") {
              client.println("<p><a href=\"/device3On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device3Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device4 - State " + device4State + "</p>");

            if (device4State == "OFF") {
              client.println("<p><a href=\"/device4On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device4Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device5 - State" + device5State + "</p>");

            if (device5State == "OFF") {
              client.println("<p><a href=\"/device5On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device5Off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            client.println("</body></html>");
            
            client.println();
            break;
          } else { 
            currentLine = "";
          }
        } else if (c != '\r') {  
          currentLine += c;      
        }
      }
       manualSwitch();
    }
    header = "";
      
         StaticJsonDocument<128> jsonDoc;
          jsonDoc["device1"] = device1State;
          jsonDoc["device2"] = device2State;
          jsonDoc["device3"] = device3State;
          jsonDoc["device4"] = device4State;
          jsonDoc["device5"] = device5State;  
    Serial.println("Publishing status to Client...");
    serializeJson(jsonDoc, Serial); 
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
*/
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

// Display the HTML web page
client.println("<!DOCTYPE html><html>");
client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
client.println("<link rel=\"icon\" href=\"data:,\">");
// CSS to style the on/off buttons 
// Feel free to change the background-color and font-size attributes to fit your preferences
client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; margin-left:auto; margin-right:auto;}");
client.println(".slider { width: 300px; }</style>");
client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>");
client.println("<script src=\"jquery.min.js\"></script>");

  /*          client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
*/
// Web Page
client.println("</head><body><h1>Welcome To QuroLabs</h1>");
client.println("<p>Position: <span id=\"servoPos\"></span></p>"); 
client.println("<input type=\"range\" min=\"0\" max=\"100\" class=\"slider\" id=\"servoSlider\" onchange=\"servo(this.value)\" value=\""+valueString+"\"/>");

client.println("<script>var slider = document.getElementById(\"servoSlider\");");
client.println("var servoP = document.getElementById(\"servoPos\"); servoP.innerHTML = slider.value;");
client.println("slider.oninput = function() { slider.value = this.value; servoP.innerHTML = this.value; }");
client.println("$.ajaxSetup({timeout:1000}); function servo(pos) { ");
client.println("$.get(\"/?value=\" + pos + \"&\"); {Connection: close};}</script>");
            client.println("<p>device1 - State " + device1State + "</p>");
            // If the green LED is off, it displays the ON button       
            if (device1State == "OFF") {
              client.println("<p><a href=\"/device1On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device1Off\"><button class=\"button button2\">OFF</button></a></p>");
            }  
           
            client.println("<p>device2 - State " + device2State + "</p>");

            if (device2State == "OFF") {
              client.println("<p><a href=\"/device2On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device2Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device3 - State " + device3State + "</p>");
            
            if (device3State == "OFF") {
              client.println("<p><a href=\"/device3On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device3Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device4 - State " + device4State + "</p>");

            if (device4State == "OFF") {
              client.println("<p><a href=\"/device4On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device4Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p>device5 - State" + device5State + "</p>");

            if (device5State == "OFF") {
              client.println("<p><a href=\"/device5On\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/device5Off\"><button class=\"button button2\">OFF</button></a></p>");
            }
                  
client.println("</body></html>"); 

//GET /?value=180& HTTP/1.1
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

client.stop();
Serial.println("Client disconnected.");
Serial.println("");
        StaticJsonDocument<128> jsonDoc;
          jsonDoc["device1"] = device1State;
          jsonDoc["device2"] = device2State;
          jsonDoc["device3"] = device3State;
          jsonDoc["device4"] = device4State;
          jsonDoc["device5"] = device5State;  
    Serial.println("Publishing status to Client...");
    serializeJson(jsonDoc, Serial); 
  }
}
void dimmerOn()
{
  int preVal = outVal;
    if(buf != 0)
      {
        outVal = buf;
            if(outVal < 1)
              dimmer.setState(OFF);
           if(outVal > 1 )
              dimmer.setState(ON);             
        delay(200);
      }
      dimmer.setPower(outVal);
    if (preVal != outVal)
      {
        Serial.print("SliderValue -> ");
        CurrentValue(dimmer.getPower());
        Serial.print(dimmer.getPower());
        Serial.println("%");
        Serial.println(dimmer.getState());
      }
      delay(50);
}
void keepWiFiAlive(void *parameter){
   while(1)
   {   
        manualSwitch();
        if(WiFi.status() == WL_CONNECTED)
          {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
          }
            Serial.println(" Connecting To WiFi..... ");
             WiFi.begin(ssid[0].c_str(), pass[0].c_str());
              unsigned long startAttemptTime = millis();
        // Keep looping while we're not connected and haven't reached the timeout
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS);

        // If we couldn't connect within the timeout period, retry in 30 seconds.
        if(WiFi.status() != WL_CONNECTED){
            Serial.println("Failed to Connect to WiFi");
            vTaskDelay(WIFI_RECOVER_TIME_MS / portTICK_PERIOD_MS);
       continue;
        }
        Serial.println("Connected TO WiFi :" + WiFi.localIP());
       ESP.restart();     
   }
}

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
   


 
