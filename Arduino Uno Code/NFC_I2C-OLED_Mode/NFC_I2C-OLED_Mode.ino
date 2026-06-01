#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include <SoftwareSerial.h>

// -------- NFC FOR PN532 --------
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc(pn532_i2c);

// -------- OLED UART (Nextion) --------
#define OLED_TX 2
#define OLED_RX 3
SoftwareSerial oledSerial(OLED_RX, OLED_TX);

// -------- Screen Buffer --------
#define MAX_LINES 6
String screenBuffer[MAX_LINES];

// -------- Nextion Function --------
void sendToNextion(String cmd) {
  oledSerial.print(cmd);
  oledSerial.write(0xFF);
  oledSerial.write(0xFF);
  oledSerial.write(0xFF);
}

// -------- Clear screen --------
void clearScreen() {
  for (int i = 0; i < MAX_LINES; i++) {
    sendToNextion("t" + String(i) + ".txt=\"\"");
    screenBuffer[i] = "";
  }
}

// -------- Ajout ligne (console) --------
void addLine(String msg) {

  // Serial Monitor
  Serial.println(msg);

  // Scroll
  for (int i = 0; i < MAX_LINES - 1; i++) {
    screenBuffer[i] = screenBuffer[i + 1];
  }

  screenBuffer[MAX_LINES - 1] = msg;

  // Refresh screen
  for (int i = 0; i < MAX_LINES; i++) {
    sendToNextion("t" + String(i) + ".txt=\"" + screenBuffer[i] + "\"");
  }
}

// -------- Setup --------
void setup() {
  Serial.begin(115200);
  oledSerial.begin(9600);

  delay(500);

  // Reset écran (supprime anciens boutons affichés)
  sendToNextion("rest");
  delay(1000);

  clearScreen();

  addLine("System init...");
  
  // NFC init
  nfc.begin();
  addLine("NFC ready");
  addLine("Scan card...");
}

// -------- Loop --------
void loop() {

  if (nfc.tagPresent()) {
    NfcTag tag = nfc.read();

    String uid = tag.getUidString();

    addLine("Card detected");
    addLine("UID:");
    addLine(uid);
  }

  delay(1000);
}