<?php require 'lib.php'; require 'guard.php';
if ($_SERVER['REQUEST_METHOD'] === 'POST') { check_csrf(); $_SESSION['cart'] = []; echo '<html><head><title>Order placed</title></head><body><h1>Thank you!</h1><p>Your order #1002 is confirmed.</p></body></html>'; exit; }
?><html><head><title>Checkout</title></head><body><h1>Checkout</h1>
<form method="post"><?= csrf_field() ?><label>Delivery address <input name="address" required></label>
<label>Card number <input name="card_number" required></label><button>Place order</button></form></body></html>
