# Notes de session — Projet RC BTS CIEL ER

## Architecture du projet

- **Raspberry Pi** : serveur LAMP uniquement (Apache + MariaDB + PHP)
  - IP Tailscale : `100.68.68.68`
  - Page web : `http://100.68.68.68/check_uid.php`
  - Login phpMyAdmin : `root` / `rc123`

- **PC Windows** : fait tourner `LAMP/UID_Check.py`
  - Arduino branché en USB sur `COM5`

- **Arduino Uno** : gère le hardware
  - PN532 RFID en I2C
  - Module A6 GSM en SoftwareSerial (pin 4=TX, pin 5=RX)
  - Écran Nextion en SoftwareSerial (pin 2=TX, pin 3=RX)
  - Sketch : `Arduino Uno Code/NFC_I2C-OLED_Mode/NFC_I2C-OLED_Mode.ino`

## Flux de fonctionnement

1. Carte RFID scannée → Arduino envoie `UID:xxxx` au Python via USB
2. Python vérifie l'UID auprès du PHP (`?api=1&uid=xxxx`)
3. PHP retourne JSON `{statut, nom, prenom, telephone}`
4. Si OK : Python envoie `ACCES AUTORISE` à l'Arduino → Nextion affiche
5. Python récupère stats session (`?api=session&uid=xxxx`)
6. Python envoie `SMS:0780750719|Bienvenue...` à l'Arduino
7. Arduino transmet au module A6 → SMS envoyé

## Numéros
- **Numéro A6 GSM** (expéditeur SMS) : `0745461370`
- **Numéro Youssouf** (destinataire SMS) : `0780750719`
- **UID carte Youssouf** : `96201305`

## BDD MySQL — table utilisateurs
- Colonnes : `id, uid, nom, prenom, telephone`
- UID `96201305` → Youssouf AitAmir → `0780750719`

## BDD MySQL — table performances
- Colonnes : `id, vitesse_kmh, tours_par_sec, date_mesure`
- ⚠️ Ancienne table avait des mauvaises colonnes — recréée manuellement

## BDD MySQL — table logs_acces
- Colonnes : `id, uid, nom, prenom, acces, date_acces`

## Corrections apportées

### UID_Check.py
- Détection automatique port COM (plus besoin de changer `/dev/ttyUSB0`)
- `decode("utf-8", errors="ignore")` pour les bytes parasites Arduino
- `response = None` avant le try pour éviter `NameError`

### check_uid.php
- Credentials : `localhost / root / rc123`
- Retourne JSON avec `telephone` pour le SMS
- API `?api=session` pour distance/vitesse max

### NFC_I2C-OLED_Mode.ino
- `gsmInit()` : ajout `oledSerial.end()` avant init A6
- Corrige conflit SoftwareSerial entre Nextion et A6

## Problème en cours
- SMS commandé par Python ✅
- Arduino reçoit la commande ✅
- A6 ne répond pas au démarrage (`[GSM] Module A6 non repondant !`)
- Fix appliqué dans le .ino : libérer oledSerial avant gsmInit()
- À vérifier dans Serial Monitor après reflash
