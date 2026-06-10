import serial
import requests
import time
import unicodedata

# ── Configuration ─────────────────────────────────────────
PORT_COM    = "/dev/ttyUSB0"   # Port Arduino Mega sur Raspberry Pi
BAUDRATE    = 9600
SERVEUR_PHP = "http://100.68.68.68/check_uid.php"
URL_PORTAIL = "http://100.68.68.68/check_uid.php"

# Numéro GSM de l'admin/banc (pour tester)
# Le numéro destinataire est récupéré depuis la BDD utilisateurs
NUMERO_DEFAUT = "0745461370"

# Intervalle minimum entre deux envois de vitesse (secondes)
INTERVALLE_ENVOI_VITESSE = 2.0

# ── Connexion Arduino ─────────────────────────────────────
arduino = serial.Serial(PORT_COM, BAUDRATE, timeout=1)
time.sleep(2)

print("En attente de données Arduino...")

dernier_envoi_vitesse = 0.0


# ── Helpers ───────────────────────────────────────────────

def supprimer_accents(texte):
    """Supprime les accents pour compatibilité encodage GSM 7-bit."""
    nfkd = unicodedata.normalize('NFKD', texte)
    return "".join(c for c in nfkd if not unicodedata.combining(c))


def calculer_distance_km(uid):
    """
    Calcule la distance totale de la dernière session depuis la BDD.
    Requête PHP : ?api=session&uid=<uid>
    Retourne (distance_km, vitesse_max_kmh) ou (None, None) si indisponible.
    """
    try:
        url = f"{SERVEUR_PHP}?api=session&uid={uid}"
        r = requests.get(url, timeout=5)
        data = r.json()
        distance = data.get("distance_km")
        vitesse  = data.get("vitesse_max_kmh")
        return distance, vitesse
    except Exception as e:
        print(f"[DIST] Erreur récupération session : {e}")
        return None, None


def envoyer_sms_via_arduino(numero, message):
    """
    Envoie la commande SMS à l'Arduino qui la transmet au module A6.
    Format : SMS:<numero>|<message>
    """
    message_ascii = supprimer_accents(message)
    commande = f"SMS:{numero}|{message_ascii}\n"
    arduino.write(commande.encode("utf-8"))
    print(f"[SMS] Commande envoyée à l'Arduino → {numero}")
    print(f"[SMS] Message : {message_ascii}")


def construire_sms(prenom, uid, distance_km, vitesse_kmh, numero_tel):
    """
    Construit le message SMS de bienvenue.
    Format : Bienvenue <prenom>, (<uid>), a la derniere course,
             vous avez parcouru X km, a une vitesse de Y km/h...
             cliquez ici pour en savoir plus : <lien>
    """
    lien = f"{URL_PORTAIL}?uid={uid}"

    if distance_km is not None and vitesse_kmh is not None:
        msg = (
            f"Bienvenue {prenom}, ({uid}), "
            f"a la derniere course, vous avez parcouru {distance_km:.2f} km, "
            f"a une vitesse de {vitesse_kmh:.1f} km/h... "
            f"cliquez ici pour en savoir plus : {lien}"
        )
    else:
        msg = (
            f"Bienvenue {prenom}, ({uid}), "
            f"aucune course precedente trouvee. "
            f"Connectez-vous sur le portail : {lien}"
        )
    return msg


# ── Boucle principale ─────────────────────────────────────

while True:
    try:
        line = arduino.readline().decode("utf-8").strip()

        # ── Bloc RFID ─────────────────────────────────────
        if line.startswith("UID:"):
            uid = line.replace("UID:", "").strip()
            print(f"[RFID] UID reçu : {uid}")

            try:
                # Vérification accès + récupération infos utilisateur
                url = f"{SERVEUR_PHP}?uid={uid}&api=1"
                response = requests.get(url, timeout=5)
                data = response.json()

                statut  = data.get("statut", "KO")
                nom     = data.get("nom", "Inconnu")
                prenom  = data.get("prenom", "Inconnu")
                numero  = data.get("telephone", NUMERO_DEFAUT)

                print(f"[RFID] Statut : {statut} | {prenom} {nom} | Tél : {numero}")

                if statut == "OK":
                    arduino.write("ACCES AUTORISE\n".encode("utf-8"))
                    print("→ Accès autorisé envoyé à l'Arduino")

                    # Récupération des données de la dernière session
                    distance_km, vitesse_max = calculer_distance_km(uid)
                    print(f"[SESSION] Distance : {distance_km} km | Vitesse max : {vitesse_max} km/h")

                    # Construction et envoi du SMS
                    sms = construire_sms(prenom, uid, distance_km, vitesse_max, numero)
                    envoyer_sms_via_arduino(numero, sms)

                else:
                    arduino.write("ACCES REFUSE\n".encode("utf-8"))
                    print("→ Accès refusé envoyé à l'Arduino")

            except requests.exceptions.RequestException as e:
                print(f"[RFID] Erreur réseau : {e}")
                arduino.write("ACCES REFUSE\n".encode("utf-8"))
            except ValueError as e:
                brute = response.text if "response" in locals() else "non disponible"
                print(f"[RFID] Erreur JSON : {e} — réponse brute : {brute}")
                arduino.write("ACCES REFUSE\n".encode("utf-8"))

        # ── Bloc Vitesse ──────────────────────────────────
        elif line.startswith("VITESSE:"):
            maintenant = time.time()
            if maintenant - dernier_envoi_vitesse >= INTERVALLE_ENVOI_VITESSE:
                dernier_envoi_vitesse = maintenant

                data = line.replace("VITESSE:", "").strip()
                parties = data.split(";")

                if len(parties) == 2:
                    try:
                        kmh  = float(parties[0])
                        trps = float(parties[1])
                        print(f"[VITESSE] {kmh:.1f} km/h  |  {trps:.3f} tr/s")

                        url = f"{SERVEUR_PHP}?api=vitesse&kmh={kmh}&trps={trps}"
                        r   = requests.get(url, timeout=5)
                        print(f"[VITESSE] Serveur → {r.text.strip()}")

                    except ValueError:
                        print(f"[VITESSE] Données invalides : {data}")
                else:
                    print(f"[VITESSE] Format inattendu : {line}")

    except Exception as e:
        print(f"[ERREUR] Lecture série : {e}")
        time.sleep(1)
