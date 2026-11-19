<?php require 'lib.php'; check_csrf();
if (empty($_POST['email']) || empty($_POST['message'])) { echo '<html><head><title>Contact</title></head><body><p class="error">Email and message are required.</p></body></html>'; exit; }
file_put_contents('/tmp/messages.log', $_POST['email'] . ' [' . $_POST['topic'] . '] ' . $_POST['message'] . "\n", FILE_APPEND);
echo '<html><head><title>Message sent</title></head><body><h1>Thanks!</h1><p>We received your message and will reply within 24 hours.</p></body></html>';
