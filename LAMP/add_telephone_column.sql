-- À exécuter sur la Raspberry Pi dans MySQL :
--   mysql -u root -p projet_rc < add_telephone_column.sql

USE projet_rc;

-- Ajoute la colonne telephone dans la table utilisateurs (si elle n'existe pas)
ALTER TABLE utilisateurs
    ADD COLUMN IF NOT EXISTS telephone VARCHAR(15) DEFAULT NULL
    COMMENT 'Numéro de téléphone pour SMS de bienvenue (format: 0XXXXXXXXX)';

-- Exemple de mise à jour d'un utilisateur :
-- UPDATE utilisateurs SET telephone = '0745461370' WHERE uid = 'AA:BB:CC:DD';
