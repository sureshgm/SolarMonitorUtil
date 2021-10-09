/*
   DC energy meter application uses WeMOS D1 R1 ESP8266 node MCU connected to ADS1015 I2C ADC.
   Udp NTP Client - http://en.wikipedia.org/wiki/Network_Time_Protocol
*/

#include <ESP8266WiFi.h>
#include <WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager
#define WEBSERVER_H
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <WiFiUdp.h>
#include <Adafruit_ADS1X15.h>
#include <EEPROM.h>
#include "SimpleTimer.h"  // https://playground.arduino.cc/Code/SimpleTimer/
#include "ThingSpeak.h"   // always include thingspeak header file after other header files and custom macros
#include "htmlfile.c"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define ZERO_OFFSET 684.0       // Zero offset for Current sensor/1.667V
#define BIT_PVCURRENT 0.12      // multiply by 0.120 to get actual current
#define BIT_PVVOLT  0.038        // multiply read ADC value by 38 to get actual voltage
#define EPROM_SIG_ADDR  0       // first 8 bytes is signature
#define EPROM_REC_CNT 8         // 8th and 9th byte will give record count
#define EPROM_DATA_OFFSET 10    // 10th byte and above data is stored

#define EPOCH_OFFSET  19800     // Unix seconds for +530 offset 

#define LED_FAST_BLINK  0x5555
#define LED_SLOW_BLINK  0x3030
#define LED_ON  0xFFFF
#define LED_OFF 0x0000
#define LED_HRTBT 0x0001

AsyncWebServer server(80);
AsyncEventSource events("/events");
DNSServer dns;
//Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
WiFiClient  client;
SimpleTimer Scheduler;      // create a object of simple timer
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ADC_MODE(ADC_VCC);    // configure ADC to read VCC

void InitEnergyMeter(void);
void ResetEnergyMeter(void);
void DCEnergyMeter(void);
void DisplayVal(void);
void SaveData2EEPROM(void);

void Connect2WiFi(void);
void sendNTPpacket(IPAddress& address);
void GetNTPTime(void);
void UpdateTime(void);

void UpdateCloud(void);
String processor(const String& var);
void UpdateLED(void);

//void configModeCallback (WiFiManager *myWiFiManager)
//{
//  Serial.println("In WiFi config mode");
//  Serial.println(WiFi.softAPIP());
//  Serial.println(myWiFiManager->getConfigPortalSSID());
//  display.clearDisplay();
//  display.setCursor(0, 0);            // Start at top-left corner
//  display.setTextSize(2);             // Draw 2X-scale text
//  display.println(F("Connect to"));
//  display.setTextSize(1);             // Draw 2X-scale text
//  display.println(F("192.168.4.1"));
//  display.display();
//}

/*
   Global variables
*/
// extern static const char index_html[];
String WiFiHostname = "SolUtlMon-001";    // give a name for your wifi device
char PrintBuff[64];         // buffer for printing values

unsigned long ticktime;     // tick time, assigned to millis() every milli second
int tick_100ms;             // variable counts 100 for 100ms task execution
uint32_t Cnt100mS;          // counter for real time
//struct Time {
//  uint8_t Secs, Mins, Hrs;
//  uint16_t Days;
//};
//Time Tm1;
struct tm Tm1;

unsigned int localPort = 2390;        // local port to listen for UDP packets
IPAddress timeServerIP;               // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;       // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];  //buffer to hold incoming and outgoing packets

float PVCurrent, PVVoltage;           // instantaneous voltage and current
float PVPower;                        // Instantaneous power
long AccuPVPower;                     // accumulated energy cleared oce a 1wh is reached
long PVEvergy;                        // holds wh reading

int SDE_Delay;                        // save data to eeprom delay counter
long ep_wrcnt;                        // eeprom Write cycle count
const char EPROM_Signature[] = "SEmon 21";

unsigned long myChannelNumber = xxxx;
const char * myWriteAPIKey = "xxxxxxxxxxxxxx";

const int LED = LED_BUILTIN;
uint16_t LED_BlinkPattern;
int EspVcc;

enum EEDataSeq {sig_w1, sig_w2, eeprom_wc, pvp_energy, pvm_energy, last_word};

void setup(void)
{
   bool res;
  Serial.begin(115200);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    Serial.println(F("SSD1306 allocation failed"));
  display.clearDisplay();
  display.setTextSize(2);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);            // Start at top-left corner
  display.println(F("   Solar"));
  display.println(F("Power Meter"));
  display.display();

  Serial.print("File: ");   Serial.println(__FILE__);
  delay(5);   // time to serial print
  Serial.print("Date: ");   Serial.print(__DATE__);
  delay(5);   // time to serial print
  Serial.print(",\tTime: ");   Serial.println(__TIME__);
  delay(5);   // time to serial print
  Serial.println(" Solar Power Utilisation Monitor");
  delay(5);   // time to serial print
  EEPROM.begin(512);
  // Initialize SPIFFS
