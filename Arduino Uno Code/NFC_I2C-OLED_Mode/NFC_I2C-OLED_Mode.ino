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

// ── Module GSM A6 : TX=4, RX=5 ───────────────────────────
// Broche 4 Arduino → RX du A6
// Broche 5 Arduino ← TX du A6
#define GSM_TX 4
#define GSM_RX 5
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ── Nextion : buffer écran ────────────────────────────────
#define MAX_LINES 6
String screenBuffer[MAX_LINES];

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

// Attend une réponse contenant 'expected' dans le délai imparti
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

void gsmInit() {
  // Note : SoftwareSerial ne peut gérer qu'un seul port à la fois
  // On libère le Nextion avant d'initialiser le GSM
  oledSerial.end();
  gsmSerial.begin(9600);
  delay(2000);

  gsmSerial.println("AT");
  if (!gsmAttendre("OK", 3000)) {
    Serial.println("[GSM] Module A6 non repondant !");
    oledSerial.begin(9600); // restitue Nextion
    return;
  }
  gsmSerial.println("ATE0");       // Désactiver écho
  gsmAttendre("OK");
  gsmSerial.println("AT+CMGF=1"); // Mode texte SMS
  gsmAttendre("OK");
  gsmSerial.println("AT+CSCS=\"GSM\""); // Encodage GSM 7-bit
  gsmAttendre("OK");

  Serial.println("[GSM] Module A6 initialise.");
  oledSerial.begin(9600); // restitue Nextion
}

void gsmEnvoyerSMS(String numero, String message) {
  Serial.println("[GSM] Envoi SMS vers " + numero);

  // SoftwareSerial : on doit écouter gsmSerial
  oledSerial.end();   // libère les pins SoftwareSerial Nextion
  gsmSerial.listen(); // écoute le A6

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(numero);
  gsmSerial.println("\"");

  if (!gsmAttendre(">", 5000)) {
    Serial.println("[GSM] Pas de prompt > !");
    oledSerial.begin(9600); // restitue Nextion
    return;
  }

  gsmSerial.print(message);
  gsmSerial.write(26); // Ctrl+Z

  if (gsmAttendre("+CMGS", 15000)) {
    Serial.println("[GSM] SMS envoye avec succes.");
  } else {
    Serial.println("[GSM] Echec envoi SMS.");
  }

  // Restitue le port Nextion
  oledSerial.begin(9600);
}

// ─────────────────────────────────────────────────────────
//  Traitement commandes reçues depuis Raspberry Pi
//  via Serial (USB) :
//    ACCES AUTORISE
//    ACCES REFUSE
//    SMS:<numero>|<message>
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
  Serial.begin(9600);     // Communication Raspberry Pi (USB)
  oledSerial.begin(9600); // Nextion

  delay(500);
  sendToNextion("rest");
  delay(1000);
  clearScreen();
  addLine("System init...");

  nfc.begin();
  addLine("NFC ready");

  gsmInit();              // Module A6 GSM

  addLine("Scan card...");
}

void loop() {

  // ── Lecture commande Raspberry Pi ─────────────────────
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    traiterCommande(cmd);
  }

  // ── Lecture carte NFC ─────────────────────────────────
  if (nfc.tagPresent()) {
    NfcTag tag = nfc.read();
    String uid = tag.getUidString();
    uid.replace(" ", "");
    uid.toUpperCase();

    addLine("Card detected");
    addLine("UID:" + uid);

    // Envoie l'UID au Raspberry Pi
    Serial.println("UID:" + uid);

    // Attente réponse Raspberry Pi (5s max)
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
