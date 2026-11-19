<?php require 'lib.php';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    check_csrf();
    if (empty($_POST['product']) || !isset($PRODUCTS[$_POST['product']])) { http_response_code(400); die('Unknown product'); }
    $_SESSION['cart'][] = [$_POST['product'], (int)$_POST['qty'], isset($_POST['giftwrap'])];
    header('Location: /shop/cart.php?added=1', true, 303); exit;
}
?><html><head><title>Your basket</title></head><body><h1>Your basket</h1>
<?php if (!empty($_GET['added'])) echo '<p>Item added to your basket.</p>'; ?>
<ul><?php foreach ($_SESSION['cart'] ?? [] as $l) echo "<li>{$l[1]} x {$PRODUCTS[$l[0]][0]}" . ($l[2] ? ' (gift wrapped)' : '') . "</li>"; ?></ul>
<a href="/shop/checkout.php">Go to checkout</a></body></html>
