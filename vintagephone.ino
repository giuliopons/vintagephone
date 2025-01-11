// VINTAGE ROTARY PHONE RETROFIT
// --------------------------------------------------
// clip audio can be recorded here: https://murf.ai/






// MP3 PLAYER
// ----------------------------------------------------
#include <SoftwareSerial.h>
#include <DFMiniMp3.h>     /* 1.1.1 */
class Mp3Notify;
#define WAIT_END true
#define VOLUME 24
#define PIN_RX_MP3 D6
#define PIN_TX_MP3 D5
int playing = 0;
int mp3_error_code;
byte DELAY_RING = 30; // millis between left and right bell
byte RINGTONES_BEFORE_ANSWER = 1;
byte DEFAULT_RINGBELLS_REPEAT = 5;
byte SWITCH_REVERSE = 0;

// instance a DFMiniMp3 object, talking to wemos on two pins serial communication.
SoftwareSerial secondarySerial(PIN_RX_MP3, PIN_TX_MP3); // RX, TX
//DFMiniMp3<SoftwareSerial, Mp3Notify> dfmp3(secondarySerial);
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3; 

DfMp3 dfmp3(secondarySerial);


// PHONE STATUS
// -----------------------------------------------
String phoneNumber = ""; // dialed phone numbers

#define DIALING 0       // 0 dialing (or waiting for dialiang, handset up)
#define CALLING 1       // 1 calling, handset up
#define CALL_ENDED 2    // 2 the call is ended or can't establish a call (wrong number)
#define ANSWERING 3     // 3 answering
#define HANDSET_DOWN 4  // 4 the user put the handset down
#define RINGING 5       // 5 the phone is ringing

byte phoneStatus = HANDSET_DOWN;


// TIMER
// --------------------------------------------------
#include "RTClib.h"
RTC_Millis rtc;
unsigned long timer_1 = 0;
unsigned long timer_update_rtc_millis = 0;
String caller_1="";




// WEMOS LIBRARIES
// ---------------------------------------------------

char projectname[50];

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#define PIN_HANGUP_SWITCH D1      // switch hang up
#define PIN_ROTARY D8    // rotary encoder pin
#define PIN_BELL_1 D2
#define PIN_BELL_2 D3
bool          wifi = false;                 // global variable for wifi status
bool          ap_active = false;            // global variable for access point mode status






// SPIFFS AND EEPROM LIBRARIES
// ----------------------------------------------------
#include <EEPROM.h>
#include <string.h>
#include "FS.h"
bool    spiffsActive = false;

// Structure for data saved in EEPROM
struct settings {
  char ssid[30];
  char password[30];
  char lat[9];
  char lon[9];
  char utc[30];
} userdata = {};




//
// WEMOS FUNCTION, WIFI
// AP, PORTAL, EPROM...
// ----------------------------------------------------

DNSServer dnsServer;
ESP8266WebServer webServer(80);




// SPIFFS AND EEPROM FUNCTIONS
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
  byte onOffLed = 0;
  while (WiFi.status() != WL_CONNECTED) {
    onOffLed++;
    Serial.print(".");
    if(onOffLed % 2==1) digitalWrite(D4, LOW); else digitalWrite(D4, HIGH);
    delay(1000);
    if (tries++ > sec) {
      // fail to connect
      digitalWrite(D4, HIGH);
      Serial.println("");
      return false;
    }
  }
  Serial.println("");
  digitalWrite(D4, HIGH);
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
    Serial.println(F("style"));
    String o = readFile("/style.css");
    webServer.send(200,   "text/css", o );
}

