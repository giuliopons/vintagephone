// VINTAGE ROTARY PHONE RETROFIT
// --------------------------------------------------
// clip audio can be recorded here: https://murf.ai/


// MP3 PLAYER
// ----------------------------------------------------
#include <SoftwareSerial.h>
#include <DFMiniMp3.h>     /* 1.07 */
#define WAIT_END true
#define VOLUME 15
#define PIN_RX_MP3 D6
#define PIN_TX_MP3 D5
int playing = 0;
int mp3_error_code;

// PHONE STATUS
// -----------------------------------------------
byte phoneStatus = 4;
// 0 dialing (or waiting for dialiang, handset up)
// 1 calling
// 2 not valid number
// 3 answering
// 4 hangup (off, handset down)
// 5 ringing


// TIMER
// --------------------------------------------------
#include "RTClib.h"
RTC_Millis rtc;
//int h=0;
//int m=0;
//int s=0;
unsigned long timer_1 = 0;
unsigned long timer_update_rtc_millis = 0;
String caller_1="";

// WEMOS
// ---------------------------------------------------
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#define PIN_HANGUP_SWITCH D1      // switch hang up
#define PIN_ROTARY D8    // rotary encoder pin
#define PIN_BELL_1 D2
#define PIN_BELL_2 D3
const char*   projectname = "vintagephone";
bool          wifi = false;
String phoneNumber = "";


// SPIFFS LIBRARIES
// ----------------------------------------------------
#include <string.h>
#include "FS.h"
bool    spiffsActive = false;





//
// WEMOS FUNCTION, WIFI
// AP, PORTAL, EPROM...
// ----------------------------------------------------

DNSServer dnsServer;
ESP8266WebServer webServer(80);

struct settings {
  char ssid[30];
  char password[30];
  char vars[255];
} userdata = {};




// SPIFFS FUNCTIONS
// -----------------------------------------------------

// Read file from file system 
String readFile(String filename) {
  String out="";
  if (spiffsActive) {
    if (SPIFFS.exists(filename)) {
      File f = SPIFFS.open(filename, "r");
      if (!f) {
        Serial.print("Unable To Open '");
        Serial.print(filename);
        Serial.println("' for Reading");
        Serial.println();
      } else {
        String s;
        
        while (f.position()<f.size())
        {
          s=f.readStringUntil('\n');
          s.trim();
          out = out + s;
          //Serial.println(s);
        } 
        f.close();
      }
   
    } else {
      Serial.print("Unable To Find ");
      Serial.println(filename);
      Serial.println();
    }
  }

  return out;

}


// EEPROM FUNCTIONS
// -----------------------------------------------------

// Read user data from EEPROM (wifi pass...)
void readUserData(){
  EEPROM.begin(sizeof(struct settings) );
  EEPROM.get( 0, userdata );
  

  // force reset ssid/pass
  //  strncpy(userdata.ssid,     "aaa",     sizeof("aaa") );
  //  strncpy(userdata.password, "b", sizeof("b") );
  //  userdata.ssid[3] = userdata.password[1] = '\0';
  //  EEPROM.put(0, userdata);
  //  EEPROM.commit();
}


// WIFI FUNCTIONS
// -----------------------------------------------------

// Try wi-fi connection for "sec" seconds, return true on success
boolean tryWifi(int sec) {
  WiFi.mode(WIFI_STA);
  Serial.println("Try wifi");
  Serial.println(userdata.ssid);
  Serial.println(userdata.password);
  WiFi.begin(userdata.ssid, userdata.password);

  byte tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    if (tries++ > sec) {
      // fail to connect
      Serial.println("");
      return false;
    }
  }
  Serial.println("");
  return true; 
}

// Connect to wifi, tries multiple "times", if fail and startSetupPortal is true launch AP portal
void connectToWifi(int times = 3, bool startSetupPortal = false){
  int i =0;
  while(i<times && wifi==false) {
    wifi = tryWifi(10);
    i++;
  }
  if(!wifi) {
    if(startSetupPortal) setupPortal();
    Serial.println("WIFI not found");
  } else {
    Serial.println("WIFI OK");
    
    if (MDNS.begin(projectname)) {
      Serial.println("MDNS responder started");
    }

  }

}


// AP PORTAL: homepage, load home.html from SPIFFS
void handleHomeMenu() {
    Serial.println("home");
    String o = readFile("/home.html");
    webServer.send(200,   "text/html", o );
}

