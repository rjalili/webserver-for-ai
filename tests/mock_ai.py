# Mock AI service for testing mod_http_b without a real API key.
# Run: python3 tests/mock_ai.py  (listens on 127.0.0.1:9999)
# Configure: AIAgentAIService anthropic / AIAgentAIKey sk-test-123 / AIAgentAIEndpoint http://127.0.0.1:9999/v1/messages
#        or: AIAgentAIService openai / AIAgentAIKey sk-openai-456 / AIAgentAIModel gpt-test / AIAgentAIEndpoint http://127.0.0.1:9999/v1/chat/completions
# Mock Anthropic + OpenAI APIs for testing mod_http_b. Records requests to /tmp/mock_calls.jsonl
import json, sys, http.server
MODE = {"fail": False}
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        rec = {"path": self.path, "headers": {k.lower(): v for k, v in self.headers.items()}, "body": body}
        open("/tmp/mock_calls.jsonl", "a").write(json.dumps(rec) + "\n")
        if self.path == "/control/fail":
            MODE["fail"] = body.get("fail", False); return self._send(200, {})
        if MODE["fail"]:
            return self._send(529, {"type": "error", "error": {"type": "overloaded_error", "message": "Overloaded"}})
        if self.path == "/v1/messages":
            assert self.headers["x-api-key"] == "sk-test-123", "bad key"
            assert self.headers["anthropic-version"] == "2023-06-01"
            user = body["messages"][0]["content"]; system = body["system"]
        else:
            assert self.headers["Authorization"] == "Bearer sk-openai-456"
            system = body["messages"][0]["content"]; user = body["messages"][1]["content"]
        q = user.split("QUESTION FROM THE VISITING AI AGENT:\n")[1].strip()
        facts = []
        if "29 EUR" in user: facts.append("whale widget costs 29 EUR (/products/whale.html)")
        if "9:00" in user: facts.append("open Mon-Fri 9-17")
        if "EARLIER IN THIS CONVERSATION" in user: facts.append("I remember the earlier question")
        if "Livraison gratuite" in user: facts.append("saw French page")
        text = f"[mock {self.path}] You asked: {q}. Known: {'; '.join(facts) or 'nothing'} — café \U0001F433"
        if self.path == "/v1/messages":
            self._send(200, {"id": "msg_1", "type": "message", "role": "assistant",
                             "content": [{"type": "text", "text": text}], "stop_reason": "end_turn"})
        else:
            self._send(200, {"choices": [{"index": 0, "message": {"role": "assistant", "content": text}}]})
    def _send(self, code, obj):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
http.server.ThreadingHTTPServer(("127.0.0.1", 9999), H).serve_forever()
