<?php require_once __DIR__ . '/includes/auth.php'; ?>
<html><head><title>My account</title></head><body><h1>My orders</h1><p>Secret order list for <?= htmlspecialchars($_SESSION['user']) ?></p></body></html>
