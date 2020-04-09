/*
	Station météo ESP8266
	Serge CLAUS
	GPL V3
	Version 3.0
	12/03/2020

*/

//----------------------------------------
#include "StationMeteo2.h"

#include <SimpleTimer.h>
SimpleTimer timer;

//OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Serveur Web
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

// Client Web
#include <ESP8266HTTPClient.h>
HTTPClient http;
WiFiClient client;

// EEprom
#include <EEPROM.h>

#include "WiFiP.h"

const char* ssid = STASSID;
const char* password = STAPSK;

#if defined CORAGE
// Mod1016
#include <AS3935.h>
#include <Wire.h>
#endif

// Serveur Web
ESP8266WebServer server ( 80 );

//----------------------------------------

bool Rain = false;      // Pluie en cours (pluviomètre)
bool LastRain = !Rain;
// Pluie
unsigned int CountRain = 0;
int CountBak = 0;		// Sauvegarde des données en EEPROM / 24H
volatile bool updateRain = false;
bool updateRain5mn = true;
bool updateRain30mn = false;
int delaiRain30mn = 0;
unsigned long PrevTime, OldTime;
int rainRate = 0;
bool pluieEnCours = false;
int oldAlert = 0;
//unsigned long RainTab[60];

// TX20 anémomètre
volatile boolean TX20IncomingData = false;
unsigned char chk;
unsigned char sa, sb, sd, se;
unsigned int sc, sf, pin;
String tx20RawDataS = "";
unsigned int Wind, Gust, Dir, DirS;
unsigned int WindMax = 0;
float WindChild, WindKMS;
String DirT[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};

#if defined CORAGE
// MOD1016
bool detected = false;
#endif

// Température extérieure
float Tp = 20;

// Divers
int Delai5mn = 0;		// Timer 5mn

//----------------------------------------

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // EEPROM
  EEPROM.begin(512);
  // OTA
  WiFi.mode(WIFI_STA);
  IPAddress ip(192, 168, 0, 14);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 0, 1);
  IPAddress gateway(192, 168, 0, 1);
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  byte i = 5;
  while ((WiFi.waitForConnectResult() != WL_CONNECTED) && (i > 0)) {
    Serial.println("Connection Failed...");
    delay(5000);
    //i--; // La station doit être connectée pour fonctionner
  }
  ArduinoOTA.setHostname("anemometre");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
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
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // Lecture des données de l'eeprom
  // L'adresse 0 doit correspondre à 24046 Sinon, initialisation des valeurs
  int Magic;
  EEPROM.get(0, Magic);
  if (Magic != 24046) {
    // Initialisation des valeurs
    Magic = 24046;
    EEPROM.put(0, Magic);
    EEPROM.put(4, CountRain);
    EEPROM.commit();
  }
  else {
    // Sinon, récupération des données
    EEPROM.get(4, CountRain);
  }

  // Timers
  timer.setInterval(60000L, infoMeteo);	  // Mise à jour des données barométriques et envoi des infos à Domoticz
#if defined CORAGE
  // MOD-1016
  Wire.begin();
  mod1016.init(IRQ_ORAGE);
  delay(2);
  autoTuneCaps(IRQ_ORAGE);
  delay(2);
  //mod1016.setTuneCaps(6);
  //delay(2);
  mod1016.setOutdoors();
  delay(2);
  mod1016.setNoiseFloor(5);     // Valeur par defaut 5
  mod1016.writeRegister(0x02, 0x30, 0x03 << 4);
  delay(200);
  pinMode(IRQ_ORAGE, INPUT);
  attachInterrupt(digitalPinToInterrupt(IRQ_ORAGE), orage, RISING);
  mod1016.getIRQ();
#endif

  // Pluviomètre
  pinMode(PINrain, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PINrain), rainCount, RISING);
  PrevTime = millis() - 36000000L;

  // TX20 anémomètre
  pinMode(DATAPIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DATAPIN), isTX20Rising, RISING);

  // Serveur Web
  server.begin();
  server.on ("/wind", sendWindSpeed);
  server.on("/gust", sendGustSpeed);
  server.on("/rain", sendRain);
  // Lecture des infos des capteurs initiale
  //infoMeteo();
  mesureCapteurs();
}

//----------------------------------------

