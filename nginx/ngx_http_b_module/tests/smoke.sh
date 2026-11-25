#!/bin/sh
# Stage 1 checks for ngx_http_b_module. Usage: smoke.sh http://host:port
H=${1:-http://127.0.0.1:8080}; fail=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
check "Link header on human requests" '[ -n "$(curl -si $H/ | grep -i "^link:.*rel=\"ai-agent\"")" ]'
check "no help header for humans" '[ -z "$(curl -si $H/ | grep -i "^x-ai-agent-help")" ]'
check "known agent gets help header" '[ -n "$(curl -si -A "GPTBot/1.0" $H/ | grep -i "^x-ai-agent-help")" ]'
check "llm only as a whole word" '[ -z "$(curl -si -A smallmodel $H/ | grep -i "^x-ai-agent-help")" ]'
check "X-AI-Agent header marks an agent" '[ -n "$(curl -si -H "X-AI-Agent: test" $H/ | grep -i "^x-ai-agent-help")" ]'
check "Accept text/markdown sets Vary" '[ -n "$(curl -si -H "Accept: text/markdown" $H/ | grep -i "^vary:.*x-ai-mode")" ]'
check "welcome document" 'curl -s $H/.well-known/ai | grep -q "\"type\":\"welcome\""'
check "unknown endpoint is 404" '[ "$(curl -so /dev/null -w "%{http_code}" $H/.well-known/ai/nope)" = 404 ]'
check "DELETE on endpoint is 405" '[ "$(curl -so /dev/null -w "%{http_code}" -X DELETE $H/.well-known/ai)" = 405 ]'
exit $fail
