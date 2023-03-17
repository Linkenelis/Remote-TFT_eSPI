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
char packet[1023]; //for incoming packet
char reply[] = "Received by SmartDisplay1"; //create reply
int packetnumber=0;
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
#define DEFAULT_NTP_SERVER_1                      NTP_pool
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

#include "Arial22num.h"
#include "Arial50num.h"
#include "Arial150num.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
WiFiManager wifiManager;
WebServer server(80);
FtpServer ftpSrv;
TaskHandle_t CPU0;
hw_timer_t * timer = NULL;
struct tm timeinfo;
time_t t;

bool TP=true;
bool ready_for_TP=true;
int last_TP=0;
int sprfont;
int previous_sec = 100;
int previous_day;
bool got_time=false;
bool tpr_loop = false;
uint16_t x, y;
int StrPos=0;
int p[8];
char * tohost;  //The host to send the UDP to (touch coordinates)
int SHOW_TIME=Show_time;
int ANA_TIME=UseAnalogClock;
int DIGI_TIME=UseDigitalClock;
//for analog clock
  float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;    // Saved H, M, S x & y multipliers
  float sdeg=0, mdeg=0, hdeg=0;
  uint16_t osx=160, osy=160, omx=160, omy=160, ohx=160, ohy=160;  // Saved H, M, S x & y coords
  uint16_t x0=0, x1=0, yy0=0, yy1=0;
  uint32_t targetTime = 0;                    // for next 1 second timeout
  static uint8_t conv2d(const char* p); // Forward declaration needed for IDE 1.6.x
  void anaclock_once(void);
  uint8_t hh=conv2d(__TIME__), mm=conv2d(__TIME__+3), ss=conv2d(__TIME__+6);  // Get H, M, S from compile time
  boolean initial = 1;
  int mday=0;
  bool anaclock_once_has_run=false;


//portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

/** hardware timer to count secs */
volatile int interruptCounter;  //secs; int(max) = 2.147.483.647; a day = 864000 secs, so over 248540 days or 680 years

void IRAM_ATTR onTimer() {  //runs every sec
  //portENTER_CRITICAL_ISR(&timerMux);    //portenter and exit are needed as to block it for other processes
    interruptCounter++;
  //portEXIT_CRITICAL_ISR(&timerMux); 
}

void UDPsend(const char* sender, const char* msg, int packetnumber, int x, int y)
{
  UDP.beginPacket(sender, UDP_port);
  // UDP.printf("TJpgDec.drawFsJpg(10,100,/Buttons/Button_Download_Red.jpg");
  if(x==-1&&y==-1){UDP.printf(msg); } else {UDP.printf("%s, %d, %d, %d",msg,packetnumber,x,y);}
  UDP.endPacket();
}


