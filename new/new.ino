//----------------------------------AYARLAR----------------------------------
const String BENIM_IDM = "36";  //hepsini yeniden yükle. gelen msesaj parçalama değişti
//----------------------------------AYARLAR----------------------------------


#include <Adafruit_SleepyDog.h>
#include <SoftwareSerial.h>
#include <EEPROM.h> //ben ekledim
#include "Adafruit_FONA.h"
#include "Adafruit_MQTT.h" //haberleşme protokolü
#include "Adafruit_MQTT_FONA.h"
#define halt(s) { Serial.println(F( s )); while(1);  }

extern Adafruit_FONA fona;
extern SoftwareSerial fonaSS; // GSM


#define FONA_RX  8  // GSM pin tanımları
#define FONA_TX  7
#define FONA_RST 9

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX); //tx=7 rx=8
Adafruit_FONA fona = Adafruit_FONA(FONA_RST); 

#define FONA_APN       "internet"
#define FONA_USERNAME  ""
#define FONA_PASSWORD  ""
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "gsmproje"
#define AIO_KEY         "aio_KLFb26z3d7yqpE4NF0NhPsot0AeA"

Adafruit_MQTT_FONA mqtt(&fona, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
#define halt(s) { Serial.println(F( s )); while(1);  }
boolean FONAconnect(const __FlashStringHelper *apn, const __FlashStringHelper *username, const __FlashStringHelper *password);

Adafruit_MQTT_Subscribe gelen = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/sulamaServerdenGiden"); // MQTT'den mesajı alan
Adafruit_MQTT_Publish gonder = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/sulamaCihazdanGiden"); // MQTT'den mesaj yayınlama

uint8_t txfailures = 0;
#define MAXTXFAILURES 5
#define ROLE_PIN 12 // röle pin bağlantısı

bool msjBanamiGeldi = false;
String gelenVeri = "", id = BENIM_IDM, konu = "-2", msj = "-2", msgId = "1111", mtrMsgId = "2222";

unsigned long motorCalismaSuresi = 0, motorCalistiZaman = 0;
bool motorCalisiyorMu = false;

#define BSLNGC_STR "*{"
#define BITIS_STR "}*"


void setup() {
  pinMode(ROLE_PIN, OUTPUT);
  digitalWrite(ROLE_PIN, LOW);
  Serial.begin(115200); //115200 Baud rate
  Serial.println(F("Sstm bslyr"));


  int resetSayi = eepromdanOku() + 1;
  eepromaYaz(resetSayi);
  Serial.print(F("rst:")); Serial.println(resetSayi);

  mqtt.subscribe(&gelen);
  Watchdog.reset(); delay(5000); Watchdog.reset();

  while (! FONAconnect(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD))) {
    Serial.println(F("GSM tkrr dnnyr"));
  }

  Serial.println(F("Baz istsynna bglndi"));
  uint8_t rssi = fona.getRSSI();
  Serial.print(F("rssi:"));
  Serial.println(rssi);
  Watchdog.reset();
  delay(5000); Watchdog.reset();
  MQTT_connect(); // haberleşme cihazına bağlantı kurdu
  uint8_t sayac1 = 0; // timer mı ?

  Serial.println(F("ilk msj gndrlyr"));
  while (!MQTT_gonder("05", "rs:" + String(resetSayi) + "_sq:" + String(rssi), msgId)) {
    rssi = fona.getRSSI();
    Serial.print(F("rssi:")); Serial.println(rssi);
    sayac1++;
    Serial.print(F(".")); Serial.println(sayac1);
    Watchdog.reset();
    MQTT_connect();
    if (sayac1 > 20) {
      break;
    }
  }
}

