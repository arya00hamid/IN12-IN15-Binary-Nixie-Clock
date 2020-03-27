// IN-12 & IN-15 Binary Nixie Clock by Marcin Saj https://nixietester.com
// https://github.com/marcinsaj/IN12-IN15-Binary-Nixie-Clock
//
// NTP IN-12 & IN-15 Binary Nixie Clock Example
//
// This example demonstrates how to connect clock to WiFi and 
// synchronizing (ones per day) RTC DS3231 time module with NTP time server 
// 
// Serial monitor is required to debug synchronization
//
// Hardware:
// WiFi signal
// IN-12 & IN-15 Binary Nixie Clock - https://nixietester.com/project/in12-in15-binary-nixie-clock/
// Arduino Nano IoT 33 - https://store.arduino.cc/arduino-nano-33-iot
// 2 X Nixie Power Supply Module, 2 x Nixie Tube Driver V2 & RTC DS3231 module
// Nixie clock require 12V, 1.5A power supply
// Schematic IN-12 & IN-15 Binary Nixie Clock - http://bit.ly/IN2-BNC-Schematic
// Schematic Nixie Tube Driver V2 - http://bit.ly/NTD-Schematic
// Schematic Nixie Power Supply Module - http://bit.ly/NPS-Schematic
// DS3231 RTC datasheet: https://datasheets.maximintegrated.com/en/ds/DS3231.pdf

#include <RTClib.h>                       // https://github.com/adafruit/RTClib
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <RTCZero.h>
#include "arduino_secrets.h"              // Please enter your sensitive data in the Secret tab/arduino_secrets.h                              

const int timeZone = 1;                   // Change this to adapt it to your time zone 
                                          // https://en.wikipedia.org/wiki/List_of_time_zones_by_country

char ssid[] = SECRET_SSID;                // Your network SSID (name)
char pass[] = SECRET_PASS;                // Your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                         // Your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

unsigned long epochTime;
int numberOfTries = 0, maxTries = 30;

#define timeToSynchronizeTime 3           // Each day at 3AM, the RTC DS3231 time will be synchronize with WIFI time
boolean timeToSynchronizeTimeFlag = 0;

int timeHour = 0;
int timeMinute = 0;
int timeSecond = 0;

RTC_DS3231 rtcModule;                     // RTC module library declaration
RTCZero rtc;                              // Arduino Nano Every and IoT 33 have built in RTC 
                                          // this RTC is used by the WiFiNINA.h library
                                          // But for timekeeping we will use DS3231 RTC module because is more accurate

#define EN_NPS    A0
#define DIN_PIN   6          // Nixie driver (shift register) serial data input pin             
#define CLK_PIN   7          // Nixie driver clock input pin
#define EN_PIN    8          // Nixie driver enable input pin

// Choose Time Format
#define hourFormat    12     // 12 Hour Clock or 24 Hour Clock

// Bit numbers declaration for nixie tubes display
//            1   2   4   8  16  32
byte H1[] = {40, 42, 15, 17, 19, 21};     // "1" Hours
byte H0[] = {39, 41, 14, 16, 18, 20};     // "0" Hours
byte M1[] = {34, 30, 24, 11,  9,  7};     // "1" Minutes
byte M0[] = {35, 31, 25, 10,  8,  6};     // "0" Minutes
byte S1[] = {32, 28, 26,  5,  3,  1};     // "1" Seconds
byte S0[] = {33, 29, 27,  4,  2,  0};     // "0" Seconds

// 18 bits for "1", 18 bits for "0" - check clock schematic
// 3 bits for "H", "M", "S" symbols
// 5 bits for gaps - nixie drivers not connected outputs 
// 2 bits for nixie driver gaps - check driver schematic 

// Nixie Display bit array
boolean nixieBitArray[46]; 

byte H = 38; // Bit number "H" symbol IN-15B nixie tube
byte M = 37; // Bit number "M" symbol IN-15A nixie tube
byte S = 36; // Bit number "S" symbol IN-15B nixie tube

// Serial monitor state
boolean serialState = 0;

// Millis delay time variable 
unsigned long previous_millis = 0;
unsigned long current_millis = 0;


void setup() 
{
  Serial.begin(9600);
  rtc.begin();
  rtcModule.begin();  
 
  pinMode(EN_NPS, OUTPUT);
  digitalWrite(EN_NPS, HIGH);   // Turn OFF nixie power supply module 

  pinMode(DIN_PIN, OUTPUT);
  digitalWrite(DIN_PIN, LOW);  
    
  pinMode(CLK_PIN, OUTPUT);
  digitalWrite(CLK_PIN, LOW);         
  
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);
  
  delay(5000);

  Serial.println("#############################################################");
  Serial.println("----------- IN-12 & IN-15 NTP Binary Nixie Clock ------------");
  Serial.println("#############################################################");
  Serial.println();

  SynchronizeTimeWiFi();        // Try to connect to WiFi and synchronize time

  digitalWrite(EN_NPS, LOW);    // Turn ON nixie power supply module
}


void loop() 
{    
  // Check if it's time to synchronize the time  
  if(timeToSynchronizeTimeFlag == 1)
  {
    SynchronizeTimeWiFi();    
  }
  
  // Millis time start
  current_millis = millis();

  // Wait 1 second
  if(current_millis - previous_millis >= 1000)
  {
    // Get time from RTC and display on nixie tubes
    DisplayTime();
    previous_millis = current_millis;      
  }
}

