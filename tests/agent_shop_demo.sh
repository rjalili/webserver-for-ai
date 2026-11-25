#!/bin/sh
# Walks through the PHP shop fixture (tests/fixtures/php-shop, served at /shop/) as an AI agent would:
# search, add to basket, log in, check out (with confirmation), contact form.
# Usage: tests/agent_shop_demo.sh http://localhost   (the shop must have been visited once so it is learned,
# e.g. curl -s http://localhost/shop/ http://localhost/shop/contact.php http://localhost/shop/login.php)
H=${1:-http://localhost}; P=/.well-known/ai; J=$(mktemp)
act() { echo ">>> $1"; curl -s -c $J -b $J -H 'Content-Type: application/json' -X POST $H$P/act -d "$1" |
        python3 -c "import json,sys; d=json.load(sys.stdin); print('   ', d.get('problem') or d.get('http_status'), d['message'])"; }
ids=$(curl -s $H$P/actions | python3 -c "
import json,sys
for a in json.load(sys.stdin)['actions']:
    if a['url'].startswith('/shop'): print(a['kind'].replace(' ','_'), a['id'], a['name'].replace(' ','_'))")
id() { echo "$ids" | awk -v k="$1" -v n="$2" '$1==k && $3 ~ n {print $2; exit}'; }
act "{\"action\":\"$(id search)\",\"fields\":{\"q\":\"whale\"}}"
act "{\"action\":\"$(id add_to_basket Whale)\",\"fields\":{\"Quantity\":\"2\",\"giftwrap\":true}}"
act "{\"action\":\"$(id log_in)\",\"fields\":{\"Username\":\"reza\",\"Password\":\"s3cret\"}}"
act '{"page":"/shop/checkout.php","form":1,"fields":{"address":"1 Rue de la Loge","card_number":"4242424242424242"}}'
act '{"page":"/shop/checkout.php","form":1,"confirm":true,"fields":{"address":"1 Rue de la Loge","card_number":"4242424242424242"}}'
act "{\"action\":\"$(id send_a_message)\",\"fields\":{\"email\":\"agent@example.com\",\"topic\":\"Wholesale\",\"message\":\"Hello\"}}"
rm -f $J
