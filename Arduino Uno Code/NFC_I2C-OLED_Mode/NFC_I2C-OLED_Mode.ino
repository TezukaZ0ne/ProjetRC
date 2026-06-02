#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include <SoftwareSerial.h>

PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc(pn532_i2c);

#define OLED_TX 2
#define OLED_RX 3
SoftwareSerial oledSerial(OLED_RX, OLED_TX);

#define MAX_LINES 6
String screenBuffer[MAX_LINES];

void sendToNextion(String cmd) {
  oledSerial.print(cmd);
  oledSerial.write(0xFF);
  oledSerial.write(0xFF);
  oledSerial.write(0xFF);
}

void clearScreen() {
  for (int i = 0; i < MAX_LINES; i++) {
    sendToNextion("t" + String(i) + ".txt=\"\"");
    screenBuffer[i] = "";
  }
}

void addLine(String msg) {
  Serial.println(msg);

  for (int i = 0; i < MAX_LINES - 1; i++) {
    screenBuffer[i] = screenBuffer[i + 1];
  }
  screenBuffer[MAX_LINES - 1] = msg;

  for (int i = 0; i < MAX_LINES; i++) {
    sendToNextion("t" + String(i) + ".txt=\"" + screenBuffer[i] + "\"");
  }
}

void afficherAcces(String ligne1, String ligne2) {
  clearScreen();
  addLine(ligne1);
  addLine(ligne2);
}

void setup() {
  Serial.begin(115200);
  oledSerial.begin(9600);

  delay(500);
  sendToNextion("rest");
  delay(1000);

  clearScreen();
  addLine("System init...");
  nfc.begin();
  addLine("NFC ready");
  addLine("Scan card...");
}

void loop() {

  if (nfc.tagPresent()) {
    NfcTag tag = nfc.read();
    String uid = tag.getUidString();

    uid.replace(" ", "");
    uid.toUpperCase();

    addLine("Card detected");
    addLine("UID:" + uid);

    Serial.println("UID:" + uid);

    unsigned long debut = millis();
    String reponse = "";

    while (millis() - debut < 5000) {
      if (Serial.available()) {
        reponse = Serial.readStringUntil('\n');
        reponse.trim();
        break;
      }
    }

    if (reponse == "ACCES AUTORISE") {
      afficherAcces("ACCES", "AUTORISE");
    } else if (reponse == "ACCES REFUSE") {
      afficherAcces("ACCES", "REFUSE");
    } else {
      afficherAcces("ERREUR", "PAS DE REP.");
    }

    delay(3000);
    clearScreen();
    addLine("Scan card...");
  }

  delay(500);
}
