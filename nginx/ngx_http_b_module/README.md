# ngx_http_b_module — http-b for nginx

The nginx port of `mod_http_b`. It is being ported **in stages**; this file says
what works today.

| Stage | Covers | Status |
|---|---|---|
| 1 | Configuration, agent detection, mode selection, `ai=` removal, discovery `Link` header, `X-AI-Agent-Help`, welcome document at `/.well-known/ai` | **done** |
| 2 | `?ai=guide` / `?ai=text` page transformation (HTML parser, Markdown, page guide JSON) | planned |
| 3 | Site description: `/.well-known/ai/sitemap.json`, `/llms.txt`, page catalogue | planned |
| 4 | Ask: keyword answers, AI service calls, conversation ids, rate limits | planned |
| 5 | Act and chat: actions catalogue, `/act`, http-b/1 chat turns, `/chat` | planned |
| 6 | Learning, code scan / access map, MCP endpoint, sponsored answers | planned |

## Build

```sh
# dynamic module, against the same nginx version you run
cd nginx-1.26.x
./configure --with-compat --add-dynamic-module=/path/to/nginx/ngx_http_b_module
make modules
cp objs/ngx_http_b_module.so /etc/nginx/modules/
```

Then, at the top of `nginx.conf`:

```nginx
load_module modules/ngx_http_b_module.so;
```

Static build: use `--add-module=` instead of `--add-dynamic-module=`.

## Configuration

```nginx
http {
    http_b                  on;
    http_b_site_name        "Bluewhite";
    http_b_site_description "Whale-themed widgets.";
    http_b_contact          hello@example.org;
    http_b_exclude          /admin /private;
}
```

| Directive | Apache equivalent | Default |
|---|---|---|
| `http_b on\|off` | `AIAgent` | off |
| `http_b_default_mode exact\|guide\|text` | `AIAgentDefaultMode` | exact |
| `http_b_user_agent <pattern> ...` | `AIAgentUserAgent` | — |
| `http_b_builtin_user_agents on\|off` | `AIAgentBuiltinUserAgents` | on |
| `http_b_site_name` | `AIAgentSiteName` | Host name |
| `http_b_site_description` | `AIAgentSiteDescription` | — |
| `http_b_contact` | `AIAgentContact` | — |
| `http_b_prefix` | `AIAgentPrefix` | `/.well-known/ai` |
| `http_b_exclude <prefix> ...` | `AIAgentExclude` | — |
| `http_b_advertise on\|off` | `AIAgentAdvertise` | on |

All directives work in `http`, `server` and `location`. The mode decided for a
request survives internal redirects (index files, `try_files`, `error_page`),
and `ai=` is removed before the request reaches `proxy_pass`/FastCGI backends.

## Test

`tests/smoke.sh` runs against a live nginx with the module enabled:

```sh
./tests/smoke.sh http://127.0.0.1:8080
```
