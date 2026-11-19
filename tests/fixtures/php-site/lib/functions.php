<?php
function require_admin() {
    if (empty($_SESSION['admin'])) { http_response_code(403); die('Forbidden'); }
}
function must_login() { require_admin(); }   // wrapper of a guard is a guard
