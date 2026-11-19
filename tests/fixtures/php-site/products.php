<?php $items = ['Whale widget' => 29, 'Dolphin gadget' => 19]; ?>
<html><head><title>Price list</title></head><body><h1>Price list</h1><ul>
<?php foreach ($items as $n => $p) echo "<li>$n: $p EUR</li>"; ?></ul><p>Don't forget: prices include VAT.</p></body></html>