// AP PORTAL: handle css
void handleCSS() {
    Serial.println("style");
    String o = readFile("/style.css");
    webServer.send(200,   "text/css", o );
}

// AP PORTAL: form that shows SSID/password fields (also POST variables)
void handleFormWifiSetup() {

  if (webServer.method() == HTTP_POST) {

    strncpy(userdata.ssid,     webServer.arg("ssid").c_str(),     sizeof(userdata.ssid) );
    strncpy(userdata.password, webServer.arg("password").c_str(), sizeof(userdata.password) );
    userdata.ssid[webServer.arg("ssid").length()] = userdata.password[webServer.arg("password").length()] = '\0';
    EEPROM.put(0, userdata);
    EEPROM.commit();
    Serial.println("saved");
    String o = readFile("/savedwifisetup.html");
    String css = readFile("/style.css"); 
    o.replace("<link rel=\"stylesheet\" href=\"style.css\">","<style>" + css + "</style>");
    webServer.send(200,   "text/html",  o );

    unsigned timer = millis() + 15000;
    while(millis()<timer) {
      yield();
    }
    Serial.println("restart");
    ESP.restart();
      
  } else {

    Serial.println("form");
    String o = readFile("/wifisetup.html");
    o.replace("#ssid#", userdata.ssid);
    o.replace("#pwd#", userdata.password);
    webServer.send(200,   "text/html", o );
  }
  
}

// AP PORTAL: another form for more settings (meteo url...) (TO DO)
void handleFormSettings() {

  if (webServer.method() == HTTP_POST) {

    //strncpy(userdata.ssid,     webServer.arg("ssid").c_str(),     sizeof(userdata.ssid) );
    //strncpy(userdata.password, webServer.arg("password").c_str(), sizeof(userdata.password) );
    //userdata.ssid[webServer.arg("ssid").length()] = userdata.password[webServer.arg("password").length()] = '\0';
    //EEPROM.put(0, userdata);
    //EEPROM.commit();
    Serial.println("saved settings");
    String o = readFile("/savedsettings.html");
    webServer.send(200,   "text/html",  o );
    
    //Serial.println("restart");
    //delay(5000);
    //ESP.restart();
      
  } else {

    Serial.println("form settings");
    String o = readFile("/settings.html");
    webServer.send(200,   "text/html", o );
  }
  
}

// AP PORTAL: setup the portal
void setupPortal() {
      Serial.println("");
      Serial.println("Start access point");

      IPAddress apIP(172, 217, 28, 1);

      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP(projectname);
    
      // if DNSServer is started with "*" for domain name, it will reply with
      // provided IP to all DNS request
      dnsServer.start(53, "*", apIP);
    
      // replay to all requests with same HTML
      webServer.onNotFound([]() {
        
        //String responseHTML = F("<!DOCTYPE html><html lang='en'><head><meta name='viewport' content='width=device-width'><title>CaptivePortal</title></head><body><h1>Hello World!</h1><p><a href='/'>entra</a></p></body></html>");
        String o = readFile("/home.html");
       
        webServer.send(200, "text/html", o);
      });
      webServer.on("/",  handleHomeMenu);
      webServer.on("/style.css",  handleCSS);

      webServer.on("/wifisetup",  handleFormWifiSetup);
      webServer.on("/settings",  handleFormSettings); 
      webServer.begin();


      // never ending loop waiting access point configuration     
      while(true){
        delay(50);
        dnsServer.processNextRequest();
        webServer.handleClient();

        /*String rx="";
      
        if(Serial.available() > 0) {
          
            rx = Serial.readStringUntil('\n');
            rx = getValue(rx,':',0); // 1:qualunquecosa dopo i :
            if(rx=="stopap") { // stoppa ap
              Serial.println("stop ap");
              //ESP.restart();
              WiFi.softAPdisconnect (true);
              
              
              return;
            }
     
        }*/
        
      }
  
}







//
// CLOCK / TIMER FUNCTIONS WITH RTC MILLIS
// ----------------------------------------------------

