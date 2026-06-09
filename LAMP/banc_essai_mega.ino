#include <Wire.h>

#define LCD_ADDR 0x3E

// ── Broches ───────────────────────────────────────────────
const int PIN_COURANT = A8;
const int PIN_TENSION = A15;
const int PIN_VITESSE = A11;

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
float tours_par_sec             = 0.0;   // ← exposé globalement

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

  // tours par seconde ─ exposé globalement pour le Serial
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

// ─────────────────────────────────────────────────────────

void setup() {
  Wire.begin();
  lcdInit();

  pinMode(PIN_VITESSE, INPUT);

  lcdCursor(0, 0);
  lcdTexte("  Voltmetre /");
  lcdCursor(0, 1);
  lcdTexte("  Amperemetre");
  delay(1500);
  lcdEffacer();

  dernierTemps      = millis();
  dernierChangement = millis();
  Serial.begin(9600);
}

void loop() {
  unsigned long maintenant = millis();

  lireImpulsions();

  if (maintenant - dernierChangement >= INTERVALLE_ECRAN) {
    dernierChangement = maintenant;
    ecran1 = !ecran1;
    lcdEffacer();
  }

  if (maintenant - dernierAffichage >= INTERVALLE) {
    dernierAffichage = maintenant;

    float tension = lireTension();
    float courant = lireCourant();
    float vitesse = calculerVitesse();   // met aussi à jour tours_par_sec

    if (ecran1) {
      afficherEcranVC(tension, courant);
    } else {
      afficherEcranVitesse(vitesse);
    }

    // ── Ligne 1 : tension / courant (débogage) ────────────
    Serial.print("V: ");    Serial.print(tension, 2);
    Serial.print(" V   C: "); Serial.print(courant, 2);
    Serial.println(" A");

    // ── Ligne 2 : vitesse parseable par le Python ─────────
    // Format fixe : VITESSE:<km/h>;<tr/s>
    Serial.print("VITESSE:");
    Serial.print(vitesse, 1);
    Serial.print(";");
    Serial.println(tours_par_sec, 3);
  }
}