//  if (!SPIFFS.begin()) {
//    Serial.println("Error mounting SPIFFS");
//  }

//  ESPAsync_WiFiManager wifiManager(&server, &dns);          // connect to WiFi
//  wifiManager.setConfigPortalTimeout(180);              // timeout for auto config
//  wifiManager.setAPCallback(configModeCallback);        // call-back on failed connection to wifi
//  wifiManager.autoConnect();
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFiManager wifiManager;
  res = wifiManager.autoConnect(); // auto generated AP name from chipid
  if(!res) Serial.println("Failed to connect");
  else     Serial.println("connected to WiFi)");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Local IP: "));
    Serial.println(WiFi.localIP());
  }
  else {
    //Serial.println(wifiManager.getStatus(WiFi.status()));
  }
  //set led pin as output
  pinMode(LED, OUTPUT);
  SDE_Delay = 0;
  Connect2WiFi();
  // OTAFirmwareUpdate
  // Setting these values incorrectly may destroy ADC!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);   // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  ads.setGain(GAIN_ONE);            // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);         // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);        // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);       // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);     // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

  ads.begin();      // start I2C ADC
  InitEnergyMeter();
  GetNTPTime();                                 // NTP time sync every hour(3600 seconds)
  Scheduler.setInterval(600000, GetNTPTime);    // Next 1 hour - 1 sec seconds)
  Scheduler.setInterval(100, DCEnergyMeter);
  Scheduler.setInterval(10000, DisplayVal);
  Scheduler.setInterval(1000, UpdateTime);
  Scheduler.setInterval(3600000, UpdateCloud);    // update clpud every hour
  Scheduler.setInterval(125, UpdateLED);
  Scheduler.setInterval(5, CheckVcc);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient * client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  ThingSpeak.begin(client);  // Initialize ThingSpeak
}

void loop(void)
{
  Scheduler.run();    // Ticker function calls all functions as scheduled in setup or else where in this program
  AsyncElegantOTA.loop();
}
/*
   Energy meter function to be called every 100mS, cosidering the RC constant of the voltage and current ADC is 0.1Sec
   Example 100V, 100A energy meter
*/
void DCEnergyMeter(void) {
  int16_t adc0, adc1, adc2, adc3;
  adc0 = ads.readADC_SingleEnded(0);    // dummy read
  adc1 = ads.readADC_SingleEnded(1);    // dummy read
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);
  PVCurrent = ((float)adc2 - ZERO_OFFSET) * BIT_PVCURRENT;
  PVVoltage = (float)adc3 * BIT_PVVOLT;
  PVPower = PVVoltage * PVCurrent;
  AccuPVPower += (long)PVPower;
  while (AccuPVPower > 36000) {           // 10 samples/second x 60 seconds * 60 minutes = 36000 = 1wh
    PVEvergy += 1;                        // increment energy count
    AccuPVPower -= 36000;
  }
}

/*


*/
void InitEnergyMeter(void)
{
  char edata[10];                  // temp array to read EEPROM
  Serial.println("Reading EEPROM Signature:");
  for (int i = 0; i < 8; i++)  {   // read 1st 8 bytes of EEPROM
    edata[i] = EEPROM.read(i);
    Serial.print(edata[i], HEX);
    Serial.print("\t");
  }
  edata[8] = 0;                    // Mark end of string

  if (strcmp(edata, EPROM_Signature)) {    // Compare with signature
    Serial.println("No Signature, writing");    // print signature mismatch
    for (int i = 0; i < 8; i++)                 // write the signature
      EEPROM.write(i, EPROM_Signature[i]);      //  EEPROM.write(addr, val);
    EEPROM.put(eeprom_wc * 4, 1);               // write '1' to EEPROM cycle count, this is the 1st write cycle
    EEPROM.put(pvp_energy * 4, 0);              // write '0' to EEPROM energy variable
    if (EEPROM.commit())  Serial.println("Signature write sucess");   // and commit
    else  Serial.println("Signature write Failure");
  }
  else {
    Serial.println("Signature matched!");         // if sinature matched print match string
    EEPROM.get( eeprom_wc * 4, ep_wrcnt);   // read eeprom write count variable from EEPROM

    if ((ep_wrcnt != 0) && (ep_wrcnt != 0xffffffff))   {
      EEPROM.get(pvp_energy * 4, PVEvergy);  // get the PV energy to EEPROM
      Serial.print("EEPROM Write Cycle:"); Serial.println(ep_wrcnt, HEX);
      Serial.print("EEPROM Energy:"); Serial.println(PVEvergy, HEX);
    }
    else  {
      PVEvergy = 0;
      SaveData2EEPROM();
    }
  }
  ticktime = millis();
  tick_100ms = 0;
  Cnt100mS = 0;
  AccuPVPower = 0;
  //  Serial.print("PVEvergy / long var:"); Serial.println(sizeof(PVEvergy));
  //  Serial.print("EEPROMAddr/ int var:"); Serial.println(sizeof(EEPROMAddr));
}