// read the time from the header of a http request
String getTimeFromInternet(String host) {
  WiFiClient client;
  while (!!!client.connect(host.c_str(), 80)) {
    Serial.println("connection failed, retrying...");
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");
 
  while(!!!client.available()) {
     yield();
  }

  while(client.available()){
    if (client.read() == '\n') {    
      if (client.read() == 'D') {    
        if (client.read() == 'a') {    
          if (client.read() == 't') {    
            if (client.read() == 'e') {    
              if (client.read() == ':') {    
                client.read();
                String theDate = client.readStringUntil('\r');
                client.stop();
                return theDate;
              }
            }
          }
        }
      }
    }
  }
}

// print date time object
void printDateTime(DateTime now){
    
      Serial.print(now.year(), DEC);
      Serial.print('/');
      Serial.print(now.month(), DEC);
      Serial.print('/');
      Serial.print(now.day(), DEC);
      Serial.print(' ');
      Serial.print(now.hour(), DEC);
      Serial.print(':');
      Serial.print(now.minute(), DEC);
      Serial.print(':');
      Serial.print(now.second(), DEC);
      Serial.print(" giorno ");
      Serial.print(now.dayOfTheWeek(), DEC);
      Serial.println();
}

// IN Italy we have legal time.
// return 1 if legal time is ON
byte LegalTime(DateTime now) {
          byte cFlag = 0;
    const byte iDayW = now.dayOfTheWeek();
    const byte iDay  = now.day();
    const byte iMonth= now.month();
    const byte iHour = now.hour();
     
    if (iMonth == 10) {
      if (iDayW == 0) {
          if (((iDay + 7) > 31) && (iHour >= 3)) { cFlag = 0; }
      } else {
        if ((iDay + (7 - iDayW))> 31) { cFlag = 0; }
        else { cFlag = 1; }
      }
    }
     
    if (iMonth == 3) {
      if (iDayW == 0) {
        if (((iDay + 7) > 31) && (iHour >= 2)) { cFlag = 1; }
      } else {
        if((iDay + (7 - iDayW))> 31) { cFlag = 1; } else { cFlag = 0; }
      }
    }
     
    if(iMonth >= 4 && iMonth <= 9) { cFlag = 1; }
    if((iMonth >= 1 && iMonth <= 2) || (iMonth >= 11 && iMonth <= 12)) { cFlag = 0; }
     
    return cFlag;
}

// set the date and time from web using getTimeFromInternet and LegalTime
void setDateTimeFromWeb(){
  String s = getTimeFromInternet("www.google.it");
  

    char* pch;
    char *dup = strdup(s.c_str());
    pch = strtok(dup," :");
    int q=0;
    String ladata = "";String laora = "";
    
    while (pch != NULL)
    {
      String s1= (String)pch;
      if(q==1) { ladata=s1; }
      if(q==2) { ladata=s1 + " " + ladata; }
      if(q==3) { ladata=ladata + " " + s1; }
      if(q==4) { laora=s1; }
      if(q==5 || q==6) { laora= laora + ":" + s1; }
      pch = strtok(NULL, " :");
      q++;
    }

    String ladat = ladata.c_str();
    String laor = laora.c_str();
    Serial.println(ladat);
    Serial.println(laor);

    int GMT = 1;
    DateTime n = DateTime(ladata.c_str(),laora.c_str());
    //n = n + (1 se ora legale)  *3600;
     byte cFlag = LegalTime(n);
      n = n + ( GMT + cFlag )   *3600;

    rtc.adjust(n);
   
  

  
}





//
// MP3 FUNCTIONS
// ----------------------------------------------------

// class to notify news from mp3 serial communication
class Mp3Notify {
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action)
  {
    mp3_error_code = 0;
    
    if (source & DfMp3_PlaySources_Sd) 
    {
        Serial.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb) 
    {
        Serial.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash) 
    {
        Serial.print("Flash, ");
    }
    //Serial.println(action);
  }
  static void OnError(uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
    mp3_error_code = errorCode;
  }
  static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track)
  {
    Serial.print("Play finished for #");
    Serial.println(track);  
    playing = 0;
    mp3_error_code = 0;
  }
  static void OnPlaySourceOnline(DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
    mp3_error_code = 0;
  }
  static void OnPlaySourceInserted(DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");
    mp3_error_code = 0;
  }
  static void OnPlaySourceRemoved(DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
    mp3_error_code = 0;
  }
};




// instance a DFMiniMp3 object, talking to wemos on two pins serial communication.
SoftwareSerial secondarySerial(PIN_RX_MP3, PIN_TX_MP3); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> dfmp3(secondarySerial);


