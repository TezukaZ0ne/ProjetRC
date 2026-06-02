<?php

if (isset($_GET['api'])) {
    $host     = "localhost";
    $user     = "root";
    $password = "rc123";
    $database = "projet_rc";
    $conn = new mysqli($host, $user, $password, $database);
    $uid = strtoupper(trim($_GET['uid']));
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

$host     = "localhost";
$user     = "root";
$password = "rc123";
$database = "projet_rc";

$conn = new mysqli($host, $user, $password, $database);

$acces = false;
$nom   = "";
$prenom = "";

if ($conn->connect_error) {
    $erreur = true;
} else if (!isset($_GET['uid']) || empty($_GET['uid'])) {
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
    $uid = strtoupper(trim($_GET['uid']));
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
    $stmt->close();
    $conn->close();
}

?>
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="5">
    <title>Contrôle d'accès - Banc RC</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            font-family: 'Segoe UI', Arial, sans-serif;
            background:
                linear-gradient(rgba(0, 0, 0, 0.55), rgba(0, 0, 0, 0.55)),
                url('bancEssais.JPEG') center center / cover no-repeat fixed;
        }
        .card {
            background: rgba(255, 255, 255, 0.12);
            backdrop-filter: blur(14px);
            -webkit-backdrop-filter: blur(14px);
            border: 1px solid rgba(255, 255, 255, 0.25);
            border-radius: 20px;
            padding: 50px 60px;
            text-align: center;
            min-width: 340px;
            box-shadow: 0 8px 40px rgba(0, 0, 0, 0.5);
        }
        .icon { font-size: 72px; margin-bottom: 20px; }
        .status { font-size: 36px; font-weight: 700; letter-spacing: 2px; text-transform: uppercase; margin-bottom: 12px; }
        .status.autorise { color: #4ade80; text-shadow: 0 0 20px rgba(74, 222, 128, 0.6); }
        .status.refuse { color: #f87171; text-shadow: 0 0 20px rgba(248, 113, 113, 0.6); }
        .nom { font-size: 20px; color: rgba(255, 255, 255, 0.9); margin-bottom: 8px; font-weight: 500; }
        .uid-affiche { font-size: 13px; color: rgba(255, 255, 255, 0.45); letter-spacing: 1px; font-family: monospace; }
        .separateur { width: 60px; height: 3px; border-radius: 2px; margin: 20px auto; }
        .separateur.autorise { background: #4ade80; }
        .separateur.refuse { background: #f87171; }
        .titre-projet { position: fixed; top: 24px; left: 0; right: 0; text-align: center; color: rgba(255, 255, 255, 0.6); font-size: 13px; letter-spacing: 3px; text-transform: uppercase; }
        .refresh-info { position: fixed; bottom: 20px; right: 24px; color: rgba(255, 255, 255, 0.3); font-size: 11px; letter-spacing: 1px; }
    </style>
</head>
<body>

    <div class="titre-projet">Banc d'essai voiture RC — BTS CIEL ER</div>

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
            <?php if (isset($uid)): ?>
                <div class="uid-affiche">UID : <?= htmlspecialchars($uid) ?></div>
            <?php endif; ?>
        <?php endif; ?>

    </div>

    <div class="refresh-info">Actualisation toutes les 5 secondes</div>

</body>
</html>
