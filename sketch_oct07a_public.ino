#include <WiFi.h>                   // For connecting to local network
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>       // For interface with the Blynk server and iOS applet
#include <time.h>                   // For acquisition and handling of time info
#include <Stepper.h>                // For control of door motor
#include <sunset.h>                 // For calculation of sunset & sunrise times
#include <Wire.h>                   // Used to establied serial communication on the I2C bus
#include <SparkFunTMP102.h>         // For communication and handling of TMP102 temp sensor data
#include <ESPmDNS.h>                // Used for OTA updating
#include <WiFiUdp.h>                // Used for OTA updating
#include <ArduinoOTA.h>             // Used for OTA updating

const char  auth[] = "";    // Get Auth Token in the Blynk App
const char* ssid = "";
const char* pass = "";
const char* ntpServer = "pool.ntp.org";
long        gmtOffset_sec = ;
long        daylightOffset_sec = 0;                         // Will need dynamic adjusting somehow
char        timeStringBuff[50];                             // Create buffer to hold string from time values
const int   IoTRelay = 26;                                  // Heat lamp relay control on pin A0
const int   doorSwitch = 25;                                // Door Reed Switch on pin A1
const int   DVR8833_SLP = 19;
const int   DVR8833_FLT = 21;
const int   stepsPerRevolution = 400;
int         timezone = -7;                                  // 
const float latitude = ;
const float longitude = ;
String      doorState = "Door state?";
int         door;                                           // Reed switch circuit, equate ">=1" to closed and "0" to open
float       interiorTemp;
float       exteriorTemp;
struct tm   timeinfo;
double      sunrise;
double      sunset;
double      todayMinutes;

BlynkTimer timer;
TMP102 interiorTMP102;
TMP102 exteriorTMP102;
Stepper doorMotor(stepsPerRevolution, 5, 18, 16, 17);
SunSet sun;

void setup()
{
  Wire.begin();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);                                      //connect to WiFi
  // ArduinoOTA.setPort(3232);                                 // Port defaults to 3232
  ArduinoOTA.setHostname("");                                  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setPassword("admin");                          // No authentication by default
  ArduinoOTA.setPasswordHash("");                              // Password can be set with it's md5 value
  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    //Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Blynk.begin(auth, ssid, pass);
  pinMode(IoTRelay, OUTPUT);
  pinMode(doorSwitch, INPUT);
  pinMode(DVR8833_SLP, OUTPUT);
  pinMode(DVR8833_FLT, INPUT);                                             // This will drive low if there's a thermal shutdown or overcurrent
  timer.setInterval(30000L, myTimerEvent);                                 // Blynk sends data every 5 minutes (300000L seconds)
  interiorTMP102.begin(0x48);
  exteriorTMP102.begin(0x49);
  interiorTMP102.setConversionRate(2);                                     //(how quickly the sensor gets a new reading) 0-3: 0:0.25Hz, 1:1Hz, 2:4Hz, 3:8Hz
  exteriorTMP102.setConversionRate(2);
  interiorTMP102.setExtendedMode(0);                                       //0:12-bit Temperature(-55C to +128C) 1:13-bit Temperature(-55C to +150C)
  exteriorTMP102.setExtendedMode(0);
  doorMotor.setSpeed(60); // sets stepper speed at 60 RPM
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);                //init and get the time
  sun.setPosition(latitude, longitude, timezone);
  digitalWrite(DVR8833_SLP, HIGH);                                         // This will enable the stepper motor driver.
}

void SunActivity() {
  getLocalTime(&timeinfo);
  todayMinutes = (timeinfo.tm_hour * 60 + timeinfo.tm_min);
  sun.setCurrentDate((timeinfo.tm_year + 1900), (timeinfo.tm_mon + 1), timeinfo.tm_mday);
  if (timeinfo.tm_isdst > 0) {                                            // NTP server reports whether DST in effect
    timezone = -6;
  } else {
    timezone = -7;
  }
  sun.setTZOffset(timezone);
  sunrise = sun.calcSunrise();
  sunset = sun.calcSunset();
  strftime(timeStringBuff, sizeof(timeStringBuff), "%c", &timeinfo);
  // String asString(timeStringBuff);                                     // Optional: Construct string object
}

void myTimerEvent()                                                       // In the app, Widget's reading frequency should be set to PUSH
{
  if (WL_IDLE_STATUS != WL_CONNECTED) {
    Blynk.disconnect();
    WiFi.disconnect(true);
    WiFi.begin(ssid, pass);
    Blynk.connect();
  }
  door = digitalRead(doorSwitch);
  interiorTMP102.wakeup();
  exteriorTMP102.wakeup();
  SunActivity();
  interiorTemp = interiorTMP102.readTempF();
  exteriorTemp = exteriorTMP102.readTempF();
  if (door == HIGH) {                                                     // If magnet present, reed switch closed, door = LOW. If no magnet, switch open, door = HIGH
    doorState = "OPEN";
  } else {
    doorState = "CLOSED";
  }
  if (exteriorTemp < 45) {                                                // If outside temp less than 40F, turn on heat lamps.
    digitalWrite(IoTRelay, HIGH);                                         // Writing IoTRelay HIGH turns on heat lamps.
  } else {
    digitalWrite(IoTRelay, LOW);                                          // Writing IoTRelay LOW turns heat lamps off.
  }
  if ( (todayMinutes > sunrise) && (todayMinutes < (sunset + 60)) ) {     // If it's daytime (between sunrise and an hour after sunset)...
    if (door != HIGH) {                                                   // ...and if the door is closed, open it.
      doorMotor.step(4000);                                               // Set this to the number of revolutions needed to open door.
    }
  } else { // If it's nighttime and...
    if (door == HIGH) {                                                   // ...if the door is open, close it.
      doorMotor.step(-4000);                                              // Set this to the number of revolutions needed to close door.
    }
  }
  Blynk.virtualWrite(V0, timeStringBuff);
  Blynk.virtualWrite(V1, interiorTemp);
  Blynk.virtualWrite(V2, exteriorTemp);
  Blynk.virtualWrite(V3, doorState);
  interiorTMP102.sleep();
  exteriorTMP102.sleep();
  //digitalWrite(DVR8833_SLP, LOW);                                       // This will cut power to the stepper motor, preventing overheating
}

void loop()
{
  ArduinoOTA.handle();    // Allows Over The Air updating through Arduino IDE
  Blynk.run();            // Connection to Blynk server
  timer.run();            // Runs BlynkTimer
}