// Play a track from a numbered folder
void playTrackFolderNum(uint8_t folder,uint8_t track,bool waitEnd = false) {
  if(phoneStatus!=4) {
    playing = 1;
    
    dfmp3.playFolderTrack(folder,track);
    if(waitEnd) {
      while(playing==1) {
        dfmp3.loop();
        checkHangStatus();
      }
      delay(200);
    }
  }  
}


// Play a track from the mp3 folder
void playTrackNum(uint8_t track,bool waitEnd=false) {
  if(phoneStatus!=4) {
     playing = 1;
     dfmp3.playMp3FolderTrack(track);
     if(waitEnd) {
      Serial.println("wait");
      while(playing==1) {
        dfmp3.loop();
        checkHangStatus();
      }
      delay(200);
    }
  }
}





// ROTARY DIALING ENCODER
void readNumber() {
  phoneNumber = "";
  
  int needToPrint = 0;
  int count=0;
  int lastState = LOW;
  int trueState = LOW;
  long lastStateChangeTime = 0;
  int cleared = 0;
  
  // constants
  int dialHasFinishedRotatingAfterMs = 100;
  int debounceDelay = 10;

  int digits = 0;

  unsigned long nodialingtime = 4000 ;
  unsigned long lasttimernothing = millis() + nodialingtime;
 
 
  while( digits < 10 && millis() < lasttimernothing) {
    dfmp3.loop();
    checkHangStatus();
    //Serial.println("loop2");
    int reading = digitalRead(PIN_ROTARY);
    if(reading==1) {
      if(playing==1) { dfmp3.stop(); playing=0; /* stop playing sound */ }
    }
  
    if ((millis() - lastStateChangeTime) > dialHasFinishedRotatingAfterMs) {
      // the dial isn't being dialed, or has just finished being dialed.
      if (needToPrint) {
        // if it's only just finished being dialed, we need to send the number down the serial
        // line and reset the count. We mod the count by 10 because '0' will send 10 pulses.
        Serial.print("(");Serial.print(count % 10, DEC);Serial.println(")");
        digits++;
        phoneNumber += (String)(count % 10);
        needToPrint = 0;
        count = 0;
        cleared = 0;
        lasttimernothing = millis() + nodialingtime;
       }
    }
  
    if (reading != lastState) {
      lastStateChangeTime = millis();
    }
    if ((millis() - lastStateChangeTime) > debounceDelay) {
        // debounce - this happens once it's stablized
        if (reading != trueState) {
         // this means that the switch has either just gone from closed->open or vice versa.
         trueState = reading;
         if (trueState == HIGH) {
          // increment the count of pulses if it's gone high.
          count++;
          needToPrint = 1; // we'll need to print this number (once the dial has finished rotating)
          
        }
      }
    }
    lastState = reading;
    
  }


}













void setup() {
  Serial.begin(115200);
  Serial.println("------------------");
  Serial.println("Starting...");
  
  pinMode(PIN_ROTARY, INPUT);
  pinMode(PIN_HANGUP_SWITCH, INPUT_PULLUP);
  pinMode(PIN_BELL_1,OUTPUT);
  pinMode(PIN_BELL_2,OUTPUT);

  // WEMOS
  // -----------------------------------------------
  // turn off led Wemos power
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // 
  // Start filing subsystem
  if (SPIFFS.begin()) {
      Serial.println("SPIFFS Active");
      spiffsActive = true;
  } else {
      Serial.println("Unable to activate SPIFFS");
  }

  // Read data from EEPROM
  readUserData();

  // Try fow Wi-fi connection
  connectToWifi();

  // Set time and date
  if(wifi) {
    
    setDateTimeFromWeb();
    timer_update_rtc_millis = millis() + 24 * 3600 * 1000;
    
    //rtc.adjust(DateTime(2022, 5, 4, 14, 8, 1));
    DateTime now = rtc.now();
    printDateTime(now);
  } else {
    rtc.adjust(DateTime(2022, 1, 1, 1, 1, 1));
  }  

  // randomize after wifi connection so time changes
  randomSeed(millis());


  // DFMINI MP3
  // -----------------------------------------------
  dfmp3.begin();
  dfmp3.reset();
  dfmp3.setVolume(VOLUME);

  uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  Serial.println("files found " + (String)count);


  

  //
  // -----------------------------------------------
  // WEMOS UPDATE OVER THE AIR
  ArduinoOTA.setHostname(projectname);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  // -----------------------------------------------





  
}





// PHONE FUNCTIONS
// ------------------------------------------------