void UDPsend(const char* msg, int packetnumber, int x, int y)
{
  UDP.beginPacket("Testesp", UDP_port);
  // UDP.printf("TJpgDec.drawFsJpg(10,100,/Buttons/Button_Download_Red.jpg");
  if(x==-1&&y==-1){UDP.printf(msg); } else {UDP.printf("%s, %d, %d, %d",msg,packetnumber,x,y);}
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
    // read the packet into packetBuffer
    int len = UDP.read(packet, 1023);
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
      //how many messages?
      String TotalPacket(packet);
      String displaymsg[10];  //max 10 (short) display messages in 1 packet (255 max in total packet)
      int countlines=0;
      StrPos=TotalPacket.indexOf("\n");
      for(int i=0; i<10; i++)
      {
        if(StrPos>0)
        {
          displaymsg[countlines]=TotalPacket.substring(0, StrPos);
          countlines++;
          TotalPacket=TotalPacket.substring(StrPos+1, 255);
          StrPos=TotalPacket.indexOf("\n");
        }
      }
      for(int j=0; j<countlines; j++)
      {
        StrPos=displaymsg[j].indexOf(")");  // Find and remove last )
        StrPacket=displaymsg[j].substring(0, StrPos);
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
          if(StrCommand=="fillRect")      {tft.fillRect(p[1],p[2],p[3],p[4],p[5]);}
          if(StrCommand=="drawNumber")    {tft.drawNumber(p[1],p[2],p[3]); }
          if(StrCommand=="drawFloat")     {tft.drawFloat(p[1],p[2],p[3],p[4]); }
          if(StrCommand=="drawString")    {StrPos=str.indexOf(",");str=str.substring(0, StrPos);tft.drawString(str,p[2],p[3]); }
          if(StrCommand=="setCursor")     {tft.setCursor(p[1],p[2]); UDP_Check();}  //run again for print or println
          if(StrCommand=="print")         {tft.print(str); }  //with setCursor
          if(StrCommand=="println")       {tft.println(str); }
          if(StrCommand=="setTextColor")  {tft.setTextColor(p[1], p[2]); }
          if(StrCommand=="loadFont")      {tft.loadFont("Fonts/Arial"+String(p[1]), FONTSFS); }  //All ttf-fonts same name, number p[1] is the size
          if(StrCommand=="unloadFont")    {tft.unloadFont(); }
          if(StrCommand=="TextWrap")      {tft.setTextWrap(p[1]);}
          if(StrCommand=="brightness")    {analogWrite(TFT_BL, p[1]); }
          if(StrCommand=="showTime")      {SHOW_TIME=p[1];}
          if(StrCommand=="drawPixel")     {tft.drawPixel(p[1],p[2],p[3]); }
          if(StrCommand=="drawChar")      {tft.drawChar(p[1],p[2],p[3],p[4],p[5], p[6]); }
          if(StrCommand=="drawLine")      {tft.drawLine(p[1],p[2],p[3],p[4],p[5]); }
          if(StrCommand=="drawFastVLine") {tft.drawFastVLine(p[1],p[2],p[3],p[4]); }
          if(StrCommand=="drawFastHLine") {tft.drawFastHLine(p[1],p[2],p[3],p[4]); }
          if(StrCommand=="setRotation")   {tft.setRotation(p[1]);}
          if(StrCommand=="fillScreen")    {tft.fillScreen(p[1]); }
          if(StrCommand=="drawRect")      {tft.drawRect(p[1],p[2],p[3],p[4],p[5]); }
          if(StrCommand=="drawRoundRect")     {tft.drawRoundRect(p[1],p[2],p[3],p[4],p[5],p[6]); }
          if(StrCommand=="fillRoundRect")     {tft.fillRoundRect(p[1],p[2],p[3],p[4],p[5],p[6]); }
          if(StrCommand=="fillRectVGradient") {tft.fillRectVGradient(p[1],p[2],p[3],p[4],p[5],p[6]); }
          if(StrCommand=="fillRectHGradient") {tft.fillRectHGradient(p[1],p[2],p[3],p[4],p[5],p[6]); }
          if(StrCommand=="drawCircle")        {tft.drawCircle(p[1],p[2],p[3],p[4]); }
          if(StrCommand=="drawCircleHelper")  {tft.drawCircleHelper(p[1],p[2],p[3],p[4],p[5]); }
          if(StrCommand=="fillCircle")        {tft.fillCircle(p[1],p[2],p[3],p[4]); }
          if(StrCommand=="fillCircleHelper")  {tft.fillCircleHelper(p[1],p[2],p[3],p[4],p[5],p[6]); }
          if(StrCommand=="drawEllipse")   {tft.drawEllipse(p[1],p[2],p[3],p[4],p[5]); }
          if(StrCommand=="drawTriangle")  {tft.drawTriangle(p[1],p[2],p[3],p[4],p[5],p[6],p[7]); }
          if(StrCommand=="fillTriangle")  {tft.fillTriangle(p[1],p[2],p[3],p[4],p[5],p[6],p[7]); }
        }
        if(StrCommand=="spr")   //Sprite, just text for now.
        {
          StrPos=StrPacket.indexOf("(");
          StrCommand=rest.substring(0, StrPos);
          rest=rest.substring(StrPos+1,255);
          p[0]=0;
          for(int i=1; i<3; i++)
          {
            StrPos=rest.indexOf(",");
            p[i] = rest.substring(0,StrPos).toInt();
            rest=rest.substring(StrPos+1,255);
          }
          if(StrCommand=="printToSprite") {spr.printToSprite(rest,p[1], p[2]); }
          if(StrCommand=="setTextColor") {spr.setTextColor(p[1],p[2]); }
          if(StrCommand=="loadFont")     {spr.loadFont("Fonts/Arial"+p[1], FONTSFS);}
          if(StrCommand=="unloadFont")   {spr.unloadFont(); }
        }
        if(StrCommand=="TJpgDec") //Show JPG; parameters are 2x int and a string
        {
          StrPos=rest.indexOf("(");
          StrCommand=rest.substring(0, StrPos);
          rest=rest.substring(StrPos+1,255);
          p[0]=0;
          for(int i=1; i<3; i++)
          {
            StrPos=rest.indexOf(",");
            p[i] = rest.substring(0,StrPos).toInt();
            rest=rest.substring(StrPos+1,255);
          }
          if(StrCommand=="drawFsJpg")     {TJpgDec.drawFsJpg(p[1],p[2], rest, LittleFS);}   // /in name then LittleFS; withou SD
          if(StrCommand=="drawSdJpg")     {TJpgDec.drawFsJpg(p[1],p[2], rest, SD);}
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
        if(StrCommand=="setClock")
        {
          StrPos=rest.indexOf("(");
          StrCommand=rest.substring(0, StrPos);
          rest=rest.substring(StrPos+1,255);
          String Alarm=rest;
          p[0]=0;
          for(int i=1; i<3; i++)
          {
            StrPos=rest.indexOf(",");
            p[i] = rest.substring(0,StrPos).toInt();
            rest=rest.substring(StrPos+1,255);
          }
          if(StrCommand=="Show_time") {SHOW_TIME=p[1];sprfont=0;}
          if(StrCommand=="Analog")    {ANA_TIME=p[1];anaclock_once_has_run=false;}
          if(StrCommand=="Digital")   {DIGI_TIME=p[1];previous_sec=100;}
          if(StrCommand=="Alarm")     {cronTabAdd (Alarm.c_str());}
        }
      }
    } else {
          String rest=StrPacket;
          for(int i=0; i<2; i++)
          {
            StrPos=rest.indexOf(",");
            p[i] = rest.substring(0,StrPos).toInt();
            rest=rest.substring(StrPos+1,255);
          }
          if (p[1]==packetnumber){ready_for_TP=true;} //our message is received, ready for next touch
    }
  }
}

