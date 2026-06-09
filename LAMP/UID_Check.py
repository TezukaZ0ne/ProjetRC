import serial
import requests
import time

# ── Configuration ─────────────────────────────────────────
PORT_COM    = "COM5"          # adapter si Linux : "/dev/ttyUSB0"
BAUDRATE    = 9600
SERVEUR_PHP = "http://100.68.68.68/check_uid.php"

# Intervalle minimum entre deux envois de vitesse (secondes)
# évite de saturer la BDD si l'Arduino envoie toutes les 500 ms
INTERVALLE_ENVOI_VITESSE = 2.0

# ── Connexion Arduino ─────────────────────────────────────
arduino = serial.Serial(PORT_COM, BAUDRATE, timeout=1)
time.sleep(2)

print("En attente de données Arduino...")

dernier_envoi_vitesse = 0.0   # timestamp du dernier POST vitesse

while True:
    try:
        line = arduino.readline().decode("utf-8").strip()

        # ── Bloc RFID ─────────────────────────────────────
        if line.startswith("UID:"):
            uid = line.replace("UID:", "").strip()
            print(f"UID reçu : {uid}")

            try:
                url = f"{SERVEUR_PHP}?uid={uid}&api=1"
                response = requests.get(url, timeout=5)
                reponse_serveur = response.text.strip()
                print(f"Réponse serveur : {reponse_serveur}")

                if reponse_serveur == "OK":
                    arduino.write("ACCES AUTORISE\n".encode("utf-8"))
                    print("→ Accès autorisé envoyé à l'Arduino")
                else:
                    arduino.write("ACCES REFUSE\n".encode("utf-8"))
                    print("→ Accès refusé envoyé à l'Arduino")

            except requests.exceptions.RequestException as e:
                print(f"Erreur réseau RFID : {e}")
                arduino.write("ACCES REFUSE\n".encode("utf-8"))

        # ── Bloc Vitesse ──────────────────────────────────
        # Format attendu : VITESSE:<kmh>;<tr/s>
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
                        print(f"Vitesse : {kmh:.1f} km/h  |  {trps:.3f} tr/s")

                        url = f"{SERVEUR_PHP}?api=vitesse&kmh={kmh}&trps={trps}"
                        r   = requests.get(url, timeout=5)
                        print(f"Vitesse envoyée → serveur : {r.text.strip()}")

                    except ValueError:
                        print(f"Données vitesse invalides : {data}")
                else:
                    print(f"Format vitesse inattendu : {line}")

    except Exception as e:
        print(f"Erreur lecture série : {e}")
        time.sleep(1)
