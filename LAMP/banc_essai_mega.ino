#include <Wire.h>
#include <SoftwareSerial.h>

#define LCD_ADDR 0x3E

// ── Broches ───────────────────────────────────────────────
const int PIN_COURANT = A8;
const int PIN_TENSION = A15;
const int PIN_VITESSE = A11;

// ── Module GSM A6 (SoftwareSerial) ────────────────────────
// TX Arduino → RX A6 : broche 10
// RX Arduino ← TX A6 : broche 11
SoftwareSerial gsmSerial(11, 10);

// ── Calibration Phidgets 1135 ±30V ───────────────────────
const float OFFSET_TENSION      = 2.669;
const float SENSIBILITE_TENSION = 0.0681;

// ── Calibration Phidgets 1122 ±30A ───────────────────────
const float OFFSET_COURANT      = 2.5;
const float SENSIBILITE_COURANT = 0.0667;

// ── Calibration vitesse ───────────────────────────────────
const float NB_IMPULSIONS_PAR_TOUR = 86.14;
const float RAYON_ROUE_M           = 0.0535;

const float VCC                       = 5.0;
const float ADC_RES                   = 1023.0;
const unsigned long INTERVALLE        = 500;
const unsigned long INTERVALLE_ECRAN  = 3000;
unsigned long dernierAffichage        = 0;
unsigned long dernierChangement       = 0;
bool ecran1                           = true;
const int NB_LECTURES                 = 20;

// ── Variables vitesse ─────────────────────────────────────
unsigned long dernierTemps      = 0;
int dernierEtat                 = HIGH;
unsigned long nbImpulsions      = 0;
float vitesse_kmh               = 0.0;
float tours_par_sec             = 0.0;

// ── LCD ───────────────────────────────────────────────────

void lcdCommande(uint8_t cmd) {
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(0x00);
  Wire.write(cmd);
  Wire.endTransmission();
  delay(2);
}

void lcdCaractere(char c) {
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(0x40);
  Wire.write(c);
  Wire.endTransmission();
  delay(1);
}

void lcdTexte(const char* texte) {
  while (*texte) lcdCaractere(*texte++);
}

void lcdCursor(uint8_t col, uint8_t row) {
  uint8_t offsets[] = {0x00, 0x40};
  lcdCommande(0x80 | (col + offsets[row]));
}

void lcdEffacer() {
  lcdCommande(0x01);
  delay(2);
}

void lcdInit() {
  delay(50);
  lcdCommande(0x38);
  delay(2);
  lcdCommande(0x39);
  delay(2);
  lcdCommande(0x14);
  lcdCommande(0x70);
  lcdCommande(0x56);
  lcdCommande(0x6C);
  delay(300);
  lcdCommande(0x38);
  lcdCommande(0x0C);
  lcdCommande(0x01);
  delay(2);
}

// ── GSM A6 ────────────────────────────────────────────────

// Attend une réponse du A6 contenant 'expected', timeout en ms
bool gsmAttendre(const char* expected, unsigned long timeout = 5000) {
  String reponse = "";
  unsigned long debut = millis();
  while (millis() - debut < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      reponse += c;
    }
    if (reponse.indexOf(expected) != -1) return true;
  }
  Serial.print("[GSM] Réponse: ");
  Serial.println(reponse);
  return false;
}

void gsmInit() {
  gsmSerial.begin(9600);
  delay(2000);

  // Test communication
  gsmSerial.println("AT");
  if (!gsmAttendre("OK", 3000)) {
    Serial.println("[GSM] Module A6 non répondant !");
    return;
  }

  // Désactiver l'écho
  gsmSerial.println("ATE0");
  gsmAttendre("OK");

  // Mode SMS texte
  gsmSerial.println("AT+CMGF=1");
  gsmAttendre("OK");

  // Encodage GSM 7-bit (évite les problèmes accents)
  gsmSerial.println("AT+CSCS=\"GSM\"");
  gsmAttendre("OK");

  Serial.println("[GSM] Module A6 initialisé.");
}

// Envoie un SMS — numero format: "0745461370"
// message : texte brut, max ~160 chars, sans accents
void gsmEnvoyerSMS(const char* numero, const char* message) {
  Serial.print("[GSM] Envoi SMS vers ");
  Serial.println(numero);

  // Commande AT+CMGS
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(numero);
  gsmSerial.println("\"");

  // Attendre le prompt '>'
  if (!gsmAttendre(">", 5000)) {
    Serial.println("[GSM] Pas de prompt > !");
    return;
  }

  // Envoyer le message
  gsmSerial.print(message);
  // Ctrl+Z pour valider
  gsmSerial.write(26);

  // Attendre confirmation +CMGS
  if (gsmAttendre("+CMGS", 15000)) {
    Serial.println("[GSM] SMS envoyé avec succès.");
  } else {
    Serial.println("[GSM] Échec envoi SMS.");
  }
}

// ── Capteurs ──────────────────────────────────────────────

float lireTension() {
  long somme = 0;
  for (int i = 0; i < NB_LECTURES; i++) {
    somme += analogRead(PIN_TENSION);
    delay(2);
  }
  float vLue = (somme / (float)NB_LECTURES / ADC_RES) * VCC;
  float tension = (vLue - OFFSET_TENSION) / SENSIBILITE_TENSION;
  if (abs(tension) < 0.05) tension = 0.0;
  return tension;
}

