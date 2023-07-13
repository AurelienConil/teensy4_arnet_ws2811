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

struct Config
{
  bool isdhcp; // true if we use dhcp, false if we use ip fixe
  int ip[4];
  int mac[6];
  int broadcast[4];   // broadcast address, depends of router
  bool issync;        // arnet sync , allow sync with multiple teensy use. Improve visual quality
  int numberofpins;   // number of pins used, numbers of strips.
  int arduinopins[8]; // Array of 8, but only one can be used
  int ledsperline;    // Number of leds in one line
  int numberoflines;  // Number of line for each teensy pins
  int startuniverse;
};

// WS2811
/*
Dans l'idéal. On dirait le nombre de ligne sur la pin 2, nombre de lignes sur la pin 3, etc ...
*/

const int ledsPerLine = 59;
const int numLines = 5;
const int ledsPerStrip = ledsPerLine * numLines; // change for your setup
const byte numStrips = 2;                        // change for your setup
const int numLeds = ledsPerStrip * numStrips;
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)
DMAMEM int displayMemory[ledsPerStrip * 6];
int drawingMemory[ledsPerStrip * 6];
const int config = WS2811_GRB | WS2811_800kHz;
const byte listPins[numStrips] = {2, 7};
// const byte listPins[numPins] = {2};
OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numStrips, listPins);

// Arnet
Artnet artnet;
const int startUniverse = 7;
const int numUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
const int maxUniverse = startUniverse + numUniverses; // This max is not accessible.
bool universesReceived[numUniverses];
bool sendFrame = 1; // flag , if==1, all universes got data, and leds can be updated.
int previousDataLength = 0;
bool useSync = true; // USE ARNET SYNCRONISATION

// Debug
int frameCount = 0;

// Header
int startDHCPEthernet();
int startIPEthernet();
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP);
void onDmxFrameSync(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP);
void initTest();
void initTestStrip();
void onSync(IPAddress remoteIP);
void ledError();
void ledOK();
void ledBlink();
void ledOff();

void setup()
{
  // SERIAL

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

  // LEDS
  Serial.println("Start Led Begin");
  leds.begin();
  Serial.println("Start Led test");
  delay(100);
  // initTest();
  // initTestStrip();
  ledBlink();
  delay(1000);

  // ETHERNET
  // TODO, si on est en mode IP fixe, et qu'elle ne fonctionne pas, on pourrait aussi, tenter le dhcp dans la foulée.
  Serial.println("Ethernet Begin");
  // int er = startIPEthernet();
  int er = startDHCPEthernet();
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

  // ARNET
  // artnet.begin(mac, ip);
  artnet.begin();
  artnet.setBroadcast(broadcast);
  artnet.setArtDmxCallback(onDmxFrame);
  if (useSync)
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

void loop()
{
  // we call the read function inside the loop
  artnet.read();

#ifdef DEBUG_LVL
  frameCount++;
  if (frameCount == 1000000)
  {
    Serial.print("ping: ");
    Serial.println(millis());
    frameCount = 0;
  }
#endif
}

int startDHCPEthernet()
{

  int error = 0;

  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0)
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
  Ethernet.begin(mac, ip);
  Serial.print("Ethernet Done, IP= ");
  Serial.println(Ethernet.localIP());

  IPAddress localip = Ethernet.localIP();
  for (int i = 0; i < 4; i++)
  {
    // check that ip, and local ip are the same
    if (ip[i] != localip[i])
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
  for (int i = 0; i < numLeds; i++)
    leds.setPixel(i, 127, 0, 0);
  leds.show();
  delay(500);
  for (int i = 0; i < numLeds; i++)
    leds.setPixel(i, 0, 127, 0);
  leds.show();
  delay(500);
  for (int i = 0; i < numLeds; i++)
    leds.setPixel(i, 0, 0, 127);
  leds.show();
  delay(500);
  for (int i = 0; i < numLeds; i++)
    leds.setPixel(i, 0, 0, 0);
  leds.show();
}

void initTestStrip()
{

  for (int i = 0; i < numLeds; i++)
  {
    leds.setPixel(i, 0, 0, 0);
  }
  leds.show();
  delay(100);

  for (int i = 0; i < (int)numStrips; i++)
  {
    for (int j = 0; j < numLines; j++)
    {
      for (int k = 0; k < ledsPerLine; k++)
      {

        int indexLed = (i * ledsPerStrip) + (j * ledsPerLine) + k;
        leds.setPixel(indexLed, (i * 110) % 255, 255, (j * 100) % 255);
        leds.show();
        delay(100);
      }
      leds.show();
      delay(400);
    }
    delay(1000);
  }
}

void ledError()
{
  for (int i = 0; i < numLeds; i++)
  {
    leds.setPixel(i, 255, 0, 0);
    delay(20);
    leds.show();
  }

  delay(200);
  leds.show();
}

void ledOK()
{
  for (int i = 0; i < numLeds; i++)
  {
    leds.setPixel(i, 0, 0, 255);
    delay(20);
    leds.show();
  }

  // delay(2000);
  // for (int i = 0; i < numLeds; i++)
  //   leds.setPixel(i, 0, 0, 0);
  // leds.show();
}

void ledOff()
{
  // turn all led to 0,0,0
  for (int i = 0; i < numLeds; i++)
  {
    leds.setPixel(i, 0, 0, 0);
  }
  delay(20);
  leds.show();
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
    for (int i = (numLeds - j); i < numLeds; i++)
    {
      leds.setPixel(i, 255, 255, 255);
      leds.show();
      delay(1);
    }

    delay(1000);
    ledOff();
  }

  delay(4000);
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

  if (universe >= startUniverse && universe < maxUniverse)
    universesReceived[universe - startUniverse] = 1;

  for (int i = 0; i < numUniverses; i++)
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
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    if (led < numLeds)
      leds.setPixel(led, data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
  }
  previousDataLength = length;

  if (sendFrame)
  {
    leds.show();
    //  Reset universeReceived to 0
    memset(universesReceived, 0, numUniverses);
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
  // universesReceived is an array from 0 to numUniverses.
  // 0 represent startUniverse
  // numUniverses represent MaxUniverse

  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    if (led < numLeds)
      leds.setPixel(led, data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
  }
  previousDataLength = length;
}

void onSync(IPAddress remoteIP)
{
  leds.show();
}