// Tell the time
void tellTheTime() {
      DateTime now = rtc.now();
      int h = now.hour();
      int m = now.minute();
      int s = now.second();
  
      printDateTime(now);

      // This sequence of mp3 follows the Italian language syntax.
      // It should be more abstract to use other syntaxes (TO DO)

      playTrackFolderNum(1,67,WAIT_END); // sono le ore
      playTrackFolderNum(1,h,WAIT_END);  // h
      playTrackFolderNum(1,62,WAIT_END); // ...e ...
      playTrackFolderNum(1,m,WAIT_END);  // m
 
}


// Function that do nothing, just silence, but keep listening for handset hang up
void makeSilenceFor(int sec) {
  unsigned long p = millis() + sec * 1000;
  while(millis()< p && phoneStatus!=4 && phoneStatus!=5) {
      dfmp3.loop();
      checkHangStatus();
  }  
}


int translateMeteoCode(byte i){
  int clip = -1;
  if(i==0) { clip = 0; }// Clear sky
  if(i==1) { clip = 1; }// Mainly clear
  if(i==2) { clip = 2; }// partly cloudy
  if(i==3) { clip = 3; }// overcast
  if(i==45) { clip = 4; }// for
  if(i==48) { clip = 5; }// rime fog
  if(i==51) { clip = 6; }// Drizzle: Light
  if(i==53) { clip = 7; }// Drizzle: moderate
  if(i==55) { clip = 8; }// Drizzle dense intensity
  if(i==56) { clip = 9; }// freezing drizzle light
  if(i==57) { clip = 10; }// freezing drizzle dense
  if(i==61) { clip = 11; }//  Rain: Slight
  if(i==63) { clip = 12; }// Rain: moderate
  if(i==65) { clip = 13; }// rain heavy intensity
  if(i==71) { clip = 14; }// Snow fall: Slight, 
  if(i==73) { clip = 15; }// snow fall moderate
  if(i==75) { clip = 16; }// snbow fall heavy intensity

  if(i==77) { clip = 17; }// Snow grains
  if(i==80) { clip = 18; }// Rain showers: Slight
  if(i==81) { clip = 19; }// Rain showers: moderate
  if(i==82) { clip = 20; }// Rain showers: violent
  if(i==85) { clip = 21; }// Snow showers slight
  if(i==86) { clip = 22; }// Snow showers heavy
  if(i==95) { clip = 23; }// Thunderstorm: Slight or moderate
  if(i==96) { clip = 24; }// Thunderstorm with slight hail
  if(i==99) { clip = 25; }// Thunderstorm with heavy hail
  
  return clip;

}



void tellMeMeteo(String pn) {

  // sigla ?
  //playTrackFolderNum(2,26);  // play a jingle and do not wait end, so we can make api calls in background 
  
  WiFiClient client;
  
  if (!client.connect("api.open-meteo.com", 80)) {
    Serial.println(F("Connection failed"));
    return;
  }

  // Send HTTP request
  client.println(F("GET /v1/forecast?latitude=45.55199&longitude=9.18562&daily=weathercode,temperature_2m_max,temperature_2m_min&timezone=Europe%2FRome HTTP/1.0"));
  client.println(F("Host: api.open-meteo.com"));
  client.println(F("Connection: close"));
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    client.stop();
    return;
  }

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
  if (strcmp(status + 9, "200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    client.stop();
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    client.stop();
    return;
  }


  // Extract weather codes
  String temp = "";
  while (client.available() && temp=="") {
    String line = client.readStringUntil('\n');
    temp = midString(line,"weathercode\":[","]");
    Serial.println(line);
    Serial.println(temp);
  }  
  if(temp!="") {

      // check if jingle finished
      // ----------------------------
      // while (playing==1) {
      //   mp3.loop();
      //   checkHangStatus();
      // }
      // ----------------------------

      char * pch;
      char* x = (char*)temp.c_str(); // a pointer to the first element of the string converted to cstring
      pch = strtok (x,",");

      byte i=0;
    
      int w = 0;
      while (pch) {
        Serial.print(i); Serial.print("= ");
        w = atoi(pch);

        if (i==0) {
          playTrackFolderNum(2,26,WAIT_END);  // today
        }
        if (i==1) {
          playTrackFolderNum(2,27,WAIT_END);  // tomorrow
        }

        if (i>1) {
          // tell the name of the day
          DateTime now = rtc.now();
           int g = (now.dayOfTheWeek() + i ) % 7;
          int clips[7] = {35,29,30,31,32,33,34};
          playTrackFolderNum(2,clips[g],WAIT_END);  // monday...
        }

        int a = translateMeteoCode(w);
        playTrackFolderNum(2,a,WAIT_END);  // clear sky...
        
        Serial.println(w);
        pch = strtok(NULL, ",");
        i++;
      }
  }
 
  // Disconnect
  client.stop();
}

