<?php require 'lib.php'; ?>
<html><head><title>Contact</title></head><body><h1>Contact us</h1>
<form method="post" action="/shop/send.php"><?= csrf_field() ?><label>Your email <input type="email" name="email"></label>
<label>Topic <select name="topic"><option value="order">Order question</option><option value="b2b">Wholesale</option></select></label>
<label>Message <textarea name="message"></textarea></label><button>Send message</button></form></body></html>
