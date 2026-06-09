<?php

// ═══════════════════════════════════════════════════════
//  Helpers BDD
// ═══════════════════════════════════════════════════════
function getConn() {
    $conn = new mysqli("localhost", "root", "rc123", "projet_rc");
    if ($conn->connect_error) die("Erreur BDD");
    return $conn;
}

// ═══════════════════════════════════════════════════════
//  API : vérification RFID  (?api=1&uid=...)
// ═══════════════════════════════════════════════════════
if (isset($_GET['api']) && $_GET['api'] === '1') {
    $conn = getConn();
    $uid  = strtoupper(trim($_GET['uid']));
    $stmt = $conn->prepare("SELECT nom, prenom FROM utilisateurs WHERE uid = ?");
    $stmt->bind_param("s", $uid);
    $stmt->execute();
    $result = $stmt->get_result();
    if ($result->num_rows > 0) {
        $row = $result->fetch_assoc();
        $log = $conn->prepare("INSERT INTO logs_acces (uid, nom, prenom, acces, date_acces) VALUES (?, ?, ?, 1, NOW())");
        $log->bind_param("sss", $uid, $row['nom'], $row['prenom']);
        $log->execute();
        echo "OK";
    } else {
        $log = $conn->prepare("INSERT INTO logs_acces (uid, nom, prenom, acces, date_acces) VALUES (?, 'Inconnu', 'Inconnu', 0, NOW())");
        $log->bind_param("s", $uid);
        $log->execute();
        echo "KO";
    }
    $conn->close();
    exit;
}

// ═══════════════════════════════════════════════════════
//  API : enregistrement vitesse  (?api=vitesse&kmh=&trps=)
// ═══════════════════════════════════════════════════════
if (isset($_GET['api']) && $_GET['api'] === 'vitesse') {
    $kmh  = floatval($_GET['kmh']  ?? 0);
    $trps = floatval($_GET['trps'] ?? 0);
    $conn = getConn();
    $stmt = $conn->prepare("INSERT INTO performances (vitesse_kmh, tours_par_sec, date_mesure) VALUES (?, ?, NOW())");
    $stmt->bind_param("dd", $kmh, $trps);
    if ($stmt->execute()) {
        echo "OK";
    } else {
        echo "KO";
    }
    $conn->close();
    exit;
}

// ═══════════════════════════════════════════════════════
//  Page HTML — lecture des données pour affichage
// ═══════════════════════════════════════════════════════
$conn = getConn();

// Dernière entrée RFID
$acces  = false;
$nom    = "";
$prenom = "";
$uid    = "";

if (!isset($_GET['uid']) || empty($_GET['uid'])) {
    $stmt = $conn->prepare("SELECT uid, nom, prenom, acces FROM logs_acces ORDER BY date_acces DESC LIMIT 1");
    $stmt->execute();
    $result = $stmt->get_result();
    if ($result->num_rows > 0) {
        $row    = $result->fetch_assoc();
        $acces  = $row['acces'] == 1;
        $nom    = $row['nom'];
        $prenom = $row['prenom'];
        $uid    = $row['uid'];
    }
} else {
    $uid  = strtoupper(trim($_GET['uid']));
    $stmt = $conn->prepare("SELECT nom, prenom FROM utilisateurs WHERE uid = ?");
    $stmt->bind_param("s", $uid);
    $stmt->execute();
    $result = $stmt->get_result();
    if ($result->num_rows > 0) {
        $row    = $result->fetch_assoc();
        $acces  = true;
        $nom    = $row['nom'];
        $prenom = $row['prenom'];
    }
}

// Dernière mesure vitesse
$vitesse_kmh  = null;
$tours_par_sec = null;
$date_vitesse  = null;
$stmt2 = $conn->prepare("SELECT vitesse_kmh, tours_par_sec, date_mesure FROM performances ORDER BY date_mesure DESC LIMIT 1");
$stmt2->execute();
$res2 = $stmt2->get_result();
if ($res2->num_rows > 0) {
    $row2          = $res2->fetch_assoc();
    $vitesse_kmh   = $row2['vitesse_kmh'];
    $tours_par_sec = $row2['tours_par_sec'];
    $date_vitesse  = $row2['date_mesure'];
}