String midString(String str, String start, String finish){
  int locStart = str.indexOf(start);
  if (locStart==-1) return "";
  locStart += start.length();
  int locFinish = str.indexOf(finish, locStart);
  if (locFinish==-1) return "";
  return str.substring(locStart, locFinish);
}



/*
void tellMeMeteo( String pn) {

  String host = "https://api.open-meteo.com/v1/forecast?latitude=45.1&longitude=9.3&daily=weathercode&timezone=Europe%2FRome";

  WiFiClient client;
  while (!!!client.connect(host.c_str(), 80)) {
    Serial.println("connection failed, retrying...");
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");

  String json = "";
  while(!!!client.available()) {
     yield();
  }

  while(client.available()){
    json = json + client.read();
  }

  Serial.println(json);

  // TO DO
  // json parsing

  // TO DO
  // and ouput audio

}

*/


//
// Ring the bells!
// Make a alternate square wave on bells coil with 2 pin and L293D driver
void bells() {
  int maxRings = 5;
  setPhoneStatus(5);
  while(phoneStatus==5 && maxRings>=1) {
    int i = 0;
    Serial.println("Ring");
    while(phoneStatus==5 && i<30) {
      digitalWrite(PIN_BELL_2,LOW);
      digitalWrite(PIN_BELL_1,HIGH);
      delay(20);
      digitalWrite(PIN_BELL_2,HIGH);
      digitalWrite(PIN_BELL_1,LOW);
      delay(20);
      checkHangStatus();
      i++;
    }
    digitalWrite(PIN_BELL_2,LOW);
    unsigned long ti = millis() + 2000;
    while(phoneStatus==5 && millis()<ti){
      delay(1);
      checkHangStatus();
    }
    maxRings--;
  }
  if (maxRings==0) {
    setPhoneStatus(4);
    phoneNumber = "";
    timer_1 = 0;    
  }
}




// answer arrives after 1 or 2 sounds
void waitingForAnswer() { for(int i=0;i<random(1,3);i++) { playTrackNum(7, WAIT_END); }}

//
// print phone status, avoid multiple same status prints
//byte oldStatus = 255;
void setPhoneStatus(byte newStatus) {
  if (phoneStatus!=newStatus) {
    //oldStatus = phoneStatus;
    phoneStatus = newStatus;
    Serial.println("PHONE STATUS IS " + (String)phoneStatus);
  }
}




// check if the handset is hanged up or picked up
// this action changes the status of the vintagephone
void checkHangStatus(){
  byte cornettaStatus;
  cornettaStatus = digitalRead(PIN_HANGUP_SWITCH);  // 0 = OPEN, PICK UP - 1 = CLOSE, HUNG UP

  if (phoneStatus==5 && cornettaStatus==0) {
    // pick up during ringing
    setPhoneStatus(3);
    phoneNumber = caller_1;
    timer_1 = 0;
    delay(200);
    // stop bells and answer
    
  } else if(phoneStatus==5 && cornettaStatus==1) {
    // nothing, still ringing

  } else if (phoneStatus!=4 && cornettaStatus==1) {
    // user hang up while dialing or answering or not valid number
    setPhoneStatus(4);
    dfmp3.stop();
    playing=0;
    delay(200);
  } else if (phoneStatus==4 && cornettaStatus==0) {
    // user pick up, from off to dialing
    setPhoneStatus(0);
    delay(200);
  }
  
  if (cornettaStatus==1 && playing==1) {
    dfmp3.stop();
    playing=0;
  }
  
}


