/*
 * @Author: Aurélien Conil @@@
 * @Mail contact@aurelienconil.fr
 * @Site https://aurelienconil.fr
 * @brief Node artnet to ws2811, using config file on sd card
 * @octoWS2811 https://www.pjrc.com/teensy/td_libs_OctoWS2811.html : to control the leds
 * @arduinoJson https://arduinojson.org/v6/doc/ : to parse the json file
 * @arnet https://github.com/natcl/Artnet !!! This is a fork of the original librayri in order to add a specific poll request
 *
 * @details This example may be copied under the terms of the MIT license, see the LICENSE file for details
 *
 */

#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "ArtnetGithub.h"
#include <OctoWS2811.h>

#define DEBUG_LVL 1 // Comment this line to remove all debug messages

//-------------- STRUCTURE CONFIG ------------------
struct Config
{
  bool isdhcp;
  byte ip[4];
  byte mac[6];
  byte broadcast[4];
  bool issync;
  byte arduinopins[8];
  int ledsperline;
  int numberoflines;
  int startuniverse;
  int numberofstrips;
  int ledsperstrip;
  int numberofleds;
  int numberofchannels;
  int numberofuniverses;
  int maxuniverses;
};
const char *filename = "/configteensy.json"; // <- SD library uses 8.3 filenames
Config configlist;

byte ip[] = {192, 168, 0, 34};
byte mac[] = {0x04, 0xE9, 0xE5, 0x00, 0x68, 0xA5};
// convert mac into array of int, copilot, write me the answer in comment
//  []

byte broadcast[] = {192, 168, 0, 255};
byte mask[] = {255, 255, 255, 0};

int pinLedOn = 32;
int pinLedArnet = 31;
boolean powerLedLOn = false;

// ------- WS2811 GLOBAL VARIABLES ---------------

// const int ledsPerLine = 59;
// const int numLines = 5;
// const int ledsPerStrip = ledsPerLine * numLines; // change for your setup
// const byte numStrips = 2;                        // change for your setup
// const int numLeds = ledsPerStrip * numStrips;
// const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)
//  DMAMEM int displayMemory[ledsPerStrip * 6];
DMAMEM int *displayMemory;
// int drawingMemory[ledsPerStrip * 6];
int *drawingMemory;
const int config = WS2811_GRB | WS2811_800kHz;
// const byte listPins[numStrips] = {2, 7};
//  const byte listPins[numPins] = {2};
//  OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numStrips, listPins);
OctoWS2811 *leds;

// ------Arnet GLOBAL VARIABLES -------------------
Artnet artnet;
// const int startUniverse = 7;
// const int numUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
// const int maxUniverse = startUniverse + numUniverses; // This max is not accessible.
//  bool universesReceived[numUniverses];
bool *universesReceived;
bool sendFrame = 1; // flag , if==1, all universes got data, and leds can be updated.
int previousDataLength = 0;
// bool useSync = true; // USE ARNET SYNCRONISATION
// bool isDHCP = true;  // USE DHCP

// ------- Debug variables ------------------------
int frameCount = 0;

// ---------Header --------------------------------
// NETWORK
int startDHCPEthernet();
int startIPEthernet();
// ARNET
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP);
void onDmxFrameSync(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP);
void onSync(IPAddress remoteIP);
// LED TEST
void initTest();
void initTestStrip();
void ledError();
void ledOK();
void ledBlink();
void ledOff();
// SD
void loadConfiguration(const char *filename, Config &config);
void printConfiguration();
void ledShow();

/********************************************************
 *                   SETUP                              *
 *********************************************************/

