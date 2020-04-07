void infoMeteo() {
  // Lecture des capteurs
  mesureCapteurs();

  CountBak++;
  // Sauvegarde des données journalière
  if (CountBak > 1440) { //1440 minutes=24H
    CountBak = 0;
    EEPROM.put(4, CountRain);
    EEPROM.commit();
  }

  /*
    // Envoi des données:
    Serial.println("Tciel=" + String(skyT));
    Serial.println("CouvN=" + String(Clouds));
    Serial.println("Text=" + String(Tp));
    Serial.println("Hext=" + String(HR));
    Serial.println("Pres=" + String(P / 100));
    //  Serial.println("IR=" + String(ir));
    Serial.println("Dew=" + String(Dew));
    Serial.println("UV=" + String(UVindex));
    Serial.println("Lux=" + String(luminosite));
    //Serial.println("SQM=20.3");
    //Serial.println("Vent=15");
  */
  envoiHTTP();

  Delai5mn++;
  delaiRain30mn++;
  if (Delai5mn > 4) {
    Delai5mn = 0;
    updateRain5mn = true;	// Envoi des données du pluviomètre
  }
}

void mesureCapteurs() {
#ifdef RTEMP
  http.begin(client, "http://192.168.0.19/temp");
  if (http.GET() == 200)  {
    Tp = http.getString().toFloat();
    //Serial.println(Tp);
  }
  //else Tp = 0; // Lecture impossible, on garde la dernière valeur valide
  http.end();
#endif

}

void envoiHTTP() {
  //Serial.println("Envoi données");
  // Anémomètre
  WindMax = 0;
  http.begin(client, "http://192.168.0.7:8080/json.htm?type=command&param=udevice&idx=3570&nvalue=0&svalue=" + String(Dir) + ";" + DirT[DirS] + ";" + String(Wind) + ";" + String(Gust) + ";" + String(Tp) + ";" + String(WindChild));
  http.GET();
  http.end();
  if (Rain != LastRain) {
    LastRain = Rain;
    // Capteur de pluie
    http.begin(client, "http://192.168.0.7:8080/json.htm?type=command&param=switchlight&idx=3572&switchcmd=" + String((Rain) ? "On" : "Off"));
    //json.htm?type=command&param=switchlight&idx=99&switchcmd=On
    http.GET();
    http.end();
  }
}

//-----------------------------------------------

void sendWindSpeed() {
  server.send ( 200, "text/plain", String(Wind / 10.0));
}

void sendGustSpeed() {
  server.send(200, "text/plain", String(Gust / 10.0));
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