// tell the time passed after alarm
void tellTheTimePassed(String numberDialed) {
    caller_1 = numberDialed;
    unsigned long minutesD = strtoul(numberDialed.c_str(), NULL, 10); 
    minutesD =  minutesD - pow(10,numberDialed.length() - 1);

    // this uses Italian language syntax.
    // probably and abstraction level is needed to handle different languages (TO DO)
    playTrackFolderNum(1,68,WAIT_END); // sono passati
    if(minutesD >= 60) {
      // if more the 60 minutes say hours and minutes
      int m2 = minutesD % 60;
      minutesD -= m2;
      int h = minutesD / 60;
      playTrackFolderNum(1,h,WAIT_END); 
      playTrackFolderNum(1,61,WAIT_END); // ...ore ...
      playTrackFolderNum(1,62,WAIT_END); // ...e ...
      minutesD=m2;
    }
    playTrackFolderNum(1,minutesD,WAIT_END); 
    playTrackFolderNum(1,60,WAIT_END); // minuti
}



//
// set the timer for alarm and tell the alarm just setted
void setTheAlarm(String numberDialed) {
    caller_1 = numberDialed;
    if(numberDialed.length()>1 && phoneNumber.length()<=4) {
      // dialed 1XXX, alarm in XXX minutes
      unsigned long minutesD = strtoul(numberDialed.c_str(), NULL, 10); 
      minutesD =  minutesD - pow(10,numberDialed.length() - 1);
      timer_1 = millis() + minutesD * 60 * 1000;

      // this sequence of play mp3 follows italian languyage syntax
      playTrackFolderNum(1,63,WAIT_END); // sveglia impostata tra
      if(minutesD >= 60) {
        int m2 = minutesD % 60;
        minutesD -= m2;
        int h = minutesD / 60;
        playTrackFolderNum(1,h,WAIT_END); 
        playTrackFolderNum(1,62,WAIT_END); // ...e ...
        minutesD=m2;
      }
      playTrackFolderNum(1,minutesD,WAIT_END); 
      playTrackFolderNum(1,60,WAIT_END); // minuti
    }
    if(numberDialed.length()==5) {
      unsigned long minutesD = strtoul(numberDialed.c_str(), NULL, 10); 
      minutesD =  minutesD - pow(10,numberDialed.length() - 1);
      
      if (minutesD<=2359) {
        String temp = "00";
        temp[0] = numberDialed.charAt(1);
        temp[1] = numberDialed.charAt(2);
        int ht = temp.toInt();
        temp = "00";
        temp[0] = numberDialed.charAt(3);
        temp[1] = numberDialed.charAt(4);
        int mt = temp.toInt();
        Serial.print("ht = "); Serial.println(ht);
        Serial.print("mt = "); Serial.println(mt);

        DateTime a = rtc.now();
        
        DateTime b = DateTime(a.year(), a.month(), a.day(), ht, mt, 0);
        printDateTime(b);
        printDateTime(a);
        

        if(b<a) b = b + 3600*24;
        printDateTime(b);
        
        unsigned long diff = b.unixtime() - a.unixtime();
        Serial.println(diff);

        timer_1 = millis() + diff * 1000;      

        // sveglia impostata alle ore
        playTrackFolderNum(1,69,WAIT_END); // sveglia impostata alle
        playTrackFolderNum(1,ht,WAIT_END); 
        playTrackFolderNum(1,62,WAIT_END); // ...e ...
        playTrackFolderNum(1,mt,WAIT_END); 
    
      } else {
        playTrackFolderNum(1,66,WAIT_END); // ore o minuti non validi
      }
    }
}