void loop() {
  // OTA
  ArduinoOTA.handle();
  // Serveur Web
  server.handleClient();
  // Maj
  timer.run();

  // Pluviometre
  unsigned long currentTime = millis();
  // Impulsion détectée ?
  if (updateRain) {  
      CountRain += Plevel;
      if (pluieEnCours) {
      // La dernière impulsion date de moins d'une heure
      OldTime= PrevTime;
      delaiRain30mn = 0;
      rainRate = 3600000.0 * 100.0 * Plevel  / (unsigned long)(currentTime - PrevTime); //mm*100
    }
    else {
      pluieEnCours = true;
      OldTime = currentTime;
    }
    PrevTime = currentTime;
    updateRain = false;
    updateRain5mn = false;
    Delai5mn=0;
    sendPluv();
    delay(1000); //Anti-parasites
  }
  // Mise à jour du pluviomètre
  if (updateRain5mn)
  {
    if (pluieEnCours && (PrevTime != OldTime)) {
            rainRate = 3600000.0 * 100.0 * Plevel  / (unsigned long)(currentTime - OldTime); //mm*100
    }
    else {
      rainRate = 0;
    }
    updateRain5mn = false;
    sendPluv();
  }

   if (updateRain30mn) { // 60mn sans pluie -> plus de pluie
    updateRain30mn=false;
    rainRate = 0;
    pluieEnCours = false;
    Rain = false;
    PrevTime = currentTime - 36000000L;
  }

#if defined CORAGE
  // MOD1016
  if (detected) {
    translateIRQ(mod1016.getIRQ());
    detected = false;
  }
#endif
  // TX20 anémomètre
  if (TX20IncomingData) {
    if (readTX20()) {
      // Data OK
      Wind = sf;
      Dir = sb * 22.5;
      DirS = sb;
      float WindKMH = Wind * 0.36;
      if (WindMax < Wind) WindMax = Wind;
      Gust = WindMax;
      if (WindKMH < 4.8) {
        WindChild = Tp + 0.2 * (0.1345 * Tp - 1.59) * WindKMH;
      }
      else {
        WindChild = 13.12 + 0.6215 * Tp + (0.3965 * Tp - 11.37) * pow(WindKMH, 0.16);
      }
    }
  }
}

void sendPluv() {
  Rain = (rainRate > 1) ? true : false;
    // Envoi des données à Domoticz
    http.begin(client, "http://192.168.0.7:8080/json.htm?type=command&param=udevice&idx=3561&nvalue=0&svalue=" + String(rainRate) + ";" + String(CountRain / 1000.0));
    http.GET();
    http.end();
    String msg = "Pas\%20de\%20pluie";
    int Alert = 1;
    if (rainRate > 0 && rainRate <= 200) {
      msg = "Pluie\%20faible";
      Alert = 2;
    };
    if (rainRate > 200 && rainRate <= 760) {
      msg = "Pluie\%20moddérée";
      Alert = 3;
    };
    if (rainRate > 760) {
      msg = "Pluie\%20forte";
      Alert = 4;
    };
    if (oldAlert != Alert) {
      http.begin(client, "http://192.168.0.7:8080/json.htm?type=command&param=udevice&idx=3575&nvalue=" + String(Alert) + "&svalue=" + msg);
      http.GET();
      http.end();
      oldAlert = Alert;
    }
}
#if defined CORAGE
ICACHE_RAM_ATTR void orage() {
  detected = true;
}
#endif

#ifdef CORAGE
void translateIRQ(uns8 irq) {
  switch (irq) {
    case 1:
      //Serial.println("NOISE DETECTED");
      break;
    case 4:
      //Serial.println("DISTURBER DETECTED");
      break;
    case 8:
      Serial.println("LIGHTNING DETECTED");
      sendOrage();
      break;
  }
}
#endif


ICACHE_RAM_ATTR void isTX20Rising() {
  if (!TX20IncomingData) {
    TX20IncomingData = true;
  }
}

boolean readTX20() {
  int bitcount = 0;

  sa = sb = sd = se = 0;
  sc = 0; sf = 0;
  tx20RawDataS = "";

  for (bitcount = 41; bitcount > 0; bitcount--) {
    pin = (digitalRead(DATAPIN));
    if (!pin) {
      tx20RawDataS += "1";
    } else {
      tx20RawDataS += "0";
    }
    if ((bitcount == 41 - 4) || (bitcount == 41 - 8) || (bitcount == 41 - 20)  || (bitcount == 41 - 24)  || (bitcount == 41 - 28)) {
      tx20RawDataS += " ";
    }
    if (bitcount > 41 - 5) {
      // start, inverted
      sa = (sa << 1) | (pin ^ 1);
    } else if (bitcount > 41 - 5 - 4) {
      // wind dir, inverted
      sb = sb >> 1 | ((pin ^ 1) << 3);
    } else if (bitcount > 41 - 5 - 4 - 12) {
      // windspeed, inverted
      sc = sc >> 1 | ((pin ^ 1) << 11);
    } else if (bitcount > 41 - 5 - 4 - 12 - 4) {
      // checksum, inverted
      sd = sd >> 1 | ((pin ^ 1) << 3);
    } else if (bitcount > 41 - 5 - 4 - 12 - 4 - 4) {
      // wind dir
      se = se >> 1 | (pin << 3);
    } else {
      // windspeed
      sf = sf >> 1 | (pin << 11);
    }

    delayMicroseconds(1220);
  }
  chk = ( sb + (sc & 0xf) + ((sc >> 4) & 0xf) + ((sc >> 8) & 0xf) ); chk &= 0xf;
  delayMicroseconds(2000);  // just in case
  TX20IncomingData = false;

  if (sa == 4 && sb == se && sc == sf && sd == chk) {
    return true;
  } else {
    return false;
  }
}
