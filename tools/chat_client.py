#!/usr/bin/env python3
"""Minimal ai-chat/1 client: what a browser plugin or an AI agent does.
Keeps cookies and the conversation id, sends say/do/go turns, prints the server's turns.
Usage as a library: c = Chat("http://localhost"); c.start("/shop/"); c.say("..."); c.do(form=1, fields={...}); c.go("/x")"""
import json, sys, urllib.request, http.cookiejar

TYPE = "application/vnd.ai-chat+json"

class Chat:
    def __init__(self, site, verbose=True):
        self.site, self.conv, self.at, self.verbose = site.rstrip("/"), None, "/", verbose
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
    def _send(self, method, url, body=None):
        req = urllib.request.Request(self.site + url, method=method, headers={"Accept": TYPE, "Content-Type": TYPE},
                                     data=json.dumps(body).encode() if body is not None else None)
        try:
            with self.opener.open(req, timeout=60) as r: turn = json.loads(r.read())
        except urllib.error.HTTPError as e: turn = json.loads(e.read())
        self.conv = turn.get("conversation") or self.conv
        if turn.get("at"): self.at = turn["at"]["url"]
        if self.verbose: self.show(turn)
        return turn
    def start(self, url="/"): return self._send("GET", url)
    def turn(self, **kw): return self._send("POST", self.at, dict(conversation=self.conv, **kw))
    def say(self, text): return self.turn(say=text)
    def go(self, url): return self.turn(go=url)
    def do(self, confirm=False, **kw): return self.turn(do=dict(kw, **({"confirm": True} if confirm else {})))
    @staticmethod
    def show(t):
        print("  SERVER:", t.get("message", t))
        if t.get("you_can"): print("    you_can:", [(a["name"], a["do"]) for a in t["you_can"]])
        if t.get("result", {}).get("problem"): print("    problem:", t["result"]["problem"])

if __name__ == "__main__":
    c = Chat(sys.argv[1] if len(sys.argv) > 1 else "http://localhost")
    c.start(sys.argv[2] if len(sys.argv) > 2 else "/")
    for line in sys.stdin:
        line = line.strip()
        if line.startswith("go "): c.go(line[3:])
        elif line: c.say(line)
