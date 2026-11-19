<?php require 'lib.php'; require 'guard.php'; ?>
<html><head><title>My account</title></head><body><h1>Welcome, <?= $_SESSION['user'] ?></h1><p>Your private order history: order #1001 (2 x Whale widget).</p>
<form method="post" action="/shop/password.php"><?= csrf_field() ?><input type="password" name="current_password"><input type="password" name="new_password"><button>Change password</button></form>
</body></html>
