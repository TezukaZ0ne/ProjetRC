import serial
import serial.tools.list_ports
import requests
import time
import unicodedata

# ── Configuration ─────────────────────────────────────────
BAUDRATE                 = 9600
SERVEUR_PHP              = "http://100.68.68.68/check_uid.php"
URL_PORTAIL              = "http://100.68.68.68/check_uid.php"
NUMERO_DEFAUT            = "0745461370"
INTERVALLE_ENVOI_VITESSE = 2.0

# ── Détection automatique du port Arduino ─────────────────
def trouver_port_arduino():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if any(x in p.description.lower() for x in ["arduino", "ch340", "ch341", "cp210", "usb serial", "uart"]):
            print(f"[PORT] Arduino détecté sur {p.device} ({p.description})")
            return p.device
    # Fallback : premier port USB disponible
    if ports:
        print(f"[PORT] Aucun Arduino détecté, utilisation de {ports[0].device}")
        return ports[0].device
    raise Exception("Aucun port série trouvé. Vérifiez le branchement USB de l'Arduino.")

PORT_COM = trouver_port_arduino()

# ── Connexion Arduino ─────────────────────────────────────
arduino = serial.Serial(PORT_COM, BAUDRATE, timeout=1)
time.sleep(2)
print("En attente de données Arduino...")

dernier_envoi_vitesse = 0.0


# ── Helpers ───────────────────────────────────────────────

def supprimer_accents(texte):
    nfkd = unicodedata.normalize('NFKD', texte)
    return "".join(c for c in nfkd if not unicodedata.combining(c))


def calculer_distance_km(uid):
    try:
        r = requests.get(f"{SERVEUR_PHP}?api=session&uid={uid}", timeout=5)
        data = r.json()
        return data.get("distance_km"), data.get("vitesse_max_kmh")
    except Exception as e:
        print(f"[DIST] Erreur récupération session : {e}")
        return None, None


def envoyer_sms_via_arduino(numero, message):
    message_ascii = supprimer_accents(message)
    commande = f"SMS:{numero}|{message_ascii}\n"
    arduino.write(commande.encode("utf-8"))
    print(f"[SMS] Commande envoyée → {numero}")
    print(f"[SMS] Message : {message_ascii}")


def construire_sms(prenom, uid, distance_km, vitesse_kmh):
    lien = f"{URL_PORTAIL}?uid={uid}"
    if distance_km is not None and vitesse_kmh is not None:
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


# ── Boucle principale ─────────────────────────────────────

while True:
    try:
        line = arduino.readline().decode("utf-8", errors="ignore").strip()

        # ── Bloc RFID ─────────────────────────────────────
        if line.startswith("UID:"):
            uid = line.replace("UID:", "").strip()
            print(f"[RFID] UID reçu : {uid}")

            response = None
            try:
                response = requests.get(f"{SERVEUR_PHP}?uid={uid}&api=1", timeout=5)
                data     = response.json()

                statut = data.get("statut", "KO")
                nom    = data.get("nom", "Inconnu")
                prenom = data.get("prenom", "Inconnu")
                numero = data.get("telephone", NUMERO_DEFAUT)

                print(f"[RFID] Statut : {statut} | {prenom} {nom} | Tél : {numero}")

                if statut == "OK":
                    arduino.write("ACCES AUTORISE\n".encode("utf-8"))
                    print("→ Accès autorisé envoyé à l'Arduino")

                    distance_km, vitesse_max = calculer_distance_km(uid)
                    print(f"[SESSION] Distance : {distance_km} km | Vitesse max : {vitesse_max} km/h")

                    sms = construire_sms(prenom, uid, distance_km, vitesse_max)
                    envoyer_sms_via_arduino(numero, sms)

                else:
                    arduino.write("ACCES REFUSE\n".encode("utf-8"))
                    print("→ Accès refusé envoyé à l'Arduino")

            except requests.exceptions.RequestException as e:
                print(f"[RFID] Erreur réseau : {e}")
                arduino.write("ACCES REFUSE\n".encode("utf-8"))
            except ValueError as e:
                brute = response.text if response is not None else "non disponible"
                print(f"[RFID] Erreur JSON : {e} — réponse brute : {brute}")
                arduino.write("ACCES REFUSE\n".encode("utf-8"))

        # ── Bloc Vitesse ──────────────────────────────────
        elif line.startswith("VITESSE:"):
            maintenant = time.time()
            if maintenant - dernier_envoi_vitesse >= INTERVALLE_ENVOI_VITESSE:
                dernier_envoi_vitesse = maintenant
                data    = line.replace("VITESSE:", "").strip()
                parties = data.split(";")

                if len(parties) == 2:
                    try:
                        kmh  = float(parties[0])
                        trps = float(parties[1])
                        print(f"[VITESSE] {kmh:.1f} km/h  |  {trps:.3f} tr/s")
                        r = requests.get(f"{SERVEUR_PHP}?api=vitesse&kmh={kmh}&trps={trps}", timeout=5)
                        print(f"[VITESSE] Serveur → {r.text.strip()}")
                    except ValueError:
                        print(f"[VITESSE] Données invalides : {data}")
                else:
                    print(f"[VITESSE] Format inattendu : {line}")

    except Exception as e:
        print(f"[ERREUR] Lecture série : {e}")
        time.sleep(1)