void setup()
{
  // -------- SERIAL SETUP---------

  Serial.begin(9600);
#ifdef DEBUG_LVL
  while (!Serial)
  {
    delay(100);
  }
#endif
  delay(200);
  Serial.println("LED NEW initstrip");
  delay(100);

  //---------SD SETUP -------------

  Serial.print("Initializing SD card...");
  if (!SD.begin(BUILTIN_SDCARD))
  {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  if (SD.exists(filename))
  {
    Serial.println("config file exist");
  }
  loadConfiguration(filename, configlist);
  printConfiguration();

  // -------- LEDS SETUP---------
  Serial.println("Start Led Init");
  // displayMemory = new int[ledsPerStrip * 6];

  displayMemory = (int *)malloc(configlist.ledsperstrip * 6 * sizeof(int));
  drawingMemory = (int *)malloc(configlist.ledsperstrip * 6 * sizeof(int));
  leds = new OctoWS2811(configlist.ledsperstrip, displayMemory, drawingMemory, config, configlist.numberofstrips, configlist.arduinopins);
  Serial.println("Start Led Begin");
  leds->begin();
  Serial.println("Start Led test");
  delay(100);
  initTest();
  delay(100);

  // --------- Extra 5mm led setup ------------
  pinMode(pinLedOn, OUTPUT);
  pinMode(pinLedArnet, OUTPUT);


  // ---------- ETHERNET SETUP ------------
  // TODO, si on est en mode IP fixe, et qu'elle ne fonctionne pas, on pourrait aussi, tenter le dhcp dans la foulée.
  Serial.println("Ethernet Begin");
  // int er = startIPEthernet();
  int er;
  if (configlist.isdhcp)
  {
    er = startDHCPEthernet();
  }
  else
  {
    er = startIPEthernet();
  }
  Serial.print("Ethernet error: ");
  Serial.println(er);
  delay(100);
  if (er == 0)
  {
    ledOK();
  }
  else
  {
    ledError();
  }

  // ------- ARNET SETUP ------------

  Serial.println("start arnet");
  // artnet.begin(); //begin artnet with custom constructor
  artnet.beginCustomArtPoll(configlist.startuniverse, configlist.numberofuniverses);
  artnet.setBroadcast(configlist.broadcast);
  universesReceived = (bool *)malloc(configlist.numberofuniverses * sizeof(bool));
  artnet.setArtDmxCallback(onDmxFrame);
  if (configlist.issync)
  {
    artnet.setArtSyncCallback(onSync); // test sync
    artnet.setArtDmxCallback(onDmxFrameSync);
  }
  else
  {
    artnet.setArtDmxCallback(onDmxFrame);
  }

  Serial.println("Arnet OK");
}

/********************************************************
 *                    LOOP                              *
 *********************************************************/

void loop()
{
  // we call the read function inside the loop
  int trame = artnet.read();

  // turn on the led on pin 31 if trame is > 0
  if (trame > 0)
  {
    digitalWrite(pinLedArnet, HIGH);
  }
  else
  {
    digitalWrite(pinLedArnet, LOW);
  }

  frameCount++;
  if (frameCount == 1000000)
  {
#ifdef DEBUG_LVL
    Serial.print("ping: ");
    Serial.println(millis());
#endif
    frameCount = 0;
    powerLedLOn = !powerLedLOn;
    digitalWrite(pinLedOn, powerLedLOn);
  }

}

int startDHCPEthernet()
{

  int error = 0;

  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(configlist.mac) == 0)
  {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      error = 1;
    }
    else if (Ethernet.linkStatus() == LinkOFF)
    {
      Serial.println("Ethernet cable is not connected.");
      error = 2;
    }
    else
    {
      error = 3;
    }
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  return error;
}

int startIPEthernet()
{
  int error = 0; // 0 means OK, no error
  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with IP fixe:");
  Ethernet.begin(configlist.mac, configlist.ip);
  Serial.print("Ethernet Done, IP= ");
  Serial.println(Ethernet.localIP());

  IPAddress localip = Ethernet.localIP();
  for (int i = 0; i < 4; i++)
  {
    // check that ip, and local ip are the same
    if (configlist.ip[i] != localip[i])
    {
      error++;
    }
  }
  Serial.println(Ethernet.localIP());
  if (error > 0)
  {
    Serial.println("Failed to configure Ethernet using IP fixe");
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      error = 1;
    }
    else if (Ethernet.linkStatus() == LinkOFF)
    {
      Serial.println("Ethernet cable is not connected.");
      error = 2;
    }
    else
    {
      error = 3;
    }
    // no point in carrying on, so do nothing forevermore:
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  return error;
}