float lireCourant() {
  long somme = 0;
  for (int i = 0; i < NB_LECTURES; i++) {
    somme += analogRead(PIN_COURANT);
    delay(2);
  }
  float vLue = (somme / (float)NB_LECTURES / ADC_RES) * VCC;
  float courant = (vLue - OFFSET_COURANT) / SENSIBILITE_COURANT;
  if (abs(courant) < 0.15) courant = 0.0;
  return courant;
}

void lireImpulsions() {
  int etatActuel = analogRead(PIN_VITESSE) > 512 ? HIGH : LOW;
  if (etatActuel == LOW && dernierEtat == HIGH) {
    nbImpulsions++;
  }
  dernierEtat = etatActuel;
}

float calculerVitesse() {
  unsigned long maintenant = millis();
  unsigned long duree = maintenant - dernierTemps;
  if (duree < INTERVALLE) return vitesse_kmh;

  dernierTemps = maintenant;

  tours_par_sec = (float)nbImpulsions / NB_IMPULSIONS_PAR_TOUR / (duree / 1000.0);
  vitesse_kmh   = 2.0 * PI * RAYON_ROUE_M * tours_par_sec * 3.6;

  if (vitesse_kmh  < 0.1) vitesse_kmh  = 0.0;
  if (tours_par_sec < 0.01) tours_par_sec = 0.0;

  nbImpulsions = 0;
  return vitesse_kmh;
}

// ── Affichage LCD ─────────────────────────────────────────

void afficherEcranVC(float tension, float courant) {
  char buf[10];
  lcdCursor(0, 0);
  dtostrf(tension, 7, 2, buf);
  lcdTexte("V:");
  lcdTexte(buf);
  lcdTexte(" V  ");
  lcdCursor(0, 1);
  dtostrf(courant, 7, 2, buf);
  lcdTexte("C:");
  lcdTexte(buf);
  lcdTexte(" A  ");
}

void afficherEcranVitesse(float vitesse) {
  char buf[10];
  lcdCursor(0, 0);
  lcdTexte("Vitesse:        ");
  lcdCursor(0, 1);
  dtostrf(vitesse, 8, 1, buf);
  lcdTexte(buf);
  lcdTexte(" km/h   ");
}

// ── Traitement commandes reçues du Raspberry Pi ───────────
// Format attendu depuis UID_Check.py :
//   ACCES AUTORISE
//   ACCES REFUSE
//   SMS:<numero>|<message>
void traiterCommandeSerie(String cmd) {
  cmd.trim();

  if (cmd == "ACCES AUTORISE") {
    Serial.println("[INFO] Accès autorisé");
    lcdEffacer();
    lcdCursor(0, 0); lcdTexte("  ACCES OK  ");
    lcdCursor(0, 1); lcdTexte("  Bienvenue!");
    delay(2000);
    lcdEffacer();

  } else if (cmd == "ACCES REFUSE") {
    Serial.println("[INFO] Accès refusé");
    lcdEffacer();
    lcdCursor(0, 0); lcdTexte("  ACCES KO  ");
    lcdCursor(0, 1); lcdTexte(" Carte inconnue");
    delay(2000);
    lcdEffacer();

  } else if (cmd.startsWith("SMS:")) {
    // Format : SMS:<numero>|<message>
    String payload = cmd.substring(4);
    int sep = payload.indexOf('|');
    if (sep != -1) {
      String numero  = payload.substring(0, sep);
      String message = payload.substring(sep + 1);
      Serial.print("[SMS] Destinataire: "); Serial.println(numero);
      Serial.print("[SMS] Message: "); Serial.println(message);
      gsmEnvoyerSMS(numero.c_str(), message.c_str());
    } else {
      Serial.println("[SMS] Format invalide");
    }
  }
}

// ─────────────────────────────────────────────────────────

void setup() {
  Wire.begin();
  lcdInit();
  pinMode(PIN_VITESSE, INPUT);

  lcdCursor(0, 0); lcdTexte("  Voltmetre /");
  lcdCursor(0, 1); lcdTexte("  Amperemetre");
  delay(1500);
  lcdEffacer();

  dernierTemps      = millis();
  dernierChangement = millis();

  Serial.begin(9600);   // Communication Raspberry Pi
  gsmInit();            // Initialisation module A6
}

void loop() {
  unsigned long maintenant = millis();

  lireImpulsions();

  // ── Lecture commandes depuis Raspberry Pi ─────────────
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    traiterCommandeSerie(cmd);
  }

  if (maintenant - dernierChangement >= INTERVALLE_ECRAN) {
    dernierChangement = maintenant;
    ecran1 = !ecran1;
    lcdEffacer();
  }

  if (maintenant - dernierAffichage >= INTERVALLE) {
    dernierAffichage = maintenant;

    float tension = lireTension();
    float courant = lireCourant();
    float vitesse = calculerVitesse();

    if (ecran1) {
      afficherEcranVC(tension, courant);
    } else {
      afficherEcranVitesse(vitesse);
    }

    // Débogage
    Serial.print("V: ");    Serial.print(tension, 2);
    Serial.print(" V   C: "); Serial.print(courant, 2);
    Serial.println(" A");

    // Format parseable Python
    Serial.print("VITESSE:");
    Serial.print(vitesse, 1);
    Serial.print(";");
    Serial.println(tours_par_sec, 3);
  }
}
