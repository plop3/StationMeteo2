void infoMeteo() {


  envoiHTTP();

  Delai5mn++;
  delaiRain30mn++;
  if (Delai5mn >= 5) {
    Delai5mn = 0;
    updateRain5mn = true;	// Envoi des données du pluviomètre
  }
  if (delaiRain30mn >= 240) {
    delaiRain30mn = 0;
    updateRain30mn = true;
  }
}


void envoiHTTP() {
  //Serial.println("Envoi données");
  // Anémomètre
  WindMax = 0;
  ///  http.begin(client, "http://192.168.0.4:8082/json.htm?type=command&param=udevice&idx=11&nvalue=0&svalue=" + String(Dir) + ";" + DirT[DirS] + ";" + String(Wind) + ";" + String(Gust) + ";" + String(Tp) + ";" + String(WindChild));
  client.publish("meteo2/windspeed", String(Wind).c_str(), true);
  client.publish("meteo2/gust", String(Gust).c_str(), true);
  client.publish("meteo2/winddir", String(Dir).c_str(), true);
  client.publish("meteo2/winddirs", DirT[DirS].c_str(), true);
  if (Rain != LastRain) {
    LastRain = Rain;
    // Capteur de pluie
    client.publish("meteo2/rain", Rain ? "ON" : "OFF", true);
    //http.begin(client, "http://192.168.0.4:8082/json.htm?type=command&param=switchlight&idx=3572&switchcmd=" + String((Rain) ? "on" : "off"));
    //json.htm?type=command&param=switchlight&idx=99&switchcmd=On

  }
}

//-----------------------------------------------

void sendWindSpeed() {
  server.send ( 200, "text/plain", String(Wind * 0.36));
}

void sendGustSpeed() {
  server.send(200, "text/plain", String(Gust * 0.36));
}

void sendRain() {
  server.send ( 200, "text/plain", String(Rain));
}

ICACHE_RAM_ATTR void rainCount() {
  // Incrémente le compteur de pluie
  //if (!digitalRead(PINrain)) {
  updateRain = true;
  //}
}

#ifdef CORAGE
void sendOrage() {
  int distance = mod1016.calculateDistance();
  /* if (distance == -1) {
     //Serial.println("Lightning out of range");
    else if (distance == 1)
     //Serial.println("Distance not in table");
    else if (distance == 0)
     //Serial.println("Lightning overhead");
    else {
  */
  if (distance > 1) {
    //Serial.print("Lightning ~");
    //Serial.print(distance);
    //Serial.println("km away\n");
    //Serial.print("Intensité: ");Serial.println(intensite);
    http.begin(client, "http://192.168.0.7:8080/json.htm?type=command&param=udevice&idx=3564&nvalue=0&svalue=" + String(distance));
    http.GET();
    http.end();
    http.begin(client, "http://192.168.0.7:8080/json.htm?type=command&param=udevice&idx=3568&nvalue=0&svalue=1");
    http.GET();
    http.end();
  }
}
#endif

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
