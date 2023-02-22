# Remote-TFT_eSPI
TFT_eSPI over UDP

Bodmers TFT_eSPI with esp32, making a remote display over UDP.
I was alway breaking my displays :-) so it started from there.
A 16MB WRover ESP32 (ESP32 module and seperate board) with ili9488 display with touch and SD.
Needs power and WiFi.

Specify the hostname of the display (=SmartDisplay1). Hostnames are used, but ip addresses can be done with a few changes.
Send over UDP looks like UDPsend "tft.drawString(Testing, 10, 250)" (in TFT_eSPI it would be tft.drawString("Testing", 10, 250), Note the difference in "-location

Basics are implemented
Touch coordinates are sent back.
clock can run independed from any sender, as is most common in displays

Multitude of possibillities eg:
-Remote display
-Multiple senders to 1 display (touch can be changed to send to different hosts depending on x and y)
-Sender can be any OS, so they can also be mixed
-ESP32-Devices with WiFi, but lacking SPI-pins like Tasmota, can do UDP via Berry

FTP included to store images (buttons) ttf-Fonts -> send image (jpg) over FTP and then display it
LittleFS and SD, UDPsend "FTP.LittleFS()" or UDPsend "FTP.SD()", to switch FTP server

16MB ESP32 gives more the 12 MB LittleFS storage, enough for most.


Settings are (mostly) in platform.ini

wiring:
TFT_MISO 12
TFT_MOSI 14
TFT_SCLK 13
TFT_CS   32
TFT_DC   33
TFT_BL   27
Touch_CS 26

Make sure TFT_eSPI is set to #define USE_HSPI_PORT (line 372 in User_Setup.h), as VSPI is used for SD
SD_SCK  18
SD_MISO 19
SD_MOSI 23
SD_CS   5

There is no read display (not with ili9488 with touch as TFT_MISO is connected to Touch and not to the display)

example Swiss File Knife in windows cmd: sfk udpsend SmartDisplay1 2000 "tft.brightness(100)"

Example in Arduino:
#include <WiFiUdp.h>
#define UDP_PORT 2000 (can be anything as long as they are known to eachothe. So good to keep them the same)
void UDPsend(char* msg)
{
  UDP.beginPacket(SmartDisplay1, UDP_PORT);
  UDP.printf(msg);
  UDP.endPacket();
}

in setup add after WiFi start:
UDP.begin(UDP_PORT);

Tasmota berry example added.
press bottom of the display to send Tasmota hostname a message, so it knows the ip of the Display (can berry send to host?)