void loop() {
  Watchdog.reset();
  MQTT_connect();

  Watchdog.reset();
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &gelen) {
      gelenVeri = (char *)gelen.lastread;
      Serial.print(F("Gln:>")); Serial.print(gelenVeri); Serial.println(F("<"));

      if (gelenVeri.substring(0, 2) == BSLNGC_STR && gelenVeri.indexOf(BITIS_STR) != -1 && gelenVeri.indexOf(";") != -1 && gelenVeri.indexOf(":") != -1 && gelenVeri.indexOf("-") != -1) {
        //Serial.println(F("Anlamli veri geldi..."));
        id = gelenVeri.substring(gelenVeri.indexOf(BSLNGC_STR) + 2 , gelenVeri.indexOf(";"));
        //Serial.print(F("1id:")); Serial.println(id);

        if (id == BENIM_IDM) {
          msjBanamiGeldi = true;
          //Serial.println(F("Mesaj bana geldi"));
          konu = gelenVeri.substring(gelenVeri.indexOf(";") + 1 , gelenVeri.indexOf(":"));
          //Serial.print(F("1konu:")); Serial.println(konu);
          msj = gelenVeri.substring(gelenVeri.indexOf(":") + 1 , gelenVeri.indexOf("-"));
          //Serial.print(F("1msj:")); Serial.println(msj);
          msgId = gelenVeri.substring(gelenVeri.indexOf("-") + 1 , gelenVeri.indexOf(BITIS_STR));
          //Serial.print(F("1mId:")); Serial.println(msgId);

          if (konu == "00") {
            MQTT_gonder("00", "1", msgId);
            msjBanamiGeldi = false;
            Serial.println(F("tst"));
          }
          else if (konu == "01") {
            //Serial.println(F("role konusu"));
            digitalWrite(ROLE_PIN, HIGH);
            Serial.println(F("r1"));
            mtrMsgId = msgId;
            MQTT_gonder("03", msj, msgId);
            msjBanamiGeldi = false;
            motorCalisiyorMu = true;
            motorCalistiZaman = millis();
            motorCalismaSuresi = msj.toInt() * 1000 * 60;
          }
          else if (konu == "02", msgId) {
            digitalWrite(ROLE_PIN, LOW);
            Serial.println(F("r0"));
            mtrMsgId = msgId;
            MQTT_gonder("04", "0", msgId);
            msjBanamiGeldi = false;
            motorCalisiyorMu = false;
          }
          else {
            Serial.println(F("bska knu"));
          }
        }
        else {
          Serial.println(F("Msj bsksna gldi"));
        }
      }
      else {
        Serial.println(F("HTLI vri gldi"));
      }
    }
  }

  if (motorCalisiyorMu && millis() - motorCalistiZaman >= motorCalismaSuresi) {
    digitalWrite(ROLE_PIN, LOW);
    Serial.println(F("r0"));
    MQTT_gonder("04", "0", mtrMsgId);
    msjBanamiGeldi = false;
    motorCalisiyorMu = false;
  }

  if (!mqtt.ping()) {
    Serial.println(F("PngX"));
  }
}


void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) {
    return;
  }

  Serial.print(F("MQTT bglntsi krlyr"));

  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret)); Serial.println(F("5sn snra tkrr dnnck"));
    mqtt.disconnect();
    delay(5000);
  }
  Serial.println(F("MQTT bglndi"));
}

bool MQTT_gonder(String konu2, String msj2, String mId2) {
  Watchdog.reset();
  Serial.print(F("Gndrlyr>"));
  String yeniMesaj = BSLNGC_STR + BENIM_IDM + ";" + konu2 + ":" + msj2 + "-" + mId2 + BITIS_STR;
  char yeniMesaj2[50];
  yeniMesaj.toCharArray(yeniMesaj2, 50);
  Serial.print(yeniMesaj2);
  if (!gonder.publish(yeniMesaj2)) {
    Serial.println(F("<___olmdi"));
    txfailures++;
    return false;
  }
  else {
    Serial.println(F("<OK!"));
    txfailures = 0;
    msjBanamiGeldi = false;
    return true;
  }
}


void eepromaYaz(unsigned int n) {
  EEPROM.write(0, n >> 8);
  EEPROM.write(0 + 1, n & 0xFF);
}

unsigned int eepromdanOku() {
  return (EEPROM.read(0) << 8) + EEPROM.read( 1);
}


boolean FONAconnect(const __FlashStringHelper *apn, const __FlashStringHelper *username, const __FlashStringHelper *password) {
  Watchdog.reset();

  Serial.println(F("GSM aclyr"));

  fonaSS.begin(9600);

  if (! fona.begin(fonaSS)) {
    Serial.println(F("GSM htsi"));
    return false;
  }
  fonaSS.println("AT+CMEE=2");
  Serial.println(F("GSM OK"));
  Watchdog.reset();
  Serial.println(F("Ag kntrl edlyr"));
  while (fona.getNetworkStatus() != 1) {
    delay(500);
  }

  Watchdog.reset();
  delay(5000);
  Watchdog.reset();

  fona.setGPRSNetworkSettings(apn, username, password);

  Serial.println(F("GPRS iptl edlyr"));
  fona.enableGPRS(false);

  Watchdog.reset();
  delay(5000);
  Watchdog.reset();

  Serial.println(F("GPRS aclyr"));
  if (!fona.enableGPRS(true)) {
    Serial.println(F("GPRS aclmdi"));
    return false;
  }
  Watchdog.reset();

  return true;
}