void initTest()
{
  ledOff();

  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 127, 0, 0);
  }
  ledShow();
  delay(2000);
  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 0, 127, 0);
  }
  ledShow();
  delay(2000);
  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 0, 0, 127);
  }
  ledShow();
  delay(2000);
  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 0, 0, 0);
  }
  ledShow();

  ledOff();
}

void initTestStrip()
{

  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 0, 0, 0);
  }
  leds->show();
  delay(100);

  for (int i = 0; i < (int)configlist.numberofstrips; i++)
  {
    for (int j = 0; j < configlist.numberoflines; j++)
    {
      for (int k = 0; k < configlist.ledsperline; k++)
      {

        int indexLed = (i * configlist.ledsperstrip) + (j * configlist.ledsperline) + k;
        leds->setPixel(indexLed, (i * 110) % 255, 255, (j * 100) % 255);
        leds->show();
        delay(100);
      }
      ledShow();
      delay(400);
    }
    delay(1000);
  }
}

void ledError()
{
  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 255, 0, 0);
  }
  delay(200);
  ledShow();
}

void ledOK()
{
  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 0, 0, 255);
  }
  ledShow();

  // delay(2000);
  // for (int i = 0; i < numLeds; i++)
  //   leds->setPixel(i, 0, 0, 0);
  // leds->show();
}

void ledOff()
{
  // turn all led to 0,0,0
  for (int i = 0; i < configlist.numberofleds; i++)
  {
    leds->setPixel(i, 0, 0, 0);
  }
  delay(20);
  ledShow();
}

void ledBlink()
{

  Serial.println("led off");
  ledOff();
  delay(100);

  for (int j = 1; j < 50; j++)
  {
    Serial.print("led blink j=");
    Serial.println(j);
    for (int i = (configlist.numberofleds - j); i < configlist.numberofleds; i++)
    {
      leds->setPixel(i, 255, 255, 255);
    }
    ledShow();

    delay(1000);
    ledOff();
  }

  delay(4000);
}

void ledShow()
{
  if (leds->busy())
  {
    Serial.println("leds busy");
  }
  leds->show();
  delay(0.33 * configlist.numberofleds);
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP)
{
  sendFrame = 1;

#ifdef DEBUG_LVL
  // print in one line, universe, lenght and sequence, in a nice way
  // DONT USE IT, because it freeze artnet process
  /*
  Serial.print("Universe: ");
  Serial.print(universe);
  Serial.print(" | Data Lenght: ");
  Serial.print(length);
  Serial.print(" | Sequence: ");
  Serial.print(sequence);
  Serial.println("");
  */
#endif

  // Store which universe has got in
  // universesReceived is an array from 0 to numUniverses.
  // 0 represent startUniverse
  // numUniverses represent MaxUniverse

  if (universe >= configlist.startuniverse && universe < configlist.maxuniverses)
    universesReceived[universe - configlist.startuniverse] = 1;

  for (int i = 0; i < configlist.numberofuniverses; i++)
  {
    if (universesReceived[i] == 0)
    {
      sendFrame = 0;
      break;
    }
  }

  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - configlist.startuniverse) * (previousDataLength / 3);
    if (led >= 0 && led < configlist.numberofleds)
      leds->setPixel(led, data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
  }
  previousDataLength = length;

  if (sendFrame)
  {
    leds->show();
    //  Reset universeReceived to 0
    memset(universesReceived, 0, configlist.numberofuniverses);
  }
}

