/*
  Copyright (c) 2018.

  Juergen Key. Alle Rechte vorbehalten.

  Weiterverbreitung und Verwendung in nichtkompilierter oder kompilierter Form,
  mit oder ohne Veraenderung, sind unter den folgenden Bedingungen zulaessig:

   1. Weiterverbreitete nichtkompilierte Exemplare muessen das obige Copyright,
  die Liste der Bedingungen und den folgenden Haftungsausschluss im Quelltext
  enthalten.
   2. Weiterverbreitete kompilierte Exemplare muessen das obige Copyright,
  die Liste der Bedingungen und den folgenden Haftungsausschluss in der
  Dokumentation und/oder anderen Materialien, die mit dem Exemplar verbreitet
  werden, enthalten.
   3. Weder der Name des Autors noch die Namen der Beitragsleistenden
  duerfen zum Kennzeichnen oder Bewerben von Produkten, die von dieser Software
  abgeleitet wurden, ohne spezielle vorherige schriftliche Genehmigung verwendet
  werden.

  DIESE SOFTWARE WIRD VOM AUTOR UND DEN BEITRAGSLEISTENDEN OHNE
  JEGLICHE SPEZIELLE ODER IMPLIZIERTE GARANTIEN ZUR VERFUEGUNG GESTELLT, DIE
  UNTER ANDEREM EINSCHLIESSEN: DIE IMPLIZIERTE GARANTIE DER VERWENDBARKEIT DER
  SOFTWARE FUER EINEN BESTIMMTEN ZWECK. AUF KEINEN FALL IST DER AUTOR
  ODER DIE BEITRAGSLEISTENDEN FUER IRGENDWELCHE DIREKTEN, INDIREKTEN,
  ZUFAELLIGEN, SPEZIELLEN, BEISPIELHAFTEN ODER FOLGENDEN SCHAEDEN (UNTER ANDEREM
  VERSCHAFFEN VON ERSATZGUETERN ODER -DIENSTLEISTUNGEN; EINSCHRAENKUNG DER
  NUTZUNGSFAEHIGKEIT; VERLUST VON NUTZUNGSFAEHIGKEIT; DATEN; PROFIT ODER
  GESCHAEFTSUNTERBRECHUNG), WIE AUCH IMMER VERURSACHT UND UNTER WELCHER
  VERPFLICHTUNG AUCH IMMER, OB IN VERTRAG, STRIKTER VERPFLICHTUNG ODER
  UNERLAUBTE HANDLUNG (INKLUSIVE FAHRLAESSIGKEIT) VERANTWORTLICH, AUF WELCHEM
  WEG SIE AUCH IMMER DURCH DIE BENUTZUNG DIESER SOFTWARE ENTSTANDEN SIND, SOGAR,
  WENN SIE AUF DIE MOEGLICHKEIT EINES SOLCHEN SCHADENS HINGEWIESEN WORDEN SIND.
*/
#include <FS.h> //this needs to be first, or it all crashes and burns...

//#define AT_HOME 0
#define REGELBETRIEB 1

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
//needed for wifimanager
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <GxEPD.h>
#include <GxGDEW075T8/GxGDEW075T8.cpp>      // 7.5" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "qr1.h"
#include "qr2.h"
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson

//
// Debug messages over the serial console.
//
#include "debug.h"

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>    // https://github.com/JChristensen/Timezone
#include <stdio.h>
#include <user_interface.h>
#include "wifi_security.h"


//Your Wifi SSID
const char* ssid = "your_ssid";
//Your Wifi Key
const char* password = "your_key";

ESP8266WebServer* server;

bool lamp = 0;

//
// The helper & object for the epaper display.
//
GxIO_Class io(SPI, SS, 0, 2);
GxEPD_Class display(io);

char port[6] = "80";
//
// The host we're going to fetch from.
//
char host[100] = "apache2.fritz.box";
char path[100] = "/";

//flag for saving data
bool shouldSaveConfig = false;

int gpio0Led = D0;

unsigned long previousMillis = 0;

WiFiUDP ntpUDP;

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP, "2.de.pool.ntp.org");

// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
// NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);

char timestrbuf[100];