void SaveData2EEPROM(void)
{
  long ep_wrcnt;
  if (SDE_Delay)
    return;
  EEPROM.get( eeprom_wc * 4, ep_wrcnt);   // read eeprom write count variable from EEPROM
  if ((ep_wrcnt == 0) || (ep_wrcnt == 0xffffffff))   {   // if EEPROM cycle count is not present
    EEPROM.put(eeprom_wc * 4, 1);          // write '1' to EEPROM cycle count, this is the 1st write cycle
    EEPROM.put(pvp_energy * 4, PVEvergy);  // write the PV energy to EEPROM
    EEPROM.commit();                       // commit changes to EEPROM
    return;
  }
  else {
    ep_wrcnt++;                             // increment the write count
    EEPROM.put(eeprom_wc * 4, ep_wrcnt);    // write back the write count to EEPROM

    EEPROM.put(pvp_energy * 4, PVEvergy);  // write the PV energy to EEPROM
    EEPROM.commit();                       // commit changes to EEPROM

    Serial.print("EEPROM Write Cycle:"); Serial.println(ep_wrcnt);
    // mday
    // Yesterday_Energy
    SDE_Delay = 10 * 60;  // 10 minutes delay between write to EEPROM, to avaiod frequent writes
  }
}

/*
    Check Vcc and save to memory
*/
void CheckVcc(void)
{
  static bool epsaved;
  EspVcc = ESP.getVcc();
  if ((EspVcc < 2650) && (epsaved != true)) {
    SaveData2EEPROM();
    epsaved = true;
  }
  else if (EspVcc > 2750)  {
    epsaved = false;
  }
}

/*

*/
void ResetEnergyMeter(void)
{
  AccuPVPower = 0;
  PVEvergy = 0;     // reset energy count
  ep_wrcnt = 0;     // reset EEPROM cycle count
  SDE_Delay = 0;     // clear EEPROM periodic write dely to  0
  SaveData2EEPROM();  // write to EEPROM.
}

/*
   LED update function
*/

void UpdateLED(void)
{
  static uint16_t bitPattren;
  if (!bitPattren)
    bitPattren = 1;
  (LED_BlinkPattern & bitPattren) ? digitalWrite(LED, HIGH) : digitalWrite(LED, HIGH);
  bitPattren <<= 1;
}

/*

*/
void DisplayVal(void)
{
  static long PrevEnergy;               // variable used for printing if energy value change
  if (PrevEnergy != PVEvergy) {         // if energy meter count has changed
    PrevEnergy = PVEvergy;              // make it equal
    sprintf(PrintBuff, "[%2d:%02d:%02d]: Energy: %ld wh", Tm1.tm_hour, Tm1.tm_min, Tm1.tm_sec, PVEvergy);
    Serial.println(PrintBuff);          // print time and the energy values
  }
  else  {
    sprintf(PrintBuff, "[%2d:%02d:%02d]: %5.2f V, %5.2f A, %7.2f w, Vcc: %d", Tm1.tm_hour, Tm1.tm_min, Tm1.tm_sec, PVVoltage, PVCurrent, PVPower, EspVcc);
    Serial.println(PrintBuff);
  }
  // Send Events to the Web Server with the Sensor Readings
  events.send("ping", NULL, millis());
  events.send(String(PVVoltage).c_str(), "PVVoltage", millis());
  events.send(String(PVCurrent).c_str(), "PVCurrent", millis());
  events.send(String(PVPower).c_str(), "PVPower", millis());
  events.send(String(PVEvergy).c_str(), "PVEnergy", millis());
  // OLED Display
  display.clearDisplay();
  display.setTextSize(1);             // Draw 1X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);            // Start at top-left corner
  display.print((WiFi.localIP().toString()));   // line 1 print IP address
  sprintf(PrintBuff, " %02d:%02d", Tm1.tm_hour, Tm1.tm_min); // line 1 print time
  display.println(PrintBuff);
  display.setCursor(0, 16);            // Start at top-left corner
  display.setTextSize(2);             // Draw 2X-scale text
  sprintf(PrintBuff, " %6ld wh", PVEvergy);
  display.println(PrintBuff);
  display.display();
}
/*

*/