void cronHandler (char *cronCommand) {
  // this callback function is supposed to handle cron commands/events that occur at time specified in crontab

    #define cronCommandIs(X) (!strcmp (cronCommand, X))  

          // ----- examples: handle your cron commands/events here - delete this code if it is not needed -----
          
          if (cronCommandIs ("gotTime"))                        { // triggers only once - the first time ESP32 sets its clock (when it gets time from NTP server for example)
                                                                  t = getLocalTime ();
                                                                  timeinfo = timeToStructTime (t);
                                                                  Serial.println ("Got time at " + timeToString (t) + " (local time), do whatever needs to be done the first time the time is known.");
                                                                  got_time=true;
                                                                  tft.println("We have the time.");
                                                                  if(ANA_TIME >0) {anaclock_once();}
                                                                }         
          if (cronCommandIs ("Alarm"))                        { // triggers on set alarm time date
                                                                  Serial.println("Alarm!!!");
                                                                  UDP.beginPacket(Sender1, UDP_port);
                                                                  UDP.print("Alarm triggered");
                                                                  UDP.endPacket();
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

  TP = digitalRead(TP_IRQ);
  if (!TP)
  {
    TP = true;
    if (ready_for_TP) //reset by received packetnumber or 3 sec in loop, whatever is first
    {
      ready_for_TP = false;
      last_TP = millis();
        if (tft.getTouch(&x, &y))
        {
          Serial.printf("at x = %i, y = %i\n", x, y);
          packetnumber++;
          if (packetnumber > 99)  {packetnumber = 1;}
          UDPsend(Sender1, "xy-touched", packetnumber, x, y);
        }
    }
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
    delay(50);                                  // some time for processes like WiFi; too low will generate errors
    ftpSrv.handleFTP();                         //
    delay(50);                                  //sometimes xsemaphore Queue? on quick touches
  }
}

static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

void anaclock()
{
  if (targetTime < millis()) {
    targetTime += 1000;
    ss++;              // Advance second
    if (ss==60) {
      ss=0;
      mm++;            // Advance minute
      if(mm>59) {
        mm=0;
        hh++;          // Advance hour
        if (hh>23) {
          hh=0;
          spr.setTextColor(TFT_WHITE, TFT_BLACK);
          if(sprfont!=32) {spr.loadFont("Fonts/Arial32", FONTSFS);}
          spr.printToSprite(timeToStringD(getLocalTime()).c_str(), 0, 70);
          spr.printToSprite(timeToStringY(getLocalTime()).c_str(), 390, 70);
          spr.printToSprite(timeToStringDOW(getLocalTime()).c_str(), 0, 2); //translate DayOfWeek and MONth_name in time_functions.h
          spr.printToSprite(timeToStringMON(getLocalTime()).c_str(),  480-spr.textWidth(timeToStringMON(getLocalTime()).c_str()), 2);
          sprfont=32;
        }
      }
    }
    if (ss==30) //adjust if needed, should not be often and just 1 sec adjustment
    {
      t = getLocalTime ();
      timeinfo = timeToStructTime (t);
      ss=timeinfo.tm_sec;
    }
    // Pre-compute hand degrees, x & y coords for a fast screen update
    sdeg = ss*6;                  // 0-59 -> 0-354
    mdeg = mm*6+sdeg*0.01666667;  // 0-59 -> 0-360 - includes seconds
    hdeg = hh*30+mdeg*0.0833333;  // 0-11 -> 0-360 - includes minutes and seconds
    hx = cos((hdeg-90)*0.0174532925);    
    hy = sin((hdeg-90)*0.0174532925);
    mx = cos((mdeg-90)*0.0174532925);    
    my = sin((mdeg-90)*0.0174532925);
    sx = cos((sdeg-90)*0.0174532925);    
    sy = sin((sdeg-90)*0.0174532925);
    if (ss==0 || initial) {
      initial = 0;
      // Erase hour and minute hand positions every minute
      tft.drawWideLine(ohx, ohy,240,161,9, TFT_BLACK,TFT_BLACK);
      ohx = hx*90+241;    
      ohy = hy*90+161;
      tft.drawWideLine(omx, omy,240,161,5, TFT_BLACK,TFT_BLACK);
      omx = mx*115+240;    
      omy = my*115+161;
    }
      // Redraw new hand positions, hour and minute hands not erased here to avoid flicker
      tft.drawWideLine(osx, osy, 240, 161,3, TFT_BLACK,TFT_BLACK);
      osx = sx*133+241;    
      osy = sy*133+161;
      tft.drawWideLine(ohx, ohy,240,161,9, TFT_WHITE,TFT_BLACK);
      tft.drawWideLine(omx, omy,240,161,5, TFT_WHITE,TFT_BLACK);
      tft.drawWideLine(osx, osy, 240,161,3, TFT_RED,TFT_BLACK);
      tft.drawSmoothArc(240,161,3,0,0,360,TFT_WHITE,TFT_BLACK, false);
  }
}
void anaclock_once()
{
  tft.fillScreen(TFT_BLACK);
  
  // Draw clock face
  tft.drawSmoothArc(240,160,160,153,0,360,TFT_GREEN,TFT_BLACK, false);

  // Draw 12 lines
  for(int i = 0; i<360; i+= 30) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*150+240;
    yy0 = sy*150+160;
    x1 = sx*140+240;
    yy1 = sy*140+160;

    tft.drawWideLine(x0,yy0,x1,yy1,4,TFT_GREEN,TFT_BLACK);
  }

  // Draw 60 dots
  for(int i = 0; i<360; i+= 6) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*142+240;
    yy0 = sy*142+160;
    // Draw minute markers
    tft.drawPixel(x0, yy0, TFT_WHITE);
    
    // Draw main quadrant dots
    if(i==0 || i==180) tft.fillSmoothCircle(x0, yy0, 4, TFT_WHITE, TFT_BLACK);
    if(i==90 || i==270) tft.fillSmoothCircle(x0, yy0, 4, TFT_WHITE, TFT_BLACK);
  }
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  if(sprfont!=32)  {spr.loadFont("Fonts/Arial32", FONTSFS);}
  spr.printToSprite(timeToStringD(getLocalTime()).c_str(), 0, 40);
  spr.printToSprite(timeToStringY(getLocalTime()).c_str(), 390, 40);
  spr.printToSprite(timeToStringDOW(getLocalTime()).c_str(), 0, 2);
  spr.printToSprite(timeToStringMON(getLocalTime()).c_str(),  480-spr.textWidth(timeToStringMON(getLocalTime()).c_str()), 2);
  sprfont=32;
  targetTime = millis() + 1000;
  t = getLocalTime ();
  timeinfo = timeToStructTime (t);
  ss=timeinfo.tm_sec;
  mm=timeinfo.tm_min;
  hh=timeinfo.tm_hour;
  anaclock_once_has_run=true;
  anaclock();
}
void digiclock() //date and time, ip and strength
{
    int xpos = tft.width() / 2; // Half the screen width
    int ypos = 30;
    char timc[12];
    t = getLocalTime ();
    timeinfo = timeToStructTime (t);
    if(previous_sec==100) {tft.fillScreen(0);}
    spr.setTextDatum(C_BASELINE);                 //using spr (=sprite) there is no need for a black rectangle, to wipe out previous. 
    if (previous_day<timeinfo.tm_mday){           //Now there is no interuption in display
      spr.setTextColor(TFT_YELLOW, TFT_BLACK);    //Fonts in a .h file for quick changes?
      spr.loadFont(Arial50num);   
      strftime(timc, sizeof timc, "%d-%m-%Y", &timeinfo);
      tft.setCursor(xpos-((spr.textWidth(timc))/2), ypos); 
      spr.printToSprite(timc);
      previous_day=timeinfo.tm_mday;
    }
    ypos += 130;  // move ypos down
    if (timeinfo.tm_sec == 0 || previous_sec==100)  //previous_sec=100 at start
    {  
      strftime(timc, sizeof timc, " %H:%M ", &timeinfo);
      spr.setTextColor(TFT_WHITE, TFT_BLACK);
      spr.loadFont(Arial150num);
      tft.setCursor(xpos - 50 - ((spr.textWidth(timc))/2), ypos);
      spr.printToSprite(timc);    // Prints to tft cursor position, tft cursor NOT moved

    }
    spr.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    spr.loadFont(Arial50num);
    tft.setCursor(xpos+155, ypos+73);
    strftime(timc, sizeof timc, ":%S ", &timeinfo);
    previous_sec=timeinfo.tm_sec;
    spr.printToSprite(timc);
    spr.unloadFont();
  
}
void Sevenseg(boolean showall){ //show current time on the TFT Display
    /*static String t, oldt = "";
    static boolean k = false;
    uint8_t  i = 0, yOffset = 0;
    uint16_t x, y, space, imgHeigh, imgWidth_l, imgWidth_s;
        x = 11;
        y = 160 + 50;
        yOffset = 0;
        space = 10; // 10px between jpgs
        imgHeigh = 160;
        imgWidth_s = 32;
        imgWidth_l = 96;
    
    if(showall == true) oldt = "";
        t = gettime_s();
        for(i = 0; i < 5; i++) {
            if(t[i] == ':') {
                if(k == false) {k = true; t[i] = 'd';} else{t[i] = 'e'; k = false;}}
            if(t[i] != oldt[i]) {
                
                    sprintf(_chbuf,"/digits/%cgn.jpg",t[i]);
                    drawImage(_chbuf, x, y + yOffset);
            }
            if((t[i]=='d')||(t[i]=='e'))x += imgWidth_s + space; else x += imgWidth_l + space;
        }
        oldt=t;*/

}
void setup(void)
{
  Serial.begin(115200);
  // wifiManager.resetSettings();    /*For testing*/
  pinMode(TP_IRQ, INPUT_PULLUP);
  Serial.print("TP analog="); Serial.println(analogRead(TP_IRQ));
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
 
  if(FontsFS==2||ButtonsFS==2||JpgFS==2)   //if SD is used
  {
    SPI.begin(18,19,23);    //This is standard VSPI, so make sure TFT_eSPI uses HSPI
    SD.end(); 
    Serial.println("setup      : Init SD card");
    SD.begin(5);
    if(!SD.begin(5, SPI, 160000000)){
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
  tft.setRotation(1); //if you change this, then touch calibrate(delete/rename calibrationdata)
  Calibrate_TP();
  spr.setColorDepth(16); // 16 bit colour needed to show antialiased fonts
  tft.setTextDatum(BL_DATUM);   //Bottom-Left start of text
  tft.fillScreen((0xF000));
  tft.setCursor(0,20);
  tft.loadFont("Fonts/Arial20", FONTSFS);
  spr.setTextDatum(BL_DATUM);
  TJpgDec.setJpgScale(1); // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setSwapBytes(true); // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setCallback(tft_output);    //name of rendering function
  tft.println("Starting WiFi...");
  wifiManager.setConnectRetries(Conn_retry);
  wifiManager.setHostname(HOSTNAME);
  wifiManager.autoConnect(WiFiAP);
  Serial.print("WiFi RSSI = ");
  tft.print("WiFi RSSI = ");
  Serial.println(WiFi.RSSI());
  tft.println(WiFi.RSSI());
  Serial.print("WiFi Quality = ");
  tft.print("WiFi Quality = ");
  Serial.println(wifiManager.getRSSIasQuality(WiFi.RSSI()));
  tft.println(wifiManager.getRSSIasQuality(WiFi.RSSI()));
  startCronDaemon (cronHandler);
  cronTabAdd ("* * * * * * gotTime");  // triggers only once - when ESP32 reads time from NTP servers for the first time
  //cronTabAdd ("0 0 0 1 1 * newYear'sGreetingsToProgrammer");  // triggers at the beginning of each year
  //cronTabAdd ("0 * * * * * onMinute");  // triggers each minute at 0 seconds
  //cronTabAdd ("0 0 * * * * onMinute");  // triggers each hour at 0 minutes 0 seconds
  //cronTabAdd ("0 10 7 * * * onMinute");  // triggers each day at 10 minutes past 7 o'clock
  timer = timerBegin(0, 80, true); // start at 0; divider for 80 MHz = 80 so we have 1 MHz timer; count up = true; timers are 64 bits
  timerAttachInterrupt(timer, &onTimer, false);   //edge doesn't work propperly on esp32, so false here
  timerAlarmWrite(timer, 1000000, true); // 1000000 = writes an alarm, that triggers an interupt, every 1 sec with divider 80
  timerAlarmEnable(timer);
  Serial.println("setup done");
  tft.println("setup done");
  tft.println("waiting for a timesync...");
  Serial.print("loop running on core ");
  Serial.println(xPortGetCoreID());
  // Begin listening to UDP port
  UDP.begin(UDP_port);
  Serial.print("Listening on UDP port ");
  Serial.println(UDP_port);
  UDP.setTimeout(500);
  ftpSrv.begin(LittleFS, FTP_USERNAME, FTP_PASSWORD); // username, password for ftp.
  IPAddress ip = WiFi.localIP();
  char myip[20];
  sprintf(myip, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  writeFile(LittleFS, "/ip.txt", myip);
  if (SHOW_TIME)
    {
      spr.loadFont(Time_Font, FONTSFS);
      spr.setTextColor(Time_Font_Color, Time_Background_Color);
    }
  
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
    //portENTER_CRITICAL_ISR(&timerMux);    //portenter and exit are needed as to block it for other processes
    interruptCounter--;
    //portEXIT_CRITICAL_ISR(&timerMux);
    if(got_time)    //show clocks?
    {
      if (SHOW_TIME>0)
      {
        if(sprfont!=20){spr.loadFont(Time_Font, FONTSFS); spr.setTextColor(Time_Font_Color, Time_Background_Color); sprfont=20;}
        spr.printToSprite(timeToStringHMS(getLocalTime()).c_str(), TimeX, TimeY); //changed printToSprite to take x and y, no need for tft.setCursor
      }
      if (ANA_TIME>0)
      {
        if (anaclock_once_has_run){ anaclock();} else {anaclock_once();}
      }
      if (DIGI_TIME>0)
      {
        digiclock();
      }
    }
  }
  TIMERG1.wdt_wprotect = TIMG_WDT_WKEY_VALUE; // write enable
  TIMERG1.wdt_feed = 1;                       // feed wdt
  TIMERG1.wdt_wprotect = 0;                   // write protect
  delay(1);                                    //some time for processes
  if(got_time)                                //check incomming UDP messages
  {
    UDP_Check();
  }
  if(!ready_for_TP){
    if(last_TP+3000<millis()) {ready_for_TP=true;}
  }
}