// AP PORTAL: handle stop AP url
void handleStopAP(){
    Serial.println(F("stop AP"));
    String o = readFile("/generic.html");
    o.replace("#title#", "Stop");
    o.replace("#msg#","Turning AP off...");
    webServer.send(200,   "text/css", o );
    ap_active = false;
    WiFi.softAPdisconnect(true);
    connectToWifi();
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

    strncpy(userdata.lat, webServer.arg("lat").c_str(),  sizeof(userdata.lat) );
    strncpy(userdata.lon, webServer.arg("lon").c_str(),  sizeof(userdata.lon) );
    strncpy(userdata.utc, webServer.arg("utc").c_str(),  sizeof(userdata.utc) );
    userdata.lat[webServer.arg("lat").length()] = userdata.lon[webServer.arg("lon").length()] = userdata.utc[webServer.arg("utc").length()] = '\0';
    EEPROM.put(0, userdata);
    EEPROM.commit();
    Serial.println("saved");
    String o = readFile("/savedsettings.html");
    String css = readFile("/style.css"); 
    o.replace("<link rel=\"stylesheet\" href=\"style.css\">","<style>" + css + "</style>");
    webServer.send(200,   "text/html",  o );
      
  } else {

    Serial.println("form");
    String o = readFile("/settings.html");
    o.replace("#lat#", userdata.lat);
    o.replace("#lon#", userdata.lon);
    o.replace("#utc#", userdata.utc);
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
      webServer.on("/",  handleHomeMenu);           // HOME
      webServer.on("/style.css",  handleCSS);       // CSS

      webServer.on("/wifisetup",  handleFormWifiSetup);   // WIFI SETTINGS
      webServer.on("/settings",  handleFormSettings);     // PHONE SETTINGS
      webServer.on("/stopap",  handleStopAP);             // STOP AP
      webServer.begin();

      ap_active = true;

      // never ending loop waiting access point configuration     
      while(ap_active){
        delay(50);
        dnsServer.processNextRequest();
        webServer.handleClient();        
      }
  
}







//
// CLOCK / TIMER FUNCTIONS WITH RTC MILLIS
// ----------------------------------------------------

// read the time from the header of a http request
String getTimeFromInternet(String host) {
  WiFiClient client;
  while (!client.connect(host.c_str(), 80)) {
    Serial.println(F("Connection failed"));
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
  return "";
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
  //static void OnError(uint16_t errorCode)
  static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode)
  {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
    mp3_error_code = errorCode;
  }
  //static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track)
  static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track)
  {
    Serial.print("Play finished for #");
    Serial.println(track);  
    playing = 0;
    mp3_error_code = 0;
  }
  static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  //static void OnPlaySourceOnline(DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
    mp3_error_code = 0;
  }

  //static void OnPlaySourceInserted(DfMp3_PlaySources source)
  static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)

  {
    PrintlnSourceAction(source, "inserted");
    mp3_error_code = 0;
  }
  //  static void OnPlaySourceRemoved(DfMp3_PlaySources source)
  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
    mp3_error_code = 0;
  }
};





