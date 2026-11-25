#!/bin/sh
# Teach mod_http_b a site in one pass instead of waiting for visitor traffic.
# Fetches every page in the sitemap (and pages they link to) anonymously, which
# the module learns from exactly as it learns from real visitors.
# Usage: tools/prime.sh https://www.example.com [max_pages]
SITE=${1:?usage: prime.sh https://site [max_pages]}; MAX=${2:-500}
command -v python3 >/dev/null || { echo "python3 required"; exit 1; }
python3 - "$SITE" "$MAX" <<'PY'
import json, sys, urllib.request, urllib.parse
site, maxp = sys.argv[1].rstrip('/'), int(sys.argv[2])
def get(path, guide=False):
    url = site + path + (('&' if '?' in path else '?') + 'ai=guide' if guide else '')
    req = urllib.request.Request(url, headers={'User-Agent': 'mod_http_b-prime/1.0'})
    with urllib.request.urlopen(req, timeout=30) as r: return r.read()
todo = [p['url'] for p in json.loads(get('/.well-known/ai/sitemap.json'))['pages']]
seen = set()
while todo and len(seen) < maxp:
    path = todo.pop(0)
    if path in seen or not path.startswith('/'): continue
    seen.add(path)
    try:
        g = json.loads(get(path, guide=True))     # guide requests are learned too, and give us links
        todo += [l['url'] for l in g.get('links', []) if l['kind'] == 'internal']
        print('learned', path)
    except Exception as e:
        print('skipped', path, e)
print(len(seen), 'pages visited. Check status at', site + '/.well-known/ai')
PY
