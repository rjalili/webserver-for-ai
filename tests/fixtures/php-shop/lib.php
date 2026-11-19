<?php
session_start();
if (empty($_SESSION['csrf'])) $_SESSION['csrf'] = bin2hex(random_bytes(16));
function csrf_field() { return '<input type="hidden" name="csrf" value="' . $_SESSION['csrf'] . '">'; }
function check_csrf() { if (!hash_equals($_SESSION['csrf'], $_POST['csrf'] ?? '')) { http_response_code(403); die('<h1>Security check failed</h1>'); } }
$PRODUCTS = ['whale' => ['Whale widget', 29], 'dolphin' => ['Dolphin gadget', 19]];