// Play a track from a numbered folder
void playTrackFolderNum(uint8_t folder,uint8_t track,bool waitEnd = false) {
  if(phoneStatus != HANDSET_DOWN) {
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
  if(phoneStatus != HANDSET_DOWN) {
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
void readNumberDialed() {
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

// Adjust time with the dial
void setTheTime(String numberDialed) {
    String temp = "00";
    temp[0] = numberDialed.charAt(2);
    temp[1] = numberDialed.charAt(3);
    int ihh = temp.toInt();
    temp = "00";
    temp[0] = numberDialed.charAt(4);
    temp[1] = numberDialed.charAt(5);
    int imm = temp.toInt();
  
  if(ihh<24 && imm<60) {
    DateTime a = rtc.now();
    rtc.adjust(DateTime(a.year(), a.month(), a.day(), ihh, imm, 0));
    
    tellTheTime();
  } else {
    playTrackFolderNum(1,66,WAIT_END); // ore o minuti non validi
  }
}

// Adjust date with the dial
void setTheDate(String numberDialed) {
    String temp = "0000";
    temp[0] = numberDialed.charAt(2);
    temp[1] = numberDialed.charAt(3);
    temp[2] = numberDialed.charAt(4);
    temp[3] = numberDialed.charAt(5);
    int yyyy = temp.toInt();
    temp = "00";
    temp[0] = numberDialed.charAt(6);
    temp[1] = numberDialed.charAt(7);
    int mm = temp.toInt();
    temp = "00";
    temp[0] = numberDialed.charAt(8);
    temp[1] = numberDialed.charAt(9);
    int dd = temp.toInt();
  
  if(yyyy>1970 && dd>0 && dd<32 && mm<13  && mm>0) {
    DateTime a = rtc.now();
    rtc.adjust(DateTime(yyyy, mm, dd, a.hour(), a.minute(), 0));
    
    //playTrackFolderNum(1,64,WAIT_END); // data impostata a    (TO DO)
    //playTrackFolderNum(1,yyyy,WAIT_END);  // yyyy    (TODO)
    //playTrackFolderNum(1,mm,WAIT_END); // mm month names
    //playTrackFolderNum(1,dd,WAIT_END);  // dd
  } else {
    //playTrackFolderNum(1,66,WAIT_END); // wrong date    (TO DO)
  }
}


// Function that do nothing, just silence, but keep listening for handset hang up
void makeSilenceFor(int sec) {
  unsigned long p = millis() + sec * 1000;
  while(millis()< p && phoneStatus!= HANDSET_DOWN  && phoneStatus!= RINGING ) {
      dfmp3.loop();
      checkHangStatus();
  }  
}





// convert meteo code to track number
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


// NOT USED
void logTelegram(String s, String chat_id, String apiToken) {
  WiFiClient client;
  if (!client.connect("api.telegram.org", 80)) {
    Serial.println(F("Connection failed"));
    return;
  }
  https://api.telegram.org/bot".$apiToken."/sendmessage?chat_id=".$chatId."&text=".rawurlencode($testo)."&parse_mode=HTML
  String o = F("GET /bot#apitoken#/sendmessage?chat_id=#chatid#&text=#s#&parse_mode=HTML HTTP/1.0");
  o.replace("#chatid#", chat_id);
  o.replace("#s#", s);
  o.replace("#apitoken#", apiToken);
  client.println(o);
  client.println(F("Host: api.telegram.org"));
  client.println(F("Connection: close"));
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    client.stop();
    return;
  }
}


void tellMeMeteo(String pn) {

  // short jingle 
  playTrackFolderNum(3,45);  // play a jingle and do not wait end, so we can make api calls in background 
  
  WiFiClient client;

  while (!client.connect("api.open-meteo.com", 80)) {
    Serial.println(F("Connection failed"));
  }

  // Send HTTP request
  String o = F("GET /v1/forecast?latitude=#lat#&longitude=#lon#&daily=weathercode,temperature_2m_max,temperature_2m_min&timezone=#utc# HTTP/1.0");
  o.replace("#lat#", userdata.lat);
  o.replace("#lon#", userdata.lon);
  o.replace("#utc#", userdata.utc);
  Serial.println(o);
  client.println(o);
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
  String tempMax="",tempMin="",temp = "";
  while (client.available() && temp=="") {
    String line = client.readStringUntil('\n');

    // max and min temperatures
    tempMax = midString(line,"temperature_2m_max\":[","]");
    tempMin = midString(line,"temperature_2m_min\":[","]");
      
    temp = midString(line,"weathercode\":[","]");
    Serial.println(line);
    Serial.println(temp);
    Serial.println(tempMax);
    Serial.println(tempMin);
  }  
  if(temp!="") {


      // check if jingle finished
      // ----------------------------
       while (playing==1) {
         dfmp3.loop();
         checkHangStatus();
      }
      // ----------------------------

      char * pch;
      char* x = (char*)temp.c_str(); // a pointer to the first element of the string converted to cstring
      pch = strtok (x,",");

      byte i=0;
    
      int w = 0;
      while (pch) {
        Serial.print(i); Serial.print("=>> ");
        w = atoi(pch);
        Serial.print(pch);
        Serial.print("----");
        Serial.println(w);

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

        
        if(i==0) {
          playTrackFolderNum(2,36,WAIT_END);  // max temp is...
          int b = round( atof(tempMax.c_str()) );
          playTrackFolderNum(1,b,WAIT_END);  // max temp value...
          playTrackFolderNum(2,38,WAIT_END);  // celsius degree...
        
          playTrackFolderNum(2,37,WAIT_END);  // min temp...
          int c = round( atof(tempMin.c_str()) );
          playTrackFolderNum(1,c,WAIT_END);  // min temp value...
          playTrackFolderNum(2,38,WAIT_END);  // celsius degree...
        }
        
        
        playTrackFolderNum(3,i<6 ? 12 : 7,WAIT_END); // sound
        
        
        pch = strtok(NULL, ",");
        i++;
      }
  }
 
  // Disconnect
  client.stop();
}

// extract string between two tags
String midString(String str, String startTag, String finishTag){
  int locStart = str.indexOf(startTag);
  if (locStart==-1) return "";
  locStart += startTag.length();
  int locFinish = str.indexOf(finishTag, locStart);
  if (locFinish==-1) return "";
  return str.substring(locStart, locFinish);
}



//
// Ring the bells!
// Make a alternate square wave on bells coil with 2 pin and L293D driver
void bells( int maxRings ) {
  setPhoneStatus( RINGING );
  while(phoneStatus==RINGING && maxRings>=1) {
    int i = 0;
    int d = DELAY_RING;
    Serial.print("Ring");
    while(phoneStatus==RINGING && i<30) {
      digitalWrite(PIN_BELL_2,LOW);
      digitalWrite(PIN_BELL_1,HIGH);
      delay(d);
      digitalWrite(PIN_BELL_2,HIGH);
      digitalWrite(PIN_BELL_1,LOW);
      delay(d);
      checkHangStatus();
      i++;
    }
    
    
    
    digitalWrite(PIN_BELL_2,LOW);
    unsigned long ti = millis() + 2000;
    while(phoneStatus== RINGING && millis()<ti){
      delay(1);
      checkHangStatus();
    }
    maxRings--;
  }
  if (maxRings==0) {
    setPhoneStatus( HANDSET_DOWN );
    phoneNumber = "";
    timer_1 = 0;    
  }
}




// answer arrives after 1 or 2 sounds
void waitingForAnswer() { for(int i=0;i<RINGTONES_BEFORE_ANSWER;i++) { playTrackNum(7, WAIT_END); }}

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
  byte switchStatus;
  switchStatus = digitalRead(PIN_HANGUP_SWITCH);  // 0 = OPEN, PICK UP - 1 = CLOSE, HUNG UP

  if(SWITCH_REVERSE==1) {
     switchStatus = !switchStatus;
  }

  if (phoneStatus== RINGING && switchStatus==0) {
    // pick up during ringing
    setPhoneStatus( ANSWERING );
    phoneNumber = caller_1;
    timer_1 = 0;
    delay(200);
    // stop bells and answer
    
  } else if(phoneStatus== RINGING && switchStatus==1) {
    // nothing, still ringing

  } else if (phoneStatus!= HANDSET_DOWN && switchStatus==1) {
    // user hang up while dialing or answering or not valid number
    setPhoneStatus(HANDSET_DOWN);
    dfmp3.stop();
    playing=0;
    delay(200);
  } else if (phoneStatus==HANDSET_DOWN && switchStatus==0) {
    // user pick up, from off to dialing
    setPhoneStatus(DIALING);
    delay(200);
  }
  
  if (switchStatus==1) {
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
    
    if(numberDialed=="9") {
      // rings after 5 seconds
      timer_1 = millis() + 5000;
      playTrackNum(1);
      
    }
    
    if(numberDialed.length()>1 && phoneNumber.length()<=4) {
      // dialed 1XXX, alarm in XXX minutes
      unsigned long minutesD = strtoul(numberDialed.c_str(), NULL, 10); 
      minutesD =  minutesD - pow(10,numberDialed.length() - 1);
      timer_1 = millis() + minutesD * 60 * 1000;

      // this sequence of play mp3 follows italian language syntax
      playTrackFolderNum(1,63,WAIT_END);      // alarm setted in
      if(minutesD >= 60) {
        int m2 = minutesD % 60;
        minutesD -= m2;
        int h = minutesD / 60;
        playTrackFolderNum(1,h,WAIT_END);     // (number)
        playTrackFolderNum(1,61,WAIT_END);    // hours
        playTrackFolderNum(1,62,WAIT_END);    // ...and ...
        minutesD=m2;
      }
      playTrackFolderNum(1,minutesD,WAIT_END); // (number) 
      playTrackFolderNum(1,60,WAIT_END);       // minutes
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
        //printDateTime(b);
        //printDateTime(a);

        if(b<a) b = b + 3600*24;
        //printDateTime(b);
        
        unsigned long diff = b.unixtime() - a.unixtime();
        //Serial.println(diff);

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




void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("-----------------------");
  Serial.println("Welcome to Vintagephone");
  Serial.println("-----------------------");
  uint32_t chipId = ESP.getChipId();
  sprintf(projectname, "vintagephone_%X", chipId);
  Serial.println(projectname);

//char chipIdStr[9]; // 8 characters + null terminator
//snprintf(chipIdStr, sizeof(chipIdStr), "%08X", chipId); 

//   if (strcmp(chipIdStr, "D40E34") == 0) {
//    SWITCH_REVERSE = 1;
//  }

  //
  // Configure pins that are connected to
  // the hardware of the old phone
  pinMode(PIN_ROTARY, INPUT);
  pinMode(PIN_HANGUP_SWITCH, INPUT_PULLUP);
  pinMode(PIN_BELL_1,OUTPUT);
  pinMode(PIN_BELL_2,OUTPUT);



  //
  // turn off the blue led of Wemos power
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);


  //
  // SPIFFS
  // Start the file subsystem, used to store HTML
  // CSS and other files for captive portal AP
  if (SPIFFS.begin()) {
      Serial.println("SPIFFS Active");
      spiffsActive = true;
  } else {
      Serial.println("Unable to activate SPIFFS");
  }


  //
  // Read data from EEPROM: if already configured
  // whe have username and password for wifi access
  readUserData();

  //
  // Try to use user an password to connect
  // to internet, result is stored in global wifi var
  connectToWifi();

  //
  // if wifi access is available, set time and date
  // from internet
  if(wifi) {
    setDateTimeFromWeb();
    timer_update_rtc_millis = millis() + 24 * 3600 * 1000;
    //rtc.adjust(DateTime(2022, 5, 4, 14, 8, 1));
    DateTime now = rtc.now();
    printDateTime(now);
  } else {
    rtc.adjust(DateTime(2022, 1, 1, 1, 1, 1));
  }  


  //
  // randomize after wifi connection so it should be
  // different everytime
  randomSeed(millis());


  //
  // Start communication with DFMINI MP3 player
  // set volume and count tracks
  dfmp3.begin();
  dfmp3.reset();
  dfmp3.setVolume(VOLUME);
  uint16_t count = dfmp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  Serial.println("files found " + (String)count);
  
  uint16_t mode = dfmp3.getPlaybackMode();

  

  Serial.println("Status is: " + (String)phoneStatus);


  //
  // Setup the WEMOS UPDATE OVER THE AIR process
  // this will allow to upload a new sketch to the Vintagephone
  // without plugging the USB cable to the Wemos.
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







void loop()
{
  //
  // Handle the Over The Air update process
  ArduinoOTA.handle();

  //
  // check the status of the hang switch
  // necessary to understand the phone status
  checkHangStatus();

   //setPhoneStatus( RINGING ); //ring
   //bells(999);



    
  //
  // this function of the DFMINI player
  // object handles the communication with
  // the hardware of the mp3
  dfmp3.loop();



  //
  // FIX RTC MILLIS
  // The clock of the ESP isn't precise, so we can
  // periodically call an internet site (google) to
  // read the correct date and time.
  if(millis() > timer_update_rtc_millis) {
     if(wifi) {   
        setDateTimeFromWeb(); 
        // every 24h check for times from web
        timer_update_rtc_millis = millis() + 24 * 3600 * 1000;
     }
  }


  // the dfmp3 library sometimes shows a Com Error
  // while communicating with the mp3 player. If this
  // happens this code tries to solve the Com Error 3
  /*
    if(mp3_error_code == 3) {
    Serial.println(".....");
    mp3_error_code = 0;
    dfmp3.begin();dfmp3.reset();dfmp3.setVolume(VOLUME);
    delay(200);
  }
  */




  // ========================================================================
  // MONITORING PHONE STATUS
  // ========================================================================


  //
  // 0 = RECEIVER PICKED UP (DIALING)
  // WAITING FOR DIALING
  if(playing==0 && phoneStatus==DIALING) {
      playTrackNum( random(8,10));
  }


  //
  // 4 = HANGEDUP
  // ALARM TIME CHECK
  if ( timer_1 >0 &&  millis()> timer_1 && phoneStatus==HANDSET_DOWN) {
    setPhoneStatus( RINGING ); //ring
    bells( caller_1 == "9" ? 999 : DEFAULT_RINGBELLS_REPEAT );
  }

  //
  // 3 = ANSWERING
  // answer to the alarm
  if (phoneStatus== ANSWERING && phoneNumber!="") {
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
    setPhoneStatus( CALL_ENDED );
    playTrackNum(1);
  }


  //
  // 0 = DIALING PROCESS
  if (phoneStatus== DIALING ) {
    readNumberDialed() ;
    
    if(phoneNumber!="") {
      Serial.println("> Call " + phoneNumber);

      //
      // found is true is number dialed match a service
      bool found = false;

      
      setPhoneStatus( CALLING ); // calling
      waitingForAnswer(); // waiting answer sound
  
  
  
      // #1   OR  #1[d]{1,3}  OR    #1-HH-MM
      // ---------------------------------------------------------------
      if(phoneNumber.charAt(0)=='1' && phoneNumber.length()<=5) {
        // NUMBER BEGINS WITH 1 TO SET TIME
        //
        setPhoneStatus( ANSWERING);
        found = true;
        if(phoneNumber.length()==1) {
            // TELL THE TIME
            tellTheTime();
        }
        if(phoneNumber.length()>1 && phoneNumber.length()<=5) {
            // SET ALARM WITH MINUTES OR WITH HOURS AND MINUTES
            setTheAlarm(phoneNumber);
        }
        setPhoneStatus( CALL_ENDED );
        playTrackNum(1);
      }
  
  
      
      // #2 DELETE ALARM
      // ---------------------------------------------------------------
      if(phoneNumber=="2") {
        found = true;
        timer_1 = 0;
        caller_1="";
        setPhoneStatus( ANSWERING );
        playTrackFolderNum(1,64,WAIT_END); // Sveglia cancellata
        setPhoneStatus( CALL_ENDED );
        playTrackNum(1);
      }
  
   
  
  
  
      // #21 RESTART WEMOS
      // ---------------------------------------------------------------
      if(phoneNumber=="21") {
        found = true;
        setPhoneStatus( ANSWERING );
        playTrackNum(17,WAIT_END); // Restarting...
        setPhoneStatus( CALL_ENDED);
        ESP.restart();
        
      }
  
      // #22 GET TIME FROM INTERNET AGAIN
      // ---------------------------------------------------------------
      if(phoneNumber=="22") {
        found = true;
        setPhoneStatus(ANSWERING);
        if(wifi) {
          playTrackFolderNum(1,70,WAIT_END); // "Setting date and time from internet..."
          setDateTimeFromWeb();
        } else {
          playTrackNum(18,WAIT_END); // "Internet not available"
        }
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
        
      }
  
      // #22-HH-MM SET TIME
      // ---------------------------------------------------------------
      if(phoneNumber.charAt(0)=='2' && phoneNumber.charAt(1)=='2' && phoneNumber.length()==6) {
        found = true;
        setPhoneStatus(ANSWERING);
        setTheTime(phoneNumber);
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }
  
      // #22-YYYY-MM-DD SET DATE (TO DO)
      // ---------------------------------------------------------------
      if(phoneNumber.charAt(0)=='2' && phoneNumber.charAt(1)=='2' && phoneNumber.length()==10) {
        found = true;
        setPhoneStatus(ANSWERING);
        setTheDate(phoneNumber);  // TO DO (mp3 answer not completed)
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }
  
      // #23 ACTIVATE AP
      // ---------------------------------------------------------------
      if(!found && phoneNumber=="23") {
        found = true;
        setPhoneStatus(ANSWERING);
        playTrackNum(19,WAIT_END); // AP attivo cerca Vintagephone...
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1,WAIT_END);
        Serial.println("AP");
        setupPortal();
        connectToWifi();
      }
  
  
      // #24 HEADS OR TAILS
      // ---------------------------------------------------------------
      if(!found && phoneNumber=="24") {
        found = true;
        setPhoneStatus(ANSWERING);
        playTrackNum(20,WAIT_END); // Lancio una monetina...
        int r = random(0,2);
        playTrackNum(15 + r,WAIT_END);
        Serial.println(r==0 ? "testa" : "croce");
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }
  
      // #25 YES OR NO
      // ---------------------------------------------------------------
      if(!found && phoneNumber=="25") {
        found = true;
        setPhoneStatus(ANSWERING);
        int r = random(0,2);
        playTrackNum(13 + r,WAIT_END);
        Serial.println(r==0 ? "yes" : "no");
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }
  
      // #26 PICK RANDOM FILE IN FOLDER 4
      // ---------------------------------------------------------------
      if(!found && phoneNumber=="26") {
        found = true;

        int maxRand = 4 + 1;
                
        setPhoneStatus(ANSWERING);
        int r = random(1,maxRand);
        playTrackFolderNum(4,r,WAIT_END);
        Serial.println("r = " + (String)r);
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }  
  
  
  
      // #4 METEO
      // ---------------------------------------------------------------
      if(phoneNumber=="4") {
        found = true;
        setPhoneStatus(ANSWERING);
        if(wifi) {
          tellMeMeteo( phoneNumber );
        } else {
          playTrackNum(18,WAIT_END); // Internet not available
        }
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }


      // #9  RING BELLS AFTER 5 SECONDS (TEST ROUTINE)
      // ---------------------------------------------------------------
      if(phoneNumber=="9") {
        setPhoneStatus( ANSWERING);
        found = true;
        setTheAlarm(phoneNumber);
        setPhoneStatus( CALL_ENDED );
        playTrackNum(1);
      }

      // #3456789 YOUR CUSTOM NUMBER THAT NOT MATCH PREVIOUS NUMBERS
      // ---------------------------------------------------------------
      if(phoneNumber=="3456789") {
        found = true;
        setPhoneStatus(ANSWERING);

        //
        // Add your code here
        // then play your tracks
        //
        // use playTrackNum(x, WAIT_END) to play mp3 number x in mp3 folder.
        // use playTrackFolderNum(x,y, WAIT_END); to play mp3 number y in x folder.
        // use WAIT_END if you want the process to wait the end of the mp3
        //
   
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }
      



      
      
      if(!found) {
        // NUMBER NOT FOUND
        //
        setPhoneStatus(ANSWERING);
        playTrackNum(random(10,13),WAIT_END); // modem o non attivo
        setPhoneStatus(CALL_ENDED);
        playTrackNum(1);
      }
  
  
      

  
      
     }
 }
}
