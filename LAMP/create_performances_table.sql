-- À exécuter sur la Raspberry Pi dans MySQL :
--   mysql -u root -p projet_rc < create_performances_table.sql

USE projet_rc;

CREATE TABLE IF NOT EXISTS performances (
    id            INT UNSIGNED   NOT NULL AUTO_INCREMENT,
    vitesse_kmh   FLOAT          NOT NULL,
    tours_par_sec FLOAT          NOT NULL,
    date_mesure   DATETIME       NOT NULL DEFAULT NOW(),
    PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
