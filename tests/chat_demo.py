#!/usr/bin/env python3
"""ai-chat/1 walkthrough of the PHP shop fixture (served at /shop/), as a browser plugin or agent would do it.
Usage: python3 tests/chat_demo.py http://localhost   (visit /shop/, /shop/contact.php, /shop/login.php once first)"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from chat_client import Chat
c = Chat(sys.argv[1] if len(sys.argv) > 1 else "http://localhost")
print("open /shop/");                    c.start("/shop/")
print("say: do you ship to Canada?");    c.say("do you ship to Canada?")
print("do: add 2 whale widgets");        c.do(form=2, fields={"Quantity": "2"})
print("go: checkout");                   c.go("/shop/checkout.php")
print("do: log in");                     c.do(form=1, fields={"Username": "reza", "Password": "s3cret"})
print("go: checkout");                   c.go("/shop/checkout.php")
print("do: place order (no confirm)");   c.do(form=1, fields={"address": "1 Rue de la Loge", "card_number": "4242424242424242"})
print("do: place order (confirmed)");    c.do(form=1, confirm=True, fields={"address": "1 Rue de la Loge", "card_number": "4242424242424242"})