$conn->close();
?>
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="5">
    <title>Banc RC — Accès & Performances</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 32px;
            font-family: 'Segoe UI', Arial, sans-serif;
            background:
                linear-gradient(rgba(0,0,0,0.55), rgba(0,0,0,0.55)),
                url('bancEssais.JPEG') center center / cover no-repeat fixed;
        }

        /* ── Carte commune ── */
        .card {
            background: rgba(255,255,255,0.12);
            backdrop-filter: blur(14px);
            -webkit-backdrop-filter: blur(14px);
            border: 1px solid rgba(255,255,255,0.25);
            border-radius: 20px;
            padding: 40px 55px;
            text-align: center;
            min-width: 320px;
            box-shadow: 0 8px 40px rgba(0,0,0,0.5);
        }

        /* ── Section RFID ── */
        .icon  { font-size: 64px; margin-bottom: 16px; }
        .status { font-size: 32px; font-weight: 700; letter-spacing: 2px; text-transform: uppercase; margin-bottom: 10px; }
        .status.autorise { color: #4ade80; text-shadow: 0 0 20px rgba(74,222,128,0.6); }
        .status.refuse   { color: #f87171; text-shadow: 0 0 20px rgba(248,113,113,0.6); }
        .nom  { font-size: 18px; color: rgba(255,255,255,0.9); margin-bottom: 8px; font-weight: 500; }
        .uid-affiche { font-size: 12px; color: rgba(255,255,255,0.45); letter-spacing: 1px; font-family: monospace; }
        .separateur { width: 60px; height: 3px; border-radius: 2px; margin: 16px auto; }
        .separateur.autorise { background: #4ade80; }
        .separateur.refuse   { background: #f87171; }

        /* ── Section Vitesse ── */
        .vitesse-titre {
            font-size: 13px;
            letter-spacing: 3px;
            text-transform: uppercase;
            color: rgba(255,255,255,0.55);
            margin-bottom: 18px;
        }
        .vitesse-valeurs {
            display: flex;
            gap: 40px;
            justify-content: center;
            align-items: flex-end;
        }
        .mesure { display: flex; flex-direction: column; align-items: center; }
        .mesure .valeur {
            font-size: 52px;
            font-weight: 700;
            color: #38bdf8;
            text-shadow: 0 0 24px rgba(56,189,248,0.55);
            line-height: 1;
        }
        .mesure .unite {
            font-size: 14px;
            color: rgba(255,255,255,0.5);
            margin-top: 4px;
            letter-spacing: 1px;
        }
        .separateur-v { width: 1px; height: 50px; background: rgba(255,255,255,0.2); align-self: center; }
        .date-vitesse {
            margin-top: 16px;
            font-size: 11px;
            color: rgba(255,255,255,0.3);
            letter-spacing: 1px;
        }
        .no-data { color: rgba(255,255,255,0.35); font-size: 14px; margin-top: 8px; }

        /* ── Déco ── */
        .titre-projet { position: fixed; top: 24px; left: 0; right: 0; text-align: center; color: rgba(255,255,255,0.6); font-size: 12px; letter-spacing: 3px; text-transform: uppercase; }
        .refresh-info { position: fixed; bottom: 20px; right: 24px; color: rgba(255,255,255,0.3); font-size: 11px; letter-spacing: 1px; }
    </style>
</head>
<body>

    <div class="titre-projet">Banc d'essai voiture RC — BTS CIEL ER</div>

    <!-- ── Carte RFID ───────────────────────────────── -->
    <div class="card">
        <?php if ($acces): ?>
            <div class="icon">✅</div>
            <div class="status autorise">Accès autorisé</div>
            <div class="separateur autorise"></div>
            <div class="nom"><?= htmlspecialchars($prenom . " " . $nom) ?></div>
            <div class="uid-affiche">UID : <?= htmlspecialchars($uid) ?></div>
        <?php else: ?>
            <div class="icon">🚫</div>
            <div class="status refuse">Accès refusé</div>
            <div class="separateur refuse"></div>
            <div class="nom">Carte non reconnue</div>
            <?php if (!empty($uid)): ?>
                <div class="uid-affiche">UID : <?= htmlspecialchars($uid) ?></div>
            <?php endif; ?>
        <?php endif; ?>
    </div>

    <!-- ── Carte Vitesse ────────────────────────────── -->
    <div class="card">
        <div class="vitesse-titre">🏎️ Performances — Vitesse</div>
        <?php if ($vitesse_kmh !== null): ?>
            <div class="vitesse-valeurs">
                <div class="mesure">
                    <span class="valeur"><?= number_format($vitesse_kmh, 1) ?></span>
                    <span class="unite">km/h</span>
                </div>
                <div class="separateur-v"></div>
                <div class="mesure">
                    <span class="valeur"><?= number_format($tours_par_sec, 2) ?></span>
                    <span class="unite">tr/s</span>
                </div>
            </div>
            <div class="date-vitesse">Dernière mesure : <?= htmlspecialchars($date_vitesse) ?></div>
        <?php else: ?>
            <div class="no-data">Aucune donnée de vitesse enregistrée</div>
        <?php endif; ?>
    </div>

    <div class="refresh-info">Actualisation toutes les 5 secondes</div>

</body>
</html>
