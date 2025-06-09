#ifndef _DECLARATIONS_H
#define _DECLARATIONS_H

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

#define ota_url "https://raw.githubusercontent.com/Quro-Labs/otaUpdate/master/update.bin"

/*  Status LED's  */
#define internetStatusLed 33        // blue LED     LED2
#define wifiStatusLed 32         // orange LED      LED1

#define zcPin 16        // default is 16
#define dimmerPin 4    //default is 4
#define testPin 13
#define bootPin 0

#define SDA 21            
#define SCL 22

#define RTC_INT 15
#define RTC_RST 2

#define WIFI_TIMEOUT_MS 20000 // 20 second WiFi connection timeout
#define WIFI_RECOVER_TIME_MS 30000 // Wait 30 seconds after a failed connection attempt

#define EEPROM_SIZE 512
#define deviceAddress 400
#define passwordAddress 450
#define device1StateAddress 501
#define device2StateAddress 502
#define device3StateAddress 503
#define device4StateAddress 504
#define device5StateAddress 505
#define dimmerAddress 506

bool device1PreviousState = LOW;
bool device2PreviousState = LOW;
bool device3PreviousState = LOW;
bool device4PreviousState = LOW;
bool device5PreviousState = LOW;

String deviceIDString = "";
String codeVersion = "v2.5";

String date = "";
int Date;
String month = "";
int Month;
String year = "";
int Year;
String hour = "";
int Hour;
String minutes = "";
int Minutes;
String seconds = "";
int Seconds;
char ThingName[25] = "";

char pubTopic[30],subTopic[30];

char Password[20] = "";
String defaultPass = "123456789";

RTC_DATA_ATTR int bootCount = 0;

/* rtc configuration */
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19080+720;
const int   daylightOffset_sec = 00;

static int pubCounter = 0;
/* Variables for slider */
int sliderValue;
int outVal = 0;

/*   Multifunctional switch variables   */
int buttonStatePrevious = LOW;                      // previousstate of the switch
unsigned long minButtonLongPressDuration = 3000;    // waiting time for the long press detection
unsigned long buttonLongPressMillis;                // Time in ms when we the button was pressed
bool buttonStateLongPress = false;                  // True if it is a long press
const int intervalButton = 50;                      // Time between two readings of the button state
unsigned long previousButtonMillis;                 // Timestamp of the latest reading
unsigned long buttonPressDuration;                  // Time the button is pressed in ms
unsigned long currentMillis;

/*  State variables for WiFiServer  */      
String device1State = "OFF";
String device2State = "OFF";
String device3State = "OFF";
String device4State = "OFF";
String device5State = "OFF";
String sliderString = "";

bool switchPin1State = LOW;    
bool switchPin2State = LOW;    
bool switchPin3State = LOW;
bool switchPin4State = LOW;    
bool switchPin5State = LOW; 

/*   Manual switch-1 variables   */
bool switch1PreviousState = HIGH;              // previousstate of the switch
const int switchInterval = 50;               // Time between two readings of the button state
unsigned long switch1PreviousMillis;        // Timestamp of the latest reading
unsigned long switch1PressDuration;        // Time the button is pressed in ms
unsigned long currentMillisSwitch1;       // Variabele to store the number of milleseconds since the Arduino has started

/*   Manual switch-2 variables   */
bool switch2PreviousState = HIGH;                      
unsigned long switch2PreviousMillis;                 
unsigned long switch2PressDuration;                  
unsigned long currentMillisSwitch2;          

/*   Manual switch-3 variables   */
bool switch3PreviousState = HIGH;             
unsigned long switch3PreviousMillis;                 
unsigned long switch3PressDuration;                  
unsigned long currentMillisSwitch3;          

/*   Manual switch-4 variables   */
bool switch4PreviousState = HIGH;                       
unsigned long switch4PreviousMillis;                  
unsigned long switch4PressDuration;                  
unsigned long currentMillisSwitch4;         

/*   Manual switch-5 variables   */
bool switch5PreviousState = HIGH;            
unsigned long switch5PreviousMillis;        
unsigned long switch5PressDuration;         
unsigned long currentMillisSwitch5;         

/*      Variables for WiFiConfig local server   */
bool blockflag;       
bool netflag;
int i = 0;
int statusCode;
const char* ssid = "text";
const char* passphrase = "text";
String st;
String content;

String header;                               // handle request from the client

String esid = "";
String epass = "";

String newPass = "";
char rcvdPayload[512];          /*  To receive the json packets from aws   */

/*         Function prototypes       */
void sendDeviceState();
void subscribeFromAWS();
 bool checkWifi(void);
void OpenWeb(void);
void setupAP(void);
void OpenWebserver();
void localMode();
void initialise_rtc();
void connectAWSIoT();
void mqttCallback (char *,byte *,unsigned int);
void mqttLoop();
void readButtonState();
void manualSwitch();
void scheduleTask();
void schedule_erase();
void firmwareUpdate();
void auto_mode();
void configureDeviceId();
void handleLocalMode(); 
void switchFeedbackLocal();
void handle_Codeversion();
String sendCodeVersion(String codeVersion);
#endif