void loop()
{
  ArduinoOTA.handle();

  checkHangStatus();

  dfmp3.loop();



  // FIX RTC MILLIS
  if(millis() > timer_update_rtc_millis) {
     if(wifi) {   
        setDateTimeFromWeb(); 
        // every 24h check for times from web
        timer_update_rtc_millis = millis() + 24 * 3600 * 1000;
     }
  }




  if(mp3_error_code == 3) {
    // try to solve Com Error 3
    Serial.println(".....");
    mp3_error_code = 0;
    dfmp3.begin();dfmp3.reset();dfmp3.setVolume(VOLUME);
    delay(200);
  }


  // 0=RECEIVER PICKED UP (DIALING)
  // WAITING FOR DIALING
  if(playing==0 && phoneStatus==0) {
      playTrackNum( random(8,10));
  }


  // 4=HANGEDUP
  // ALARM TIME CHECK
  if ( timer_1 >0 &&  millis()> timer_1 && phoneStatus==4) {
    setPhoneStatus(5); //ring
    bells();
  }

  
  // 3=ANSWERING
  // answer to the alarm
  if (phoneStatus==3 && phoneNumber!="") {
    if(phoneNumber.charAt(0)=='1') {
      // 
      if(phoneNumber.length()<5) {
        makeSilenceFor(1);
        tellTheTimePassed(phoneNumber);
      }
      if(phoneNumber.length()==5) {
        makeSilenceFor(1);
        playTrackFolderNum(1,65,WAIT_END); 
        tellTheTime();
      }
    }
    setPhoneStatus(2);
    playTrackNum(1);
  }


 //
 // 0=DIALING
 if (phoneStatus==0) {
   readNumber() ;
   
   if(phoneNumber!="") {
    Serial.println("> Call " + phoneNumber);

    bool found = false;
    
    setPhoneStatus(1); // calling
    waitingForAnswer();



    // #1   OR  #1[d]{1,3}  OR    #1-HH-MM
    // ---------------------------------------------------------------
    if(phoneNumber.charAt(0)=='1' && phoneNumber.length()<=5) {
      // NUMBER BEGINS WITH 1 TO SET TIME
      //
      setPhoneStatus(3);
      found = true;
      if(phoneNumber.length()==1) {
          // TELL THE TIME
          tellTheTime();
      }
      if(phoneNumber.length()>1 && phoneNumber.length()<=5) {
          // SET ALARM WITH MINUTES OR WITH HOURS AND MINUTES
          setTheAlarm(phoneNumber);
          
      }
      setPhoneStatus(2);
      playTrackNum(1);
    }


    
    // #2 ALARM DELETED
    // ---------------------------------------------------------------
    if(phoneNumber=="2") {
      found = true;
      timer_1 = 0;
      caller_1="";
      setPhoneStatus(3);
      playTrackFolderNum(1,64,WAIT_END); // Sveglia cancellata
      setPhoneStatus(2);
      playTrackNum(1);
      
    }


    // #21 RESTART WEMOS
    // ---------------------------------------------------------------
    if(phoneNumber=="21") {
      found = true;
      setPhoneStatus(3);
      playTrackNum(17,WAIT_END); // Riavvio in corso
      setPhoneStatus(2);
      ESP.restart();
      
    }

    // #22 GET TIME FROM INTERNET AGAIN
    // ---------------------------------------------------------------
    if(phoneNumber=="22") {
      found = true;
      setPhoneStatus(3);
      if(wifi) {
        playTrackFolderNum(1,70,WAIT_END); // "Setting date and time from internet..."
        setDateTimeFromWeb();
      } else {
        playTrackNum(18); // "Internet not available"
      }
      setPhoneStatus(2);
      playTrackNum(1);
      
    }


    // #23 ACTIVATE AP
    // ---------------------------------------------------------------
    if(!found && phoneNumber=="23") {
      found = true;
      setPhoneStatus(3);
      playTrackNum(19,WAIT_END); // AP attivo cerca Vintagephone...
      setPhoneStatus(2);
      playTrackNum(1,WAIT_END);
      Serial.println("AP");
      setupPortal();
      connectToWifi();
    }


    // #24 HEADS OR TAILS
    // ---------------------------------------------------------------
    if(!found && phoneNumber=="24") {
      found = true;
      setPhoneStatus(3);
      playTrackNum(20,WAIT_END); // Lancio una monetina...
      int r = random(0,2);
      playTrackNum(15 + r,WAIT_END);
      Serial.println(r==0 ? "testa" : "croce");
      setPhoneStatus(2);
      playTrackNum(1);
    }

    // #25 YES OR NO
    // ---------------------------------------------------------------
    if(!found && phoneNumber=="25") {
      found = true;
      setPhoneStatus(3);
      int r = random(0,2);
      playTrackNum(13 + r,WAIT_END);
      Serial.println(r==0 ? "yes" : "no");
      setPhoneStatus(2);
      playTrackNum(1);
    }





    // #4 METEO
    // ---------------------------------------------------------------
    if(phoneNumber=="4") {
      found = true;
      setPhoneStatus(3);
      if(wifi) {
        tellMeMeteo( phoneNumber );
      } else {
        playTrackNum(18); // Internet not available
      }
      setPhoneStatus(2);
      playTrackNum(1);
    }
    
    if(!found) {
      // NUMBER NOT FOUND
      //
      setPhoneStatus(3);
      playTrackNum(random(10,13),WAIT_END); // modem o non attivo
      setPhoneStatus(2);
      playTrackNum(1);
    }



    
   }
 }
}