int runningNumber=0;

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//
// Fetch and display the image specified at the given URL
//
int display_url(const char * m_path)
{
  int httpStatus=200;
  //
  // Clear the display.
  //
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  char ppath[120]= { '\0' };
  memset(ppath, '\0', sizeof(ppath));
  sprintf(ppath,"%s%d.pbm",m_path,runningNumber);

  /*
     Create a HTTP client-object.
  */
  WiFiClient m_client;
  int iport = atoi(port);

  //
  // Keep track of where we are.
  //
  bool finishedHeaders = false;
  bool currentLineIsBlank = true;
  String m_body = "";
  long now;

  Serial.println("About to make the connection to");
  Serial.print(host);
  Serial.println("#");
  Serial.print(port);
  Serial.println("#");
  Serial.println(ppath);

  //
  // Connect to the remote host, and send the HTTP request.
  //
  if (!m_client.connect(host, iport))
  {
    Serial.println("Failed to connect");
    return 503; //Service Unavailable
  } 

  Serial.println("Making HTTP-request\n");

  m_client.print("GET ");
  m_client.print(ppath);
  m_client.println(" HTTP/1.0");
  m_client.print("Host: ");
  m_client.println(host);
  m_client.println("User-Agent: epaper-web-image/1.0");
  m_client.println("Connection: close");
  m_client.println("");

  Serial.println("Made HTTP-request\n");

  now = millis();

  //
  // Test for timeout waiting for the first byte.
  //
  while (m_client.available() == 0)
  {
    if (millis() - now > 15000)
    {
      Serial.println(">>> Client Timeout !");
      m_client.stop();
      return 408; //REQUEST TIMEOUT
    }
  }

  int l = 0;

  //
  // Now we hope we'll have smooth-sailing, and we'll
  // read a single character until we've got it all
  //
  int pbmheaders = 2;
  unsigned char gImage[640 / 8] = {0};
  int byteIndexInLine = 0;
  int bitCount = 0;
  int lineIndex = 0;
  char magic[2];
  char status[4];
  int byteCounter=0;
  while (m_client.connected())
  {
    if (m_client.available())
    {
      char c = m_client.read();
      if (finishedHeaders)
      {
        byteCounter=0;
        if (pbmheaders == 0)
        {
          if(magic[1]=='1')
          {
            if (c == '1')
            {
              display.drawPixel(byteIndexInLine * 8 + bitCount, lineIndex, GxEPD_BLACK);
              gImage[byteIndexInLine] = (gImage[byteIndexInLine] << 1) | 0x1;
              ++bitCount;
              if (bitCount == 8)
              {
                bitCount = 0;
                ++byteIndexInLine;
                gImage[byteIndexInLine] = 0;
              }
            }
            else if (c == '0')
            {
              display.drawPixel(byteIndexInLine * 8 + bitCount, lineIndex, GxEPD_WHITE);
              //gImage[byteIndexInLine]=(gImage[byteIndexInLine]<<1)&0xfe;
              ++bitCount;
              if (bitCount == 8)
              {
                bitCount = 0;
                ++byteIndexInLine;
                gImage[byteIndexInLine] = 0;
              }
            }
            if (byteIndexInLine == 640 / 8)
            {
              if (lineIndex % 10 == 0)
                Serial.println("drawing a tenth line");
              //              display.drawBitmap(0, lineIndex, gImage, 640, 1, GxEPD_BLACK);
              ++lineIndex;
              bitCount = 0;
              byteIndexInLine = 0;
            }
          }
          else
          {
            for(int bc=0;bc<8;++bc)
            {
              int color=c&(0x80>>bc);
              display.drawPixel(byteIndexInLine * 8 + bc, lineIndex, color?GxEPD_BLACK:GxEPD_WHITE);
            }
            gImage[byteIndexInLine] = c;
            ++byteIndexInLine;
            gImage[byteIndexInLine] = 0;
            if (byteIndexInLine == 640 / 8)
            {
              if (lineIndex % 10 == 0)
                Serial.println("drawing a tenth line");
              //              display.drawBitmap(0, lineIndex, gImage, 640, 1, GxEPD_BLACK);
              ++lineIndex;
              bitCount = 0;
              byteIndexInLine = 0;
            }
          }
        }
        else
        {
          if (c == '\n')
          {
            --pbmheaders;
            Serial.println("PBM headers skipped");
          }
          if(byteCounter<2)
          {
            magic[byteCounter]=c;
            ++byteCounter;
          }
        }
      }
      else
      {
        if((byteCounter>8)&&(byteCounter<12))
          status[byteCounter-9]=c;
        ++byteCounter;
        if (currentLineIsBlank && c == '\n')
        {
          finishedHeaders = true;
          status[3]=0;
          Serial.print("HTTP status ");
          Serial.println(status);
          sscanf(status,"%d",&httpStatus);
          if(httpStatus!=200)
          {
            break;    
          }
        }
        
      }

      if (c == '\n')
        currentLineIsBlank = true;
      else if (c != '\r')
        currentLineIsBlank = false;

      // Wait for more packetses
      // delay(1);
    }
  }
  if(httpStatus==200)
  {
    Serial.println("Nothing more is available - terminating");
    m_client.stop();
  
    WiFi.mode(WIFI_OFF);
    //
    // Trigger the update of the display.
    //
    display.update();
  }
  else
  {
    m_client.stop();
  }
  return httpStatus;
}

