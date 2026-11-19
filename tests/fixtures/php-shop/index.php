<?php require 'lib.php'; ?>
<html><head><title>Bluewhite shop</title></head><body><h1>Shop</h1>
<form action="/shop/search.php" method="get"><input type="search" name="q" placeholder="Search products"><button>Search</button></form>
<?php foreach ($PRODUCTS as $id => $p): ?>
<h2><?= $p[0] ?> - <?= $p[1] ?> EUR</h2>
<form method="post" action="/shop/cart.php"><?= csrf_field() ?><input type="hidden" name="product" value="<?= $id ?>">
<label>Quantity <select name="qty"><option>1</option><option>2</option><option>3</option></select></label>
<label><input type="checkbox" name="giftwrap" value="yes"> Gift wrap</label>
<button type="submit" name="do" value="add">Add to basket</button></form>
<?php endforeach; ?>
<p><a href="/shop/cart.php">Basket</a> | <a href="/shop/login.php">Log in</a> | <a href="/shop/contact.php">Contact</a> | <a href="/shop/checkout.php">Checkout</a></p>
</body></html>
