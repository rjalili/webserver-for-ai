#!/bin/sh
# Smoke test for mod_ai_agent. Usage: tests/smoke.sh [http://host:port]
# Requires curl and python3.
H=${1:-http://localhost}
pass=0; fail=0
ok()   { pass=$((pass+1)); echo "  ok   $1"; }
bad()  { fail=$((fail+1)); echo "  FAIL $1"; }
json() { python3 -c "import json,sys; d=json.load(sys.stdin); $1" 2>/dev/null; }

echo "Testing $H"
curl -s "$H/.well-known/ai" | json "assert d['type']=='welcome'" && ok "welcome endpoint" || bad "welcome endpoint"
curl -s "$H/.well-known/ai/sitemap.json" | json "assert d['type']=='sitemap' and len(d['pages'])>0" && ok "sitemap lists pages" || bad "sitemap lists pages"
curl -s "$H/.well-known/ai/ask?q=contact" | json "assert d['type']=='answer' and d['answer']" && ok "ask answers in English" || bad "ask answers"
curl -s -X POST -H 'Content-Type: application/json' -d '{"question":"contact"}' "$H/.well-known/ai/ask" | json "assert d['question']=='contact'" && ok "ask via POST JSON" || bad "ask via POST JSON"
curl -s "$H/?ai=guide" | json "assert d['type']=='page_guide' and d['summary']" && ok "?ai=guide" || bad "?ai=guide"
curl -s -H 'X-AI-Mode: guide' "$H/" | json "assert d['type']=='page_guide'" && ok "X-AI-Mode header" || bad "X-AI-Mode header"
curl -s -H 'Accept: text/markdown' "$H/" | grep -q "About this page (English)" && ok "Accept: text/markdown" || bad "Accept: text/markdown"
human=$(curl -s "$H/" | cksum); agent=$(curl -s -A 'ClaudeBot/1.0' "$H/?ai=exact" | cksum)
[ "$human" = "$agent" ] && ok "exact mode is byte-identical to the human page" || bad "exact mode identical"
curl -s -D- -o /dev/null "$H/" | grep -qi 'rel="ai-agent"' && ok "Link header advertised" || bad "Link header"
curl -s "$H/llms.txt" | grep -q "^# " && ok "/llms.txt" || bad "/llms.txt"
curl -s "$H/this-page-does-not-exist-$$?ai=guide" | json "assert d['status']==404" && ok "404 explained in English" || bad "404 guide"
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
