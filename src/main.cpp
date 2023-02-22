#include <Arduino.h>
#include "SD.h"
//#include <SD_MMC.h>
#include "littleFS_helper.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#define HOSTNAME Hostname
//UDP
#include <WiFiUdp.h>
WiFiUDP UDP;
char packet[255]; //for incoming packet
char reply[] = "SmartDisplay1 received"; //create reply
#include <DNSServer.h>
#include <WiFiManager.h>  
#include <WebServer.h>
#include "ESP32FtpServer.h"
#include <WiFiUdp.h>
#include <esp_task_wdt.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

#define TP_IRQ 25
#define CALIBRATION_FILE "/calibrationData"
#define DEFAULT_NTP_SERVER_1                      "1.si.pool.ntp.org"
#define DEFAULT_NTP_SERVER_2                      "2.si.pool.ntp.org"
#define DEFAULT_NTP_SERVER_3                      "3.si.pool.ntp.org"
#define TIMEZONE TIMEZONE_set
#if (FontsFS == 1)
  #define FONTSFS LittleFS
#elif (FontsFS == 2)
  #define FONTSFS SD
#endif
#if (ButtonsFS == 1)
  #define BUTTONSFS LittleFS
#elif (ButtonsFS == 2)
  #define BUTTONSFS SD
#endif
#if (JpgFS == 1)
  #define JPGFS LittleFS
#elif (JpgFS == 2)
  #define JPGFS SD
#endif

#include "version_of_servers.h"
#include "dmesg_functions.h"
#include "perfMon.h"  
#include "file_system.h"
#include "time_functions.h"               // file_system.h is needed prior to #including time_functions.h if you want to store the default parameters
//#include "ftpClient.h"                    // file_system.h is needed prior to #including ftpClient.h if you want to store the default parameters

struct tm timeinfo;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
WiFiManager wifiManager;
WebServer server(80);
FtpServer ftpSrv;
TaskHandle_t CPU0;
hw_timer_t * timer = NULL;


bool got_time=false;
bool tpr_loop = false;
uint16_t x, y;
int StrPos=0;
int p[8];
char * tohost;  //The host to send the UDP to (touch coordinates)



portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

/** hardware timer to count secs */
volatile int interruptCounter;  //secs; int(max) = 2.147.483.647; a day = 864000 secs, so over 248540 days or 680 years

void IRAM_ATTR onTimer() {  //runs every sec
  portENTER_CRITICAL_ISR(&timerMux);    //portenter and exit are needed as to block it for other processes
    interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux); 
}

void UDPsend(const char* sender, const char* msg, int x, int y)
{
  UDP.beginPacket(sender, UDP_port);
  // UDP.printf("TJpgDec.drawFsJpg(10,100,/Buttons/Button_Download_Red.jpg");
  if(x==-1&&y==-1){UDP.printf(msg); } else {UDP.printf("%s, %d, %d",msg,x,y);}
  UDP.endPacket();
}

void UDPsend(const char* msg, int x, int y)
{
  UDP.beginPacket("Testesp", UDP_port);
  // UDP.printf("TJpgDec.drawFsJpg(10,100,/Buttons/Button_Download_Red.jpg");
  if(x==-1&&y==-1){UDP.printf(msg); } else {UDP.printf("%s, %d, %d",msg,x,y);}
  UDP.endPacket();
}

// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void UDP_Check(void)
{
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    // read the packet into packetBufffer
    int len = UDP.read(packet, 255);
    if (len > 0) {
      packet[len] = 0;
    }
    Serial.print("Contents:"); Serial.println(packet);
    String StrPacket= String(packet);
    if(!StrPacket.startsWith("Received"))  //No reply to a reply
    {
      // send a reply, to the IP address and port that sent us the packet we received
      UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
      UDP.printf("Received reply: %s data from %s", reply, UDP.remoteIP().toString());
      UDP.endPacket();
      StrPos=StrPacket.indexOf(")");  // Find and remove last )
      StrPacket=StrPacket.substring(0, StrPos);
      StrPos=StrPacket.indexOf(".");
      String StrCommand=StrPacket.substring(0, StrPos);
      String rest=StrPacket.substring(StrPos+1,255);
      if(StrCommand=="tft") //parameters are int type
      {
        StrPos=rest.indexOf("(");
        StrCommand=rest.substring(0, StrPos);
        rest=rest.substring(StrPos+1,255);
        String str=rest;

        p[0]=0;
        for(int i=1; i<8; i++)
        {
          StrPos=rest.indexOf(",");
          p[i] = rest.substring(0,StrPos).toInt();
          rest=rest.substring(StrPos+1,255);
        }
        if(StrCommand=="drawPixel")     {tft.drawPixel(p[1],p[2],p[3]); return;}
        if(StrCommand=="drawChar")      {tft.drawChar(p[1],p[2],p[3],p[4],p[5], p[6]); return;}
        if(StrCommand=="drawLine")      {tft.drawLine(p[1],p[2],p[3],p[4],p[5]); return;}
        if(StrCommand=="drawFastVLine") {tft.drawFastVLine(p[1],p[2],p[3],p[4]); return;}
        if(StrCommand=="drawFastHLine") {tft.drawFastHLine(p[1],p[2],p[3],p[4]); return;}
        if(StrCommand=="fillRect")      {tft.fillRect(p[1],p[2],p[3],p[4],p[5]); return;}
        if(StrCommand=="setRotation")   {tft.setRotation(p[1]);return;}
        if(StrCommand=="fillScreen")    {tft.fillScreen(p[1]); return;}
        if(StrCommand=="drawRect")      {tft.drawRect(p[1],p[2],p[3],p[4],p[5]); return;}
        if(StrCommand=="drawRoundRect")      {tft.drawRoundRect(p[1],p[2],p[3],p[4],p[5],p[6]); return;}
        if(StrCommand=="fillRoundRect")      {tft.fillRoundRect(p[1],p[2],p[3],p[4],p[5],p[6]); return;}
        if(StrCommand=="fillRectVGradient")  {tft.fillRectVGradient(p[1],p[2],p[3],p[4],p[5],p[6]); return;}
        if(StrCommand=="fillRectHGradient")  {tft.fillRectHGradient(p[1],p[2],p[3],p[4],p[5],p[6]); return;}
        if(StrCommand=="drawCircle")  {tft.drawCircle(p[1],p[2],p[3],p[4]); return;}
        if(StrCommand=="drawCircleHelper")  {tft.drawCircleHelper(p[1],p[2],p[3],p[4],p[5]); return;}
        if(StrCommand=="fillCircle")  {tft.fillCircle(p[1],p[2],p[3],p[4]); return;}
        if(StrCommand=="fillCircleHelper")  {tft.fillCircleHelper(p[1],p[2],p[3],p[4],p[5],p[6]); return;}
        if(StrCommand=="drawEllipse")  {tft.drawEllipse(p[1],p[2],p[3],p[4],p[5]); return;}
        if(StrCommand=="drawTriangle")  {tft.drawTriangle(p[1],p[2],p[3],p[4],p[5],p[6],p[7]); return;}
        if(StrCommand=="fillTriangle")  {tft.fillTriangle(p[1],p[2],p[3],p[4],p[5],p[6],p[7]); return;}
        if(StrCommand=="drawNumber")      {tft.drawNumber(p[1],p[2],p[3]); return;}
        if(StrCommand=="drawFloat")      {tft.drawFloat(p[1],p[2],p[3],p[4]); return;}
        if(StrCommand=="drawString")      {StrPos=str.indexOf(",");str=str.substring(0, StrPos);tft.drawString(str,p[2],p[3]); return;}
        if(StrCommand=="setCursor")     {tft.setCursor(p[1],p[2]); UDP_Check();}  //run again for print or println
        if(StrCommand=="print")         {tft.print(str); return;}  //with setCursor
        if(StrCommand=="println")       {tft.println(str); return;}
        

        if(StrCommand=="loadFont")     {tft.loadFont("Fonts/Arial"+String(p[1]), FONTSFS); return;}  //All ttf-fonts same name, number p[1] is the size
        if(StrCommand=="brightness")   {analogWrite(TFT_BL, p[1]); ; return;}
      }
      if(StrCommand=="spr")   //Sprite, just text for now.
      {
        StrPos=StrPacket.indexOf("(");
        StrCommand=StrPacket.substring(0, StrPos);
        rest=StrPacket.substring(StrPos+1,255);
        p[0]=0;
        for(int i=1; i<3; i++)
        {
          StrPos=rest.indexOf(",");
          p[i] = rest.substring(0,StrPos).toInt();
          rest=rest.substring(StrPos+1,255);
        }
        if(StrCommand=="printToSprite") {spr.printToSprite(rest); return;}
        if(StrCommand=="setTextColor") {spr.setTextColor(p[1],p[2]); return;}
        if(StrCommand=="loadFont")     {spr.loadFont("Fonts/Arial"+p[1], FONTSFS);return;}
      }
      if(StrCommand=="TJpgDec") //Show JPG; parameters are 2x int and a string
      {
        StrPos=StrPacket.indexOf("(");
        StrCommand=StrPacket.substring(0, StrPos);
        rest=StrPacket.substring(StrPos+1,255);
        p[0]=0;
        for(int i=1; i<3; i++)
        {
          StrPos=rest.indexOf(",");
          p[i] = rest.substring(0,StrPos).toInt();
          rest=rest.substring(StrPos+1,255);
        }
        if(StrCommand=="drawFsJpg")     {TJpgDec.drawFsJpg(p[1],p[2],"", SD_MMC);return;}
      }
      if(StrCommand=="FTP")
      {
        StrPos=rest.indexOf("(");
        StrCommand=rest.substring(0, StrPos);
        rest=rest.substring(StrPos+1,255);
        if(StrCommand=="LittleFS")
        {
          ftpSrv.begin(LittleFS, FTP_USERNAME, FTP_PASSWORD); // username, password for ftp.
        }
        if(StrCommand=="SD")
        {
          ftpSrv.begin(SD, FTP_USERNAME, FTP_PASSWORD); // username, password for ftp.
        }
      }
    }
  }
}