void saveConfigToFs()
{
        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
        json["host"] = host;
        json["port"] = port;
        json["path"] = path;
        json["runningnumber"] = runningNumber;

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile)
        {
          Serial.println("failed to open config file for writing");
        }

        json.printTo(Serial);
        json.printTo(configFile);
        configFile.close();
        //end save
}

void displayQrCodes()
{
    display.setRotation(2);
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(0, 0);
    display.println();
    display.println("Please connect to SSID");
    display.println("AutoConnectAP_ePaper!");
    display.drawExampleBitmap(gImage_IMG_0002, 270, 90, 100, 100, GxEPD_BLACK);
    display.setCursor(0, 200);
    display.println();
    display.println("Then open configuration at");
    display.println("http://192.168.4.1!");
    display.drawExampleBitmap(gImage_IMG_0001, 270, 290, 100, 100, GxEPD_BLACK);
    display.update();
    display.setRotation(0);
}

void readConfigFromFlash()
{
  String realSize = String(ESP.getFlashChipRealSize());
  String ideSize = String(ESP.getFlashChipSize());
  bool flashCorrectlyConfigured = realSize.equals(ideSize);

  //read configuration from FS json
  Serial.println("mounting FS...");

  //  if(flashCorrectlyConfigured)
  {
    if (SPIFFS.begin())
    {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json"))
      {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile)
        {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);

          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success())
          {
            Serial.println("\nparsed json");
            strcpy(port, json["port"]);
            strcpy(host, json["host"]);
            strcpy(path, json["path"]);
            sscanf(json["runningnumber"],"%d",&runningNumber);
          }
          else
          {
            Serial.println("failed to load json config");
          }
        }
      }
    }
    else
    {
      Serial.println("failed to mount FS");
    }
    //end read
  }
  //  else
  //    else Serial.println("flash incorrectly configured, SPIFFS cannot start, IDE size: " + ideSize + ", real size: " + realSize);
}

void setup(void)
{
  rst_info *resetInfo;
  resetInfo = ESP.getResetInfoPtr();
  Serial.begin(115200);
  // delay(5000);
  //  Serial.println("");
  //clean FS, for testing
  //SPIFFS.format();
//  pinMode(gpio0Switch, INPUT_PULLUP); // Push Button for GPIO0 active LOW
  pinMode(gpio0Led, OUTPUT); // Push Button for GPIO0 active LOW

  readConfigFromFlash();

  // setup the display
  display.init();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  WiFi.persistent(true);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
#ifdef REGELBETRIEB
  Serial.println(analogRead(A0) >512 ? "HIGH" : "LOW");
  if (analogRead(A0) >512)
    WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str()); // reading data from EPROM, (last saved credentials)
  else
  {
    WiFi.persistent(false);
    WiFi.begin("geht", "nicht");
    WiFi.persistent(true);
  }
#else
  WiFi.begin(wifi_ssid,wifi_pwd); // reading data from EPROM, (last saved credentials)
#endif

