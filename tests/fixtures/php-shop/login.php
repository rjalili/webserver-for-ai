<?php require 'lib.php';
$err = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    check_csrf();
    if ($_POST['user'] === 'reza' && $_POST['password'] === 's3cret') { $_SESSION['user'] = 'reza'; header('Location: /shop/account.php'); exit; }
    $err = '<p class="error">Wrong username or password.</p>';
}
?><html><head><title>Log in</title></head><body><h1>Log in</h1><?= $err ?>
<form method="post"><?= csrf_field() ?><label for="u">Username</label><input id="u" name="user" required>
<label for="p">Password</label><input id="p" type="password" name="password" required><button>Log in</button></form></body></html>