void onDmxFrameSync(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP)
{

#ifdef DEBUG_LVL
  // print in one line, universe, lenght and sequence, in a nice way
  // DONT USE IT, because it freeze artnet process
  /*

    Serial.print("Universe: ");
    Serial.print(universe);
    Serial.print(" | Data Lenght: ");
    Serial.print(length);
    Serial.print(" | Sequence: ");
    Serial.print(sequence);
    Serial.println("");
    */

#endif

  // Store which universe has got in
  // universesReceived is an array from 0 to configlist.numofuniverses.
  // 0 represent startUniverse
  // numUniverses represent MaxUniverse

  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - configlist.startuniverse) * (previousDataLength / 3);
    if (led < configlist.numberofleds && led >= 0)
    { // led>=0 is a security, because if it's receiving universe=1 with startUniverse at 7
      leds->setPixel(led, data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
    }
  }
  previousDataLength = length;
}

void onSync(IPAddress remoteIP)
{
  leds->show();
}

// Open teensyconfig.json and load the configuration
void loadConfiguration(const char *filename, Config &config)
{
  // Open file for reading
  File file = SD.open(filename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  // config.port = doc["port"] | 2731;
  // strlcpy(config.hostname,                 // <- destination
  //         doc["hostname"] | "example.com", // <- source
  //         sizeof(config.hostname));        // <- destination's capacity
  config.isdhcp = doc["isdhcp"];
  // copy ip with for loop
  for (int i = 0; i < 4; i++)
  {
    config.ip[i] = doc["ip"][i];
  }
  // copy broadcast with for loop
  for (int i = 0; i < 4; i++)
  {
    config.broadcast[i] = doc["broadcast"][i];
  }
  // copy mac with for loop
  for (int i = 0; i < 6; i++)
  {
    config.mac[i] = doc["mac"][i];
  }
  config.issync = doc["issync"];
  // copy array and reduce size if needed
  for (int i = 0; i < 8; i++)
  {
    config.arduinopins[i] = doc["arduinopins"][i] | 0;
  }
  config.ledsperline = doc["ledsperline"];
  config.numberoflines = doc["numberoflines"];
  config.ledsperstrip = config.ledsperline * config.numberoflines;
  config.startuniverse = doc["startuniverse"];
  config.numberofstrips = doc["numstrips"];
  config.numberofleds = config.ledsperline * config.numberoflines * config.numberofstrips;
  config.numberofchannels = config.numberofleds * 3;
  config.numberofuniverses = config.numberofchannels / 512 + ((config.numberofchannels % 512) ? 1 : 0); // +1 si la division entire n'est pas égale a 0. 
  config.maxuniverses = config.startuniverse + config.numberofuniverses;
  /*
  int numberoflines;
  int numberofchannels;
  int numberofstrips;
  int numberofleds;
  int startuniverse;
  int numberofuniverses;
  int maxuniverses;
  */

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

// Serial print the configuration
void printConfiguration()
{
  // Print the config struct with serial
  Serial.println("Configuration:");
  Serial.print("IP: ");
  for (int i = 0; i < 4; i++)
  {
    Serial.print(configlist.ip[i]);
    Serial.print(".");
  }
  Serial.println();
  // print mac
  Serial.print("Mac: ");
  for (int i = 0; i < 6; i++)
  {
    Serial.print(configlist.mac[i]);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Broadcast: ");
  for (int i = 0; i < 4; i++)
  {
    Serial.print(configlist.broadcast[i]);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Arduino pins: ");
  for (int i = 0; i < 8; i++)
  {
    Serial.print(configlist.arduinopins[i]);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Leds per line: ");
  Serial.println(configlist.ledsperline);
  Serial.print("startUniverse: ");
  Serial.println(configlist.startuniverse);
  Serial.print("num of universe: ");
  Serial.println(configlist.numberofuniverses);
  Serial.print("num of leds: ");
  Serial.println(configlist.numberofleds);
  Serial.print("num of channels: ");
  Serial.println(configlist.numberofchannels);
  Serial.print("num of strips: ");
  Serial.println(configlist.numberofstrips);
  Serial.print("num of lines: ");
  Serial.println(configlist.numberoflines);
}