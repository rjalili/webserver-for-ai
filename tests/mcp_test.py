#!/usr/bin/env python3
"""Drive the site's MCP endpoint with the official MCP Python SDK, the way Claude or ChatGPT would.
Usage: python3 tests/mcp_test.py http://localhost   (pip install mcp; the shop fixture served at /shop/)"""
import asyncio, json, sys
from mcp import ClientSession
try:                                    # MCP Python SDK >= 2
    from mcp.client.streamable_http import streamable_http_client as _client
except ImportError:                     # older SDKs
    from mcp.client.streamable_http import streamablehttp_client as _client

URL = (sys.argv[1] if len(sys.argv) > 1 else "http://localhost").rstrip("/") + "/.well-known/ai/mcp"

def attr(o, *names):                     # SDK v1 uses camelCase, v2 snake_case
    for n in names:
        if hasattr(o, n): return getattr(o, n)

def first_line(res):
    t = res.content[0].text
    return ("ERROR " if attr(res, "is_error", "isError") else "") + t.split("\n\n")[0][:150]

async def main():
    async with _client(URL) as streams:
        async with ClientSession(streams[0], streams[1]) as s:
            info = await s.initialize()
            print("server:", attr(info, "server_info", "serverInfo").name, "| protocol", attr(info, "protocol_version", "protocolVersion"))
            tools = await s.list_tools()
            print("tools:", [t.name for t in tools.tools])
            r = await s.call_tool("site_overview", {}); print("overview:", r.content[0].text.split("\n")[0][:110])
            r = await s.call_tool("open_page", {"url": "/shop/"}); print("open /shop/:", first_line(r))
            r = await s.call_tool("do_action", {"form": 2, "fields": {"Quantity": "2"}}); print("add whale:", first_line(r))
            r = await s.call_tool("open_page", {"url": "/shop/checkout.php"}); print("checkout:", first_line(r))
            r = await s.call_tool("do_action", {"form": 1, "fields": {"Username": "reza", "Password": "s3cret"}}); print("log in:", first_line(r))
            r = await s.call_tool("open_page", {"url": "/shop/cart.php"}); print("basket (session kept):", json.loads(r.content[0].text.split("\n\n",1)[1])["content"][:60].replace("\n"," | "))
            r = await s.call_tool("open_page", {"url": "/shop/checkout.php"}); print("checkout:", first_line(r))
            r = await s.call_tool("do_action", {"form": 1, "fields": {"address": "1 Rue de la Loge", "card_number": "4242424242424242"}}); print("order, no confirm:", first_line(r))
            r = await s.call_tool("do_action", {"form": 1, "confirm": True, "fields": {"address": "1 Rue de la Loge", "card_number": "4242424242424242"}}); print("order, confirmed:", first_line(r))
            r = await s.call_tool("ask", {"question": "tell me about the whale widget"}); print("ask:", first_line(r)[:90], "| ads delineated:", '"sponsored"' in r.content[0].text)
            r = await s.call_tool("do_action", {"action": "nope"}); print("bad action:", first_line(r)[:90])

async def other_user():
    async with _client(URL) as streams:
        async with ClientSession(streams[0], streams[1]) as s:
            await s.initialize()
            r = await s.call_tool("open_page", {"url": "/shop/account.php"})
            print("second session (a different user) opening the account page:", first_line(r))

asyncio.run(main())
asyncio.run(other_user())