void UpdateTime(void)
{
  if (SDE_Delay) SDE_Delay--;   // Sve data to EEPROM delay
  Tm1.tm_sec++;           // else increment seconds
  if (Tm1.tm_sec > 59) {  // if 60 seconds passed
    Tm1.tm_sec = 0;       // reset Seconds to 0
    Tm1.tm_min++;         // increment minutes
    if (Tm1.tm_min > 59) { // if 60 minutes passed
      Tm1.tm_min = 0;     // reset minutes
      Tm1.tm_hour++;        // increment hours
      if (Tm1.tm_hour > 23)  { // if 23 hours passed
        Tm1.tm_hour = 0;    // reset hours
        Tm1.tm_mday++;     // increment days
        if (Tm1.tm_mday > 31)   // if 365 days passed
          Tm1.tm_mday = 1;       // reset day to 1
      } // hrs
    }   // mins
  }    // secs
}

/*

*/

void Connect2WiFi(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WiFiHostname.c_str());
  //WiFi.begin(ssid, pass);
  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  //while(WiFi.status() != WL_CONNECTED) {
  //    Serial.print(".");
  //    delay(1000);
  //  }
  Serial.println("\n WiFi connected");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
/*
   This function is used to get NTP time using UDP connection
   in 1st instance of calling this program, the function send request and sets next tick call after 1 second
   in the second instance of call, the function reads the received data
*/
void GetNTPTime(void)
{
  static char Pid;
  if (Pid > 1) Pid = 0;

  if (Pid == 0) {            // get a random server from the pool
    Pid = 1;                      // next time get into parsing received packet
    WiFi.hostByName(ntpServerName, timeServerIP);
    sendNTPpacket(timeServerIP);  // send an NTP packet to a time server
    Scheduler.setTimeout(1000, GetNTPTime);     // after 1 second call this function to read time
    return;
  }
  else  {
    Pid = 0;                      // next time send NTP request
    int cb = udp.parsePacket();
    if (!cb) {
      Serial.println("NTP - No data received");
      Scheduler.setTimeout(1000, GetNTPTime);     // failed response? request after 1 second
    } else {
      Serial.print("NTP received bytes:"); Serial.println(cb);
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

      // timestamp at 40 - 43 byte, four bytes,
      unsigned long secsSince1900 = word(packetBuffer[40], packetBuffer[41]);
      secsSince1900 <<= 16;
      secsSince1900 |= word(packetBuffer[42], packetBuffer[43]);
      time_t epoch = secsSince1900 - 2208988800UL;   // convert to Unix time [Jan 1 1970]
      epoch += EPOCH_OFFSET;                                // Add country time OFFSET
      //      Tm1.Secs = epoch % 60;
      //      Tm1.Mins = (epoch  % 3600) / 60;
      //      Tm1.Hrs = (epoch  % 86400L) / 3600;
      //      Tm1.Days = (epoch % 31536000) / 86400;                // day in an year
      Tm1 = *localtime(&epoch);
    }
    //Scheduler.setInterval(60000, GetNTPTime);    // Next 1 hour - 1 sec seconds)
  }

}
/*
    PVVoltage, PVCurrent, PVPower, PVEvergy
*/
void UpdateCloud(void)
{
  // set the fields with the values
  static long PrevPVEnergy;
  long QuantumPower;
  if (PrevPVEnergy != PVEvergy)  {
    QuantumPower = PVEvergy - PrevPVEnergy;
    PrevPVEnergy = PVEvergy;
    ThingSpeak.setField(1, 57754701);
    ThingSpeak.setField(2, PVVoltage);
    ThingSpeak.setField(3, PVCurrent);
    ThingSpeak.setField(4, QuantumPower);
    ThingSpeak.setField(5, PVEvergy);
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200) {
      Serial.println("Cloud updated");
    }
    else {
      Serial.println("Cloud Failed" + String(x));
    }
  }
  else
    return;
}
/*

*/
String processor(const String& var) {
  //Serial.println(var);
  if (var == "PV_VOLTAGE") {
    return String(PVVoltage);
  }
  else if (var == "PV_CURRENT") {
    return String(PVCurrent);
  }
  else if (var == "PV_POWER") {
    return String(PVPower);
  }
  else if (var == "PV_ENERGY") {
    return String(PVEvergy);
  }
}
