import serial
import requests
import time

# --- Configuration ---
PORT_COM    = "COM5"
BAUDRATE    = 115200
SERVEUR_PHP = "http://100.68.68.68/check_uid.php"

# --- Connexion Arduino ---
arduino = serial.Serial(PORT_COM, BAUDRATE, timeout=1)
time.sleep(2)

print("En attente de scan RFID...")

while True:
    try:
        line = arduino.readline().decode("utf-8").strip()

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
                print(f"Erreur réseau : {e}")
                arduino.write("ACCES REFUSE\n".encode("utf-8"))

    except Exception as e:
        print(f"Erreur lecture série : {e}")
        time.sleep(1)
