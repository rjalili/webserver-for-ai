<?php require 'lib.php'; $q = strtolower($_GET['q'] ?? ''); ?>
<html><head><title>Search results</title></head><body><h1>Results for "<?= htmlspecialchars($q) ?>"</h1><ul>
<?php foreach ($PRODUCTS as $id => $p) if ($q !== '' && str_contains(strtolower($p[0]), $q)) echo "<li>{$p[0]}: {$p[1]} EUR</li>"; ?></ul></body></html>