void SynchronizeTimeWiFi()
{
  int i  = 0;

  // Try connecting to the WiFi up to 5 times
  while ((status != WL_CONNECTED) && i < 5) 
  {
    Serial.println();
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // Wait 10 seconds for connection, print progress bar      
    int progressBar = 0;
    
    while(progressBar <= 60)
    {
      Serial.print("#");
      delay(165);
      progressBar++;    
    }
    
    i++;
  }
  
  Serial.println();

  if(status == WL_CONNECTED)
  {
    Serial.println("You are connected to WiFi!");  
    
    // Attempt to WiFi time synchronization
    GetTimeWiFi();
  }
  else if (status != WL_CONNECTED)
  {
    Serial.println("The WiFi connection failed");
    Serial.println("Check your WIFI connection and credentials");
    Serial.println("Next attempt to connect in 24 hours");  
  }
  
  timeToSynchronizeTimeFlag = 0;
  PrintWiFiStatus();  
}

void GetTimeWiFi()
{
  do 
  {
    epochTime = WiFi.getTime();
    numberOfTries++;
  }
  while ((epochTime == 0) && (numberOfTries < maxTries));

  if (numberOfTries == maxTries) 
  {
    Serial.println("Time synchronization is failed");
    Serial.println("Next attempt to synchronize time in 24 hours");
    Serial.println(); 
    delay(3000);
  }
  else 
  {
    Serial.println("Time synchronization succeeded!");
    Serial.println();
    rtc.setEpoch(epochTime);

    timeHour = rtc.getHours() + timeZone;
    
    if(timeHour < 0)
    {
      timeHour = 24 + timeHour;  
    }

    if(timeHour > 23)
    {
      timeHour = timeHour - 24;  
    }
    
    timeMinute = rtc.getMinutes();
    timeSecond = rtc.getSeconds();

    // Update RTC DS3231 module
    rtcModule.adjust(DateTime(0, 0, 0, timeHour, timeMinute, timeSecond));
  }  
}

void PrintWiFiStatus() 
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println();
}

void DisplayTime()
{
  DateTime now = rtcModule.now();
  
  timeSecond = now.second();
  timeMinute = now.minute();
  timeHour = now.hour();
 
  if(timeHour == timeToSynchronizeTime && timeMinute == 0 && timeSecond == 0)
  {
    timeToSynchronizeTimeFlag = 1;  
  }
  
  byte timeFormat = hourFormat;
    
  // Check time format and adjust
  if(timeFormat == 12 && timeHour > 12) timeHour = timeHour - 12;
  if(timeFormat == 12 && timeHour == 0) timeHour = 12; 

  Serial.print("Time: ");
  if(timeHour < 10)   Serial.print("0");
  Serial.print(timeHour);
  Serial.print(":");
  if(timeMinute < 10) Serial.print("0");
  Serial.print(timeMinute);  
  Serial.print(":");
  if(timeSecond < 10) Serial.print("0");
  Serial.println(timeSecond);      

  NixieDisplay(timeHour, timeMinute, timeSecond);
}
 
void NixieDisplay(byte hours, byte minutes, byte seconds)
{
  boolean bitTime = 0;

  for (int i = 0; i <=45; i++)
  {
    // Clear bit array 
    nixieBitArray[i] = 0;  
  }

  nixieBitArray[H] = 1;                           // Turn ON "H" symbol IN-15B nixie tube
  nixieBitArray[M] = 1;                           // Turn ON "M" symbol IN-15A nixie tube
  nixieBitArray[S] = 1;                           // Turn ON "S" symbol IN-15B nixie tube  
    
  for(int i = 0; i < 6; i++)
  {
    bitTime = hours & B00000001;                  // Extraction of individual bits 0/1
    hours = hours >> 1;                           // Bit shift

    if(bitTime == 1) nixieBitArray[H1[i]] = 1;    // Set corresponding bit     
    else nixieBitArray[H0[i]] = 1;

    bitTime = minutes & B00000001;                // Extraction of individual bits 0/1
    minutes = minutes >> 1;                       // Bit shift

    if(bitTime == 1) nixieBitArray[M1[i]] = 1;    // Set corresponding bit      
    else nixieBitArray[M0[i]] = 1;

    bitTime = seconds & B00000001;                // Extraction of individual bits 0/1
    seconds = seconds >> 1;                       // Bit shift

    if(bitTime == 1) nixieBitArray[S1[i]] = 1;    // Set corresponding bit     
    else nixieBitArray[S0[i]] = 1;
  }
    
  ShiftOutData();
}

void ShiftOutData()
{
  // Ground EN pin and hold low for as long as you are transmitting
  digitalWrite(EN_PIN, 0); 
  // Clear everything out just in case to
  // prepare shift register for bit shifting
  digitalWrite(DIN_PIN, 0);
  digitalWrite(CLK_PIN, 0);  

  // Send data to the nixie drivers 
  for (int i = 45; i >= 0; i--)
  {    
    // Send current bit 
    if(nixieBitArray[i] == 1) digitalWrite(DIN_PIN, HIGH);
    else digitalWrite(DIN_PIN, LOW);     
    // Register shifts bits on upstroke of CLK pin 
    digitalWrite(CLK_PIN, 1);
    // Set low the data pin after shift to prevent bleed through
    digitalWrite(CLK_PIN, 0);  
  }   

  // Return the EN pin high to signal chip that it 
  // no longer needs to listen for data
  digitalWrite(EN_PIN, 1);
    
  // Stop shifting
  digitalWrite(CLK_PIN, 0);    
}
