/*The MIT License (MIT)

Copyright (c) 2014 Nathanaël Lécaudé
https://github.com/natcl/Artnet, http://forum.pjrc.com/threads/24688-Artnet-to-OctoWS2811

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "ArtnetGithub.h"

Artnet::Artnet() {}

void Artnet::begin(byte mac[], byte ip[])
{
#if !defined(ARDUINO_SAMD_ZERO) && !defined(ESP8266) && !defined(ESP32)
  Ethernet.begin(mac, ip);
#endif

  Udp.begin(ART_NET_PORT);
}

void Artnet::begin()
{
  Udp.begin(ART_NET_PORT);
}

void Artnet::beginCustomArtPoll(int startU, int nbU)
{
  Udp.begin(ART_NET_PORT);
  customArtPollReply = true;
  startUniverse = startU;
  nbUniverses = nbU;
}

void Artnet::setBroadcastAuto(IPAddress ip, IPAddress sn)
{
  // Cast in uint 32 to use bitwise operation of DWORD
  uint32_t ip32 = ip;
  uint32_t sn32 = sn;

  // Find the broacast Address
  uint32_t bc = (ip32 & sn32) | (~sn32);

  // sets the broadcast address
  setBroadcast(IPAddress(bc));
}

void Artnet::setBroadcast(byte bc[])
{
  // sets the broadcast address
  broadcast = bc;
}
void Artnet::setBroadcast(IPAddress bc)
{
  // sets the broadcast address
  broadcast = bc;
}

void Artnet::modifyArtpollReply(String shortname, String longname, int port, int *swin, int *swout)
{
  // this function can modify the artnet_reply_s struct
  // in order to personnalize the reply to the poll
  // It can change the shortname and longname
  // It can change the number of ports
  // It can change the swin and swout

  // convert shortname string to char* and modidy struct
  uint8_t shortname_char[18];
  shortname.toCharArray((char *)shortname_char, 18);
  memcpy(ArtPollReply.shortname, shortname_char, sizeof(shortname_char));

  // convert longname string to char* and modidy struct
  uint8_t longname_char[64];
  longname.toCharArray((char *)longname_char, 64);
  memcpy(ArtPollReply.longname, longname_char, sizeof(longname_char));

  // modify the number of ports
  ArtPollReply.numbports = port;

  // modify the swin and swout
  uint8_t swin_char[4];
  uint8_t swout_char[4];
  for (uint8_t i = 0; i < 4; i++)
  {
    swin_char[i] = swin[i];
    swout_char[i] = swout[i];
  }
  memcpy(ArtPollReply.swout, swout_char, sizeof(swout_char));
  memcpy(ArtPollReply.swin, swin_char, sizeof(swin_char));
}

uint16_t Artnet::read()
{
  packetSize = Udp.parsePacket();

  remoteIP = Udp.remoteIP();
  if (packetSize <= MAX_BUFFER_ARTNET && packetSize > 0)
  {
    Udp.read(artnetPacket, MAX_BUFFER_ARTNET);

    // Check that packetID is "Art-Net" else ignore
    for (byte i = 0; i < 8; i++)
    {
      if (artnetPacket[i] != ART_NET_ID[i])
        return 0;
    }

    opcode = artnetPacket[8] | artnetPacket[9] << 8;

    if (opcode == ART_DMX)
    {
      sequence = artnetPacket[12];
      incomingUniverse = artnetPacket[14] | artnetPacket[15] << 8;
      dmxDataLength = artnetPacket[17] | artnetPacket[16] << 8;

      if (artDmxCallback)
        (*artDmxCallback)(incomingUniverse, dmxDataLength, sequence, artnetPacket + ART_DMX_START, remoteIP);
      return ART_DMX;
    }
    if (opcode == ART_POLL)
    {

      if (customArtPollReply)
      {
        customArtPoll();
      }
      else
      {
        standardArtPoll();
      }

      return ART_POLL;
    } // end of art poll
    if (opcode == ART_SYNC)
    {
      if (artSyncCallback)
        (*artSyncCallback)(remoteIP);
      return ART_SYNC;
    }
  }
  else
  {
    return 0;
  }
  return 0;
}

void Artnet::standardArtPoll()
{
  // fill the reply struct, and then send it to the network's broadcast address
  Serial.print("POLL from ");
  Serial.print(remoteIP);
  Serial.print(" broadcast addr: ");
  Serial.println(broadcast);

#if !defined(ARDUINO_SAMD_ZERO) && !defined(ESP8266) && !defined(ESP32)
  IPAddress local_ip = Ethernet.localIP();
#else
  IPAddress local_ip = WiFi.localIP();
#endif
  node_ip_address[0] = local_ip[0];
  node_ip_address[1] = local_ip[1];
  node_ip_address[2] = local_ip[2];
  node_ip_address[3] = local_ip[3];

  sprintf((char *)id, "Art-Net");
  memcpy(ArtPollReply.id, id, sizeof(ArtPollReply.id));
  memcpy(ArtPollReply.ip, node_ip_address, sizeof(ArtPollReply.ip));

  ArtPollReply.opCode = ART_POLL_REPLY;
  ArtPollReply.port = ART_NET_PORT;

  memset(ArtPollReply.goodinput, 0x08, 4);
  memset(ArtPollReply.goodoutput, 0x80, 4);
  memset(ArtPollReply.porttypes, 0xc0, 4);

  uint8_t shortname[18];
  uint8_t longname[64];
  sprintf((char *)shortname, "artnet arduino");
  sprintf((char *)longname, "Art-Net -> Arduino Bridge");
  memcpy(ArtPollReply.shortname, shortname, sizeof(shortname));
  memcpy(ArtPollReply.longname, longname, sizeof(longname));

  ArtPollReply.etsaman[0] = 0;
  ArtPollReply.etsaman[1] = 0;
  ArtPollReply.verH = 1;
  ArtPollReply.ver = 0;
  ArtPollReply.subH = 0;
  ArtPollReply.sub = 0;
  ArtPollReply.oemH = 0;
  ArtPollReply.oem = 0xFF;
  ArtPollReply.ubea = 0;
  ArtPollReply.status = 0xd2;
  ArtPollReply.swvideo = 0;
  ArtPollReply.swmacro = 0;
  ArtPollReply.swremote = 0;
  ArtPollReply.style = 0;

  ArtPollReply.numbportsH = 0;
  ArtPollReply.numbports = 4;
  ArtPollReply.status2 = 0x08;

  ArtPollReply.bindip[0] = node_ip_address[0];
  ArtPollReply.bindip[1] = node_ip_address[1];
  ArtPollReply.bindip[2] = node_ip_address[2];
  ArtPollReply.bindip[3] = node_ip_address[3];

  uint8_t swin[4] = {0x07, 0x08, 0x09, 0x0A};
  uint8_t swout[4] = {0x07, 0x08, 0x09, 0x0A};
  for (uint8_t i = 0; i < 4; i++)
  {
    ArtPollReply.swout[i] = swout[i];
    ArtPollReply.swin[i] = swin[i];
  }

  sprintf((char *)ArtPollReply.nodereport, "%i DMX output universes active.", ArtPollReply.numbports);
  Udp.beginPacket(broadcast, ART_NET_PORT); // send the packet to the broadcast address
  Udp.write((uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
  Udp.endPacket();
}

void Artnet::customArtPoll()
{
  int nbArtPoll = nbUniverses / 4 + ((nbUniverses %4 )? 1 : 0 );     // +1 seulement si le reste de la division entiere est différent de 0

  for (int i = 0; i < nbArtPoll; i++)
  {
    // fill the reply struct, and then send it to the network's broadcast address
    Serial.print("POLL nb ");
    Serial.print(i);
    Serial.print(" from ");
    Serial.print(remoteIP);
    Serial.print(" broadcast addr: ");
    Serial.println(broadcast);

#if !defined(ARDUINO_SAMD_ZERO) && !defined(ESP8266) && !defined(ESP32)
    IPAddress local_ip = Ethernet.localIP();
#else
    IPAddress local_ip = WiFi.localIP();
#endif
    node_ip_address[0] = local_ip[0];
    node_ip_address[1] = local_ip[1];
    node_ip_address[2] = local_ip[2];
    node_ip_address[3] = local_ip[3];

    sprintf((char *)id, "Art-Net");
    memcpy(ArtPollReply.id, id, sizeof(ArtPollReply.id));
    memcpy(ArtPollReply.ip, node_ip_address, sizeof(ArtPollReply.ip));

    ArtPollReply.opCode = ART_POLL_REPLY;
    ArtPollReply.port = ART_NET_PORT;

    memset(ArtPollReply.goodinput, 0x08, 4);
    memset(ArtPollReply.goodoutput, 0x80, 4);
    memset(ArtPollReply.porttypes, 0xc0, 4);

    uint8_t shortname[18];
    uint8_t longname[64];
    // change shortname to Teensy artnet + i
    //  in order to have artnet1, arntet2, artnet3, artnet4
    sprintf((char *)shortname, "artnet arduino %i", i);

    sprintf((char *)longname, "Art-Net -> Arduino Bridge");
    memcpy(ArtPollReply.shortname, shortname, sizeof(shortname));
    memcpy(ArtPollReply.longname, longname, sizeof(longname));

    ArtPollReply.etsaman[0] = 0;
    ArtPollReply.etsaman[1] = 0;
    ArtPollReply.verH = 1;
    ArtPollReply.ver = 0;
    ArtPollReply.subH = 0;
    ArtPollReply.sub = 0;
    ArtPollReply.oemH = 0;
    ArtPollReply.oem = 0xFF;
    ArtPollReply.ubea = 0;
    ArtPollReply.status = 0xd2;
    ArtPollReply.swvideo = 0;
    ArtPollReply.swmacro = 0;
    ArtPollReply.swremote = 0;
    ArtPollReply.style = 0;

    ArtPollReply.numbportsH = 0;
    ArtPollReply.numbports = 4;
    ArtPollReply.status2 = 0x08;

    ArtPollReply.bindip[0] = node_ip_address[0];
    ArtPollReply.bindip[1] = node_ip_address[1];
    ArtPollReply.bindip[2] = node_ip_address[2];
    ArtPollReply.bindip[3] = node_ip_address[3];

    /*
    uint8_t swin[4] = {0x07, 0x08, 0x09, 0x0A};
      uint8_t swout[4] = {0x07, 0x08, 0x09, 0x0A};
      for (uint8_t i = 0; i < 4; i++)
      {
        ArtPollReply.swout[i] = swout[i];
        ArtPollReply.swin[i] = swin[i];
      }
    */

    // change this for loop, to match the main for loop, and start universe
    // swout[j] = startUniverse + i*4 + j;
    // swin[j] = startUniverse + i*4 + j;

    for (int j = 0; j < 4; j++)
    {
      ArtPollReply.swout[j] = (uint8_t)(startUniverse + i * 4 + j);
      ArtPollReply.swin[j] = (uint8_t)startUniverse + i * 4 + j;
    }

    sprintf((char *)ArtPollReply.nodereport, "%i DMX output universes active.", ArtPollReply.numbports);
    Udp.beginPacket(broadcast, ART_NET_PORT); // send the packet to the broadcast address
    Udp.write((uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
    Udp.endPacket();
    delay(10);
  }
}

void Artnet::printPacketHeader()
{
  Serial.print("packet size = ");
  Serial.print(packetSize);
  Serial.print("\topcode = ");
  Serial.print(opcode, HEX);
  Serial.print("\tuniverse number = ");
  Serial.print(incomingUniverse);
  Serial.print("\tdata length = ");
  Serial.print(dmxDataLength);
  Serial.print("\tsequence n0. = ");
  Serial.println(sequence);
}

void Artnet::printPacketContent()
{
  for (uint16_t i = ART_DMX_START; i < dmxDataLength; i++)
  {
    Serial.print(artnetPacket[i], DEC);
    Serial.print("  ");
  }
  Serial.println('\n');
}
