#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include <SoftwareSerial.h>

// ── NFC I2C ───────────────────────────────────────────────
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc(pn532_i2c);

// ── Nextion (écran) : TX=2, RX=3 ─────────────────────────
#define OLED_TX 2
#define OLED_RX 3
SoftwareSerial oledSerial(OLED_RX, OLED_TX);

// ── Module GSM A6 : TX=7, RX=6 ───────────────────────────
#define GSM_TX 6
#define GSM_RX 5
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ── Nextion : buffer écran ────────────────────────────────
#define MAX_LINES 6
String screenBuffer[MAX_LINES];

long gsmBaudRate = 0; // baud rate détecté

// ─────────────────────────────────────────────────────────
//  NEXTION
// ─────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────
//  GSM A6
// ─────────────────────────────────────────────────────────

bool gsmAttendre(const char* expected, unsigned long timeout = 5000) {
  String reponse = "";
  unsigned long debut = millis();
  while (millis() - debut < timeout) {
    while (gsmSerial.available()) {
      reponse += (char)gsmSerial.read();
    }
    if (reponse.indexOf(expected) != -1) return true;
  }
  Serial.print("[GSM] Rep brute: ");
  Serial.println(reponse);
  return false;
}

bool gsmTesterBaud(long baud) {
  Serial.print("[GSM] Test baud rate: ");
  Serial.println(baud);

  gsmSerial.begin(baud);
  gsmSerial.listen();
  delay(500);
  while (gsmSerial.available()) gsmSerial.read(); // flush

  for (int i = 0; i < 3; i++) {
    gsmSerial.println("AT");
    if (gsmAttendre("OK", 2000)) {
      Serial.print("[GSM] OK detecte a ");
      Serial.println(baud);
      return true;
    }
    delay(300);
  }
  return false;
}

void gsmInit() {
  oledSerial.end();
  delay(5000); // attente démarrage A6

  // Envoi AT+IPR=9600 à l'aveugle sur tous les baud rates
  // pour forcer le A6 à 9600 quoi qu'il arrive
  long bauds[] = { 115200, 57600, 38400, 19200, 9600 };
  int nbBauds = 5;
  Serial.println("[GSM] Forçage IPR=9600 sur tous baud rates...");
  for (int b = 0; b < nbBauds; b++) {
    gsmSerial.begin(bauds[b]);
    gsmSerial.listen();
    delay(200);
    while (gsmSerial.available()) gsmSerial.read();
    gsmSerial.println("AT+IPR=9600");
    delay(300);
  }

  // Attendre que le A6 finisse son boot complètement
  // On attend que le flux serie soit silencieux pendant 3s
  gsmSerial.begin(9600);
  gsmSerial.listen();
  delay(2000);

  Serial.println("[GSM] Attente silence boot A6...");
  unsigned long derniereByte = millis();
  while (millis() - derniereByte < 3000) {
    if (gsmSerial.available()) {
      gsmSerial.read();
      derniereByte = millis(); // reset si on reçoit encore qqch
    }
  }
  Serial.println("[GSM] Boot A6 termine, envoi AT...");

  bool ok = false;
  for (int i = 0; i < 5; i++) {
    while (gsmSerial.available()) gsmSerial.read(); // flush
    gsmSerial.println("AT");
    if (gsmAttendre("OK", 3000)) { ok = true; break; }
    delay(1000);
  }

  if (!ok) {
    Serial.println("[GSM] Module A6 non repondant !");
    oledSerial.begin(9600);
    return;
  }
  gsmBaudRate = 9600;
  Serial.println("[GSM] A6 verrouille a 9600 baud.");

  gsmSerial.println("ATE0");         // Désactiver écho
  gsmAttendre("OK");
  gsmSerial.println("AT+CMGF=1");   // Mode texte SMS
  gsmAttendre("OK");
  gsmSerial.println("AT+CSCS=\"GSM\""); // Encodage GSM 7-bit
  gsmAttendre("OK");

  Serial.println("[GSM] Module A6 initialise.");
  oledSerial.begin(9600);
}

void gsmEnvoyerSMS(String numero, String message) {
  Serial.println("[GSM] Envoi SMS vers " + numero);

  oledSerial.end();
  gsmSerial.begin(9600);
  gsmSerial.listen();
  while (gsmSerial.available()) gsmSerial.read();

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(numero);
  gsmSerial.println("\"");

  if (!gsmAttendre(">", 5000)) {
    Serial.println("[GSM] Pas de prompt > !");
    oledSerial.begin(9600);
    return;
  }

  gsmSerial.print(message);
  gsmSerial.write(26); // Ctrl+Z

  if (gsmAttendre("+CMGS", 15000)) {
    Serial.println("[GSM] SMS envoye avec succes.");
  } else {
    Serial.println("[GSM] Echec envoi SMS.");
  }

  oledSerial.begin(9600);
}

// ─────────────────────────────────────────────────────────
//  Traitement commandes
// ─────────────────────────────────────────────────────────
void traiterCommande(String cmd) {
  cmd.trim();

  if (cmd == "ACCES AUTORISE") {
    afficherAcces("ACCES", "AUTORISE");
    delay(3000);
    clearScreen();
    addLine("Scan card...");

  } else if (cmd == "ACCES REFUSE") {
    afficherAcces("ACCES", "REFUSE");
    delay(3000);
    clearScreen();
    addLine("Scan card...");

  } else if (cmd.startsWith("SMS:")) {
    String payload = cmd.substring(4);
    int sep = payload.indexOf('|');
    if (sep != -1) {
      String numero  = payload.substring(0, sep);
      String message = payload.substring(sep + 1);
      Serial.println("[SMS] Destinataire: " + numero);
      Serial.println("[SMS] Message: " + message);
      gsmEnvoyerSMS(numero, message);
    } else {
      Serial.println("[SMS] Format invalide");
    }
  }
}

// ─────────────────────────────────────────────────────────
//  SETUP / LOOP
// ─────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);
  oledSerial.begin(9600);

  delay(500);
  sendToNextion("rest");
  delay(1000);
  clearScreen();
  addLine("System init...");

  nfc.begin();
  addLine("NFC ready");

  gsmInit();

  addLine("Scan card...");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    traiterCommande(cmd);
  }

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

    traiterCommande(reponse);
    delay(500);
  }

  delay(500);
}
