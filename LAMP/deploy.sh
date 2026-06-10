#!/bin/bash
# ── Script de déploiement automatique LAMP ────────────────
# À exécuter sur le Raspberry Pi avec : sudo bash deploy.sh

echo "=== Déploiement Banc RC ==="

# 1. Copie du PHP
echo "[1/4] Copie check_uid.php..."
sudo cp "$(dirname "$0")/check_uid.php" /var/www/html/check_uid.php
sudo cp "$(dirname "$0")/../imgs/bancEssais.JPEG" /var/www/html/bancEssais.JPEG 2>/dev/null || true
sudo chown www-data:www-data /var/www/html/check_uid.php
echo "      OK"

# 2. Création user MySQL dédié + BDD si nécessaire
echo "[2/4] Configuration MySQL..."
sudo mariadb << 'SQL'
CREATE DATABASE IF NOT EXISTS projet_rc CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'rc_user'@'localhost' IDENTIFIED BY 'rc123';
GRANT ALL PRIVILEGES ON projet_rc.* TO 'rc_user'@'localhost';

-- Si root n'a pas de mot de passe, on lui en met un
-- ALTER USER 'root'@'localhost' IDENTIFIED BY 'rc123';

FLUSH PRIVILEGES;
SQL
echo "      OK"

# 3. Création des tables
echo "[3/4] Création des tables..."
sudo mariadb projet_rc << 'SQL'
CREATE TABLE IF NOT EXISTS utilisateurs (
    id        INT UNSIGNED NOT NULL AUTO_INCREMENT,
    uid       VARCHAR(30)  NOT NULL UNIQUE,
    nom       VARCHAR(50)  NOT NULL,
    prenom    VARCHAR(50)  NOT NULL,
    telephone VARCHAR(15)  DEFAULT NULL,
    PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS performances (
    id            INT UNSIGNED NOT NULL AUTO_INCREMENT,
    vitesse_kmh   FLOAT        NOT NULL,
    tours_par_sec FLOAT        NOT NULL,
    date_mesure   DATETIME     NOT NULL DEFAULT NOW(),
    PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS logs_acces (
    id         INT UNSIGNED NOT NULL AUTO_INCREMENT,
    uid        VARCHAR(30)  NOT NULL,
    nom        VARCHAR(50)  NOT NULL,
    prenom     VARCHAR(50)  NOT NULL,
    acces      TINYINT(1)   NOT NULL DEFAULT 0,
    date_acces DATETIME     NOT NULL DEFAULT NOW(),
    PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE utilisateurs ADD COLUMN IF NOT EXISTS telephone VARCHAR(15) DEFAULT NULL;
SQL
echo "      OK"

# 4. Restart Apache
echo "[4/4] Restart Apache..."
sudo systemctl restart apache2
echo "      OK"

echo ""
echo "=== Déploiement terminé ! ==="
echo "Teste : curl http://localhost/check_uid.php"
