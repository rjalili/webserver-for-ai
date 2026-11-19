<?php
if (!isset($_SESSION['user'])) { header('Location: /shop/login.php'); exit; }