void cronHandler (char *cronCommand) {
  // this callback function is supposed to handle cron commands/events that occur at time specified in crontab

    #define cronCommandIs(X) (!strcmp (cronCommand, X))  

          // ----- examples: handle your cron commands/events here - delete this code if it is not needed -----
          
          if (cronCommandIs ("gotTime"))                        { // triggers only once - the first time ESP32 sets its clock (when it gets time from NTP server for example)
                                                                  time_t t = getLocalTime ();
                                                                  struct tm st = timeToStructTime (t);
                                                                  Serial.println ("Got time at " + timeToString (t) + " (local time), do whatever needs to be done the first time the time is known.");
                                                                  got_time=true;
                                                                }           
                                                                                                                                
          // handle also other cron commands/events you may have

}

void Calibrate_TP(void)
{
  uint16_t calibrationData[5];
  uint8_t calDataOK = 0;
  if (LittleFS.exists(CALIBRATION_FILE))
  {
    File f = LittleFS.open(CALIBRATION_FILE);
    if (f)
    {
      if (f.readBytes((char *)calibrationData, 14) == 14)
        calDataOK = 1;
      f.close();
    }
  }
  if (calDataOK)
  {
    // calibration data valid
    tft.setTouch(calibrationData);
    Serial.println("Cal OK");
  }
  else
  {
    // data not valid. recalibrate
    tft.fillScreen((0xFFFF));
    tft.setCursor(20, 0, 2);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(1);
    tft.println("calibration run");
    Serial.println("Cal recal");
    tft.calibrateTouch(calibrationData, TFT_WHITE, TFT_RED, 15);
    // store data
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f)
    {
      f.write((const unsigned char *)calibrationData, 14);
      Serial.println("Cal recal write");
      f.close();
    }
  }
}

void TP_loop()
{
  char * UDPmsg;
  if (!digitalRead(TP_IRQ))
  {
    if (tpr_loop)
    {
      tpr_loop = false;
      Serial.println("Pressed");
      //UDPsend("pressed",-1,-1);
      if (Pressedxy)
      {
        tft.getTouch(&x, &y);
        Serial.printf("at x = %i, y = %i\n", x, y);
        if(y>=280){UDPsend(Sender2, "xy-touched", x, y);} else {UDPsend(Sender2, "xy-touched", x, y);}
      }
    }
  }
  else
  {
    if (!tpr_loop)
    {
      Serial.println("Released");
      //UDPsend("released",-1,-1);
      if (Releasedxy)
      {
        tft.getTouch(&x, &y);
        Serial.printf("at x = %i, y = %i\n", x, y);
        UDPsend("xy",-1,-1);
      }
    }
    tpr_loop = true;
  }
}

void CPU0code( void * pvParameters ){         /*2nd task and wifi on CPU0*/
  esp_task_wdt_init(30, false);               //wdt 30 sec and no reset, just a message in the monitor for this task (CPU0)
  Serial.print("TP_loop running on core "); Serial.println(xPortGetCoreID());
  for(;;){
    TP_loop(); 
    TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE; // write enable
    TIMERG0.wdt_feed=1;                       // feed wdt
    TIMERG0.wdt_wprotect=0;                   // write protect
    ftpSrv.handleFTP();
  }
}

bool get_time(void)
{
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return false;
  }
  else
  {
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    return true;
  }
}

