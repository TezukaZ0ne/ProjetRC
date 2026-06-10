import time
import unicodedata
import requests
import board
import busio
from adafruit_pn532.i2c import PN532_I2C
import serial

# ── Configuration ─────────────────────────────────────────
SERVEUR_PHP              = "http://100.68.68.68/check_uid.php"
URL_PORTAIL              = "http://100.68.68.68/check_uid.php"
NUMERO_DEFAUT            = "0745461370"
INTERVALLE_ENVOI_VITESSE = 2.0

PORT_A6  = "/dev/ttyUSB0"   # Port série direct du module A6 GSM
BAUD_A6  = 9600

# ── Connexion PN532 (I2C direct) ──────────────────────────
i2c = busio.I2C(board.SCL, board.SDA)
pn532 = PN532_I2C(i2c, debug=False)
pn532.SAM_configuration()
print("[NFC] PN532 initialisé en I2C.")

# ── Connexion Module A6 GSM (Serial direct) ───────────────
gsm = serial.Serial(PORT_A6, BAUD_A6, timeout=1)
time.sleep(2)

# ── Init A6 ───────────────────────────────────────────────
def gsm_cmd(cmd, attente="OK", timeout=5):
    gsm.write((cmd + "\r\n").encode())
    rep = ""
    debut = time.time()
    while time.time() - debut < timeout:
        if gsm.in_waiting:
            rep += gsm.read(gsm.in_waiting).decode(errors="ignore")
        if attente in rep:
            return True, rep
        time.sleep(0.1)
    return False, rep

def gsm_init():
    print("[GSM] Initialisation module A6...")
    ok, _ = gsm_cmd("AT")
    if not ok:
        print("[GSM] Module A6 non répondant !")
        return False
    gsm_cmd("ATE0")
    gsm_cmd("AT+CMGF=1")
    gsm_cmd('AT+CSCS="GSM"')
    print("[GSM] Module A6 prêt.")
    return True

# ── Helpers ───────────────────────────────────────────────
def supprimer_accents(texte):
    nfkd = unicodedata.normalize('NFKD', texte)
    return "".join(c for c in nfkd if not unicodedata.combining(c))

def envoyer_sms(numero, message):
    message = supprimer_accents(message)
    print(f"[SMS] Envoi vers {numero}...")
    gsm.write(f'AT+CMGS="{numero}"\r\n'.encode())
    time.sleep(1)
    rep = gsm.read(gsm.in_waiting).decode(errors="ignore")
    if ">" not in rep:
        print(f"[SMS] Pas de prompt '>' : {rep}")
        return
    gsm.write((message + chr(26)).encode())
    ok, rep = gsm_cmd("", attente="+CMGS", timeout=15)
    if ok:
        print("[SMS] SMS envoyé avec succès.")
    else:
        print(f"[SMS] Échec envoi. Réponse : {rep}")

def calculer_distance_km(uid):
    try:
        r = requests.get(f"{SERVEUR_PHP}?api=session&uid={uid}", timeout=5)
        data = r.json()
        return data.get("distance_km"), data.get("vitesse_max_kmh")
    except Exception as e:
        print(f"[DIST] Erreur : {e}")
        return None, None

def construire_sms(prenom, uid, distance_km, vitesse_kmh):
    lien = f"{URL_PORTAIL}?uid={uid}"
    if distance_km is not None:
        return (
            f"Bienvenue {prenom}, ({uid}), "
            f"a la derniere course, vous avez parcouru {distance_km:.2f} km, "
            f"a une vitesse de {vitesse_kmh:.1f} km/h... "
            f"cliquez ici pour en savoir plus : {lien}"
        )
    return (
        f"Bienvenue {prenom}, ({uid}), "
        f"aucune course precedente trouvee. "
        f"Connectez-vous sur le portail : {lien}"
    )

# ── Démarrage ─────────────────────────────────────────────
gsm_init()
print("[NFC] En attente d'une carte RFID...")

dernier_envoi_vitesse = 0.0

# ── Boucle principale ─────────────────────────────────────
while True:
    try:
        # ── Lecture RFID directe ──────────────────────────
        uid_bytes = pn532.read_passive_target(timeout=0.5)
        if uid_bytes is not None:
            uid = "".join(f"{b:02X}" for b in uid_bytes)
            print(f"[RFID] UID détecté : {uid}")

            try:
                r = requests.get(f"{SERVEUR_PHP}?uid={uid}&api=1", timeout=5)
                data = r.json()
                statut = data.get("statut", "KO")
                prenom = data.get("prenom", "Inconnu")
                nom    = data.get("nom", "Inconnu")
                numero = data.get("telephone", NUMERO_DEFAUT)

                print(f"[RFID] {statut} | {prenom} {nom} | Tél : {numero}")

                if statut == "OK":
                    print("→ Accès autorisé")
                    distance_km, vitesse_max = calculer_distance_km(uid)
                    sms = construire_sms(prenom, uid, distance_km, vitesse_max)
                    envoyer_sms(numero, sms)
                else:
                    print("→ Accès refusé")

            except requests.exceptions.RequestException as e:
                print(f"[RFID] Erreur réseau : {e}")
            except ValueError as e:
                print(f"[RFID] Erreur JSON : {e}")

            time.sleep(2)  # Anti-rebond

    except Exception as e:
        print(f"[ERREUR] : {e}")
        time.sleep(1)