//  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(gpio0Led, lamp);

  // Wait for WiFi
  Serial.println("");
  Serial.print("Verbindungsaufbau mit:  ");
  Serial.println(WiFi.SSID().c_str());

  while (WiFi.status() == WL_DISCONNECTED)            // last saved credentials
  {
    delay(500);
    if (lamp == 0)
    {
      digitalWrite(gpio0Led, 1);
      lamp = 1;
    }
    else
    {
      digitalWrite(gpio0Led, 0);
      lamp = 0;
    }
    Serial.print(".");
  }
  lamp = 0;
  digitalWrite(gpio0Led, 0);

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED)
  {
    Serial.printf("\nConnected successful to SSID '%s'\n", WiFi.SSID().c_str());
  }
  else
  {
    displayQrCodes();
    WiFi.mode(WIFI_OFF);
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    WiFiManagerParameter custom_port("Port", "Port", port, 5);
    WiFiManagerParameter custom_host("Host", "Host", host, 99);
    WiFiManagerParameter custom_path("Path", "Path", path, 99);
    wifiManager.addParameter(&custom_port);
    wifiManager.addParameter(&custom_host);
    wifiManager.addParameter(&custom_path);

    //reset settings - for testing
    //wifiManager.resetSettings();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    digitalWrite(gpio0Led, LOW);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP_12F647"
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("AutoConnectAP_ePaper"))
    {
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      digitalWrite(gpio0Led, HIGH);
      // WPS button I/O setup
      /*      Serial.printf("\nCould not connect to WiFi. state='%d'\n", status);
            Serial.println("Please press WPS button on your router, until mode is indicated.");
            Serial.println("next press the ESP module WPS button, router WPS timeout = 2 minutes");

            while(digitalRead(gpio0Switch) == HIGH)  // wait for WPS Button active
            {
              delay(50);
              if(lamp == 0){
                 digitalWrite(gpio0Led, 1);
                 lamp = 1;
               }else{
                 digitalWrite(gpio0Led, 0);
                 lamp = 0;
              }
              Serial.print(".");
              yield(); // do nothing, allow background work (WiFi) in while loops
            }
            Serial.println("WPS button pressed");
            lamp=0;
            digitalWrite(gpio0Led, 0);

            if(!startWPSPBC()) {
               Serial.println("Failed to connect with WPS :-(");
            } else*/
      {
        WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str()); // reading data from EPROM,
        while (WiFi.status() == WL_DISCONNECTED)            // last saved credentials
        {
          delay(500);
          Serial.print("."); // show wait for connect to AP
        }
      }
    }
    else
    {
      Serial.println(custom_port.getValue());
      Serial.println(custom_host.getValue());
      Serial.println(custom_path.getValue());
      strcpy(port, custom_port.getValue());
      strcpy(host, custom_host.getValue());
      strcpy(path, custom_path.getValue());

      //save the custom parameters to FS
      if (shouldSaveConfig)
      {
        saveConfigToFs();
      }

    }
  }

  Serial.println("Verbunden");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  Serial.println("Led an!");
  digitalWrite(gpio0Led, LOW);
  if(resetInfo->reason == REASON_DEEP_SLEEP_AWAKE)
  {
    ++runningNumber;
    saveConfigToFs();
    int httpStatus=display_url(path);
    if(httpStatus!=200)
    {
      Serial.println("Page not found, starting over!");
      runningNumber=0;
      saveConfigToFs();
      httpStatus=display_url(path);
    }
  }
  Serial.println("Going into deep sleep");
  Serial.println("Led aus!");
  digitalWrite(gpio0Led, HIGH);
  ESP.deepSleep(0);
}
void printTimeToBuffer(time_t t, char *tz)
{
  //this is needed because dayShortStr and monthShortStr obviously use the same buffer so we get
  //20:30:59 Aug 17 Aug 2018 CEST
  //instead of
  //20:30:59 Fri 17 Aug 2018 CEST
  //if we dont do this!
  String wd(dayShortStr(weekday(t)));
  sprintf(timestrbuf, "%02d:%02d:%02d %s %02d %s %d %s", hour(t), minute(t), second(t), wd.c_str(), day(t), monthShortStr(month(t)), year(t), tz);
}

void loop(void)
{

}