void setup(void)
{
  Serial.begin(115200);
  // wifiManager.resetSettings();    /*For testing*/
  pinMode(TFT_BL, OUTPUT);
  pinMode(TP_IRQ, INPUT);
  pinMode(5, OUTPUT);
  analogWrite(TFT_BL, 100);                                 // Back light
  Serial.println("\n\n##################################"); /*Just info*/
  Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
  Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
  Serial.println("##################################\n\n");
  if (psramInit())
  {
    Serial.println("\nPSRAM is correctly initialized");
  }
  else
  {
    Serial.println("PSRAM not available");
  }
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  listDir(LittleFS, "/", 1);
  Serial.print("Flash storage total = ");
  Serial.println(LittleFS.totalBytes());
  Serial.print("Flash storage used = ");
  Serial.println(LittleFS.usedBytes());
  Serial.println("setup      : Init SD card");
  if(FontsFS==2||ButtonsFS==2||JpgFS==2)   //if SD is used
  {
    SPI.begin(18,19,23);    //This is standard VSPI, so make sure TFT_eSPI uses HSPI
    if(!SD.begin(5)){
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    listDir(SD, "/", 0);
  }
  tft.init();
  tft.setRotation(3);
  Calibrate_TP();
  spr.setColorDepth(16); // 16 bit colour needed to show antialiased fonts
  tft.setTextDatum(BL_DATUM);   //Bottom-Left start of text
  tft.fillScreen((0xF000));
  tft.setCursor(5,20);
  spr.setTextDatum(BL_DATUM);
  TJpgDec.setJpgScale(1); // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setSwapBytes(true); // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setCallback(tft_output);    //name of rendering function
  wifiManager.setConnectRetries(Conn_retry);
  wifiManager.setHostname(HOSTNAME);
  wifiManager.autoConnect(WiFiAP);
  Serial.print("WiFi RSSI = ");
  Serial.println(WiFi.RSSI());
  Serial.print("WiFi Quality = ");
  Serial.println(wifiManager.getRSSIasQuality(WiFi.RSSI()));
  startCronDaemon (cronHandler);
  cronTabAdd ("* * * * * * gotTime");  // triggers only once - when ESP32 reads time from NTP servers for the first time
  //cronTabAdd ("0 0 0 1 1 * newYear'sGreetingsToProgrammer");  // triggers at the beginning of each year
  //cronTabAdd ("0 * * * * * onMinute");  // triggers each minute at 0 seconds
  timer = timerBegin(0, 80, true); // start at 0; divider for 80 MHz = 80 so we have 1 MHz timer; count up = true; timers are 64 bits
  timerAttachInterrupt(timer, &onTimer, false);   //edge doesn't work propperly on esp32, so false here
  timerAlarmWrite(timer, 1000000, true); // 1000000 = writes an alarm, that triggers an interupt, every 1 sec with divider 80
  timerAlarmEnable(timer);
  Serial.println("setup done");
  Serial.print("loop running on core ");
  Serial.println(xPortGetCoreID());
  // Begin listening to UDP port
  UDP.begin(UDP_port);
  Serial.print("Listening on UDP port ");
  Serial.println(UDP_port);
  ftpSrv.begin(LittleFS, FTP_USERNAME, FTP_PASSWORD); // username, password for ftp.
  IPAddress ip = WiFi.localIP();
  char myip[20];
  sprintf(myip, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  writeFile(LittleFS, "/ip.txt", myip);

  
  xTaskCreatePinnedToCore(
      CPU0code,                 /* Task function. */
      "CPU0",                   /* name of task. */
      10000,                    /* Stack size of task */
      NULL,                     /* parameter of the task */
      6,                        /* priority of the task */
      &CPU0,                    /* Task handle to keep track of created task */
      0);                       /* pin task to core 0 */
  esp_task_wdt_init(30, false); // wdt 30 sec and no reset, just a message in the monitor for this task (CPU1)

}

void loop()
{
  if (interruptCounter > 0)
  {
    interruptCounter--;
    if (Show_time)
    {
      spr.loadFont(Time_Font, FONTSFS);
      spr.setTextColor(Time_Font_Color, Time_Background_Color);
      tft.setCursor(TimeX, TimeY);
      spr.printToSprite(timeToStringHMS(getLocalTime()).c_str());
    }
  }
  TIMERG1.wdt_wprotect = TIMG_WDT_WKEY_VALUE; // write enable
  TIMERG1.wdt_feed = 1;                       // feed wdt
  TIMERG1.wdt_wprotect = 0;                   // write protect
  if(got_time)  //check incomming UDP
  {
    UDP_Check();
  }
}
