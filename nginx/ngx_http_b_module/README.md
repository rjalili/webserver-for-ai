# ngx_http_b_module: http-b for nginx

`ngx_http_b_module` gives nginx everything `mod_http_b` gives Apache: the
five layers of the http-b protocol (exact, read, discover, ask, act/chat), the
MCP endpoint, learning, the code scanner, sponsored answers and the same
safeguards. An AI agent cannot tell which of the two servers it is talking to.

## How it works

The protocol is implemented once, in `../../mod_http_b.c`. nginx compiles that
same file, unchanged, through a thin layer:

| File | Role |
|---|---|
| `ngx_http_b_module.c` | nginx side: directives, request phase, header and body filters, thread pool |
| `hb_core.c` | runs `mod_http_b.c` on APR, providing the parts of the Apache API it uses |
| `hb_core.h` | the boundary between the two (plain C types only) |

So every fix and feature in `mod_http_b.c` reaches both servers, and the
test suite in `../../tests` runs against either.

## Requirements

- nginx 1.22 or later, built **with threads** (`--with-threads`; most distribution packages are)
- APR and APR-util, the Apache httpd development headers (headers only; Apache itself does not need to run), libcurl and PCRE2

| System | Packages |
|---|---|
| Debian, Ubuntu | `libapr1-dev libaprutil1-dev apache2-dev libcurl4-openssl-dev libpcre2-dev` |
| RHEL, Fedora, Rocky | `apr-devel apr-util-devel httpd-devel libcurl-devel pcre2-devel` |
| macOS (Homebrew) | `apr apr-util httpd curl pcre2` |

## Build and install

As a dynamic module, for the nginx you already run:

```sh
nginx -v                                   # e.g. nginx/1.26.2
curl -O https://nginx.org/download/nginx-1.26.2.tar.gz && tar xzf nginx-1.26.2.tar.gz
cd nginx-1.26.2
./configure --with-compat --with-threads --add-dynamic-module=/path/to/webserver-for-ai/nginx/ngx_http_b_module
make modules
sudo cp objs/ngx_http_b_module.so /usr/lib/nginx/modules/   # or /etc/nginx/modules/
```

Use exactly the version `nginx -v` reports. Paths to APR, the httpd headers and
the libraries come from `apr-1-config`, `apu-1-config` and `apxs`; set
`HTTP_B_INCS` (include directories) and `HTTP_B_LIBS` (linker flags) to override.
For a static build, use `--add-module=` instead of `--add-dynamic-module=`.

Then load it at the top of `nginx.conf` and turn it on:

```nginx
load_module modules/ngx_http_b_module.so;

http {
    http_b            on;
    http_b_site_name  "Bluewhite";
    ...
}
```

`nginx -t && nginx -s reload`, then check:

```sh
curl -s https://www.example.com/.well-known/ai
../../tests/smoke.sh https://www.example.com
```

A commented example is in `http_b.conf`.

## Directives

Every Apache directive `AIAgentXyz` is the nginx directive `http_b_xyz`, with
the same arguments and the same defaults. They work in the `http` block (all
sites) and in `server` blocks (one site; a server inherits from `http`).

| nginx | Apache | Meaning |
|---|---|---|
| `http_b` | `AIAgent` | Enable the AI agent interface for this server/vhost (default On) |
| `http_b_default_mode` | `AIAgentDefaultMode` | What detected agents get when they don't ask: exact (default), guide or text |
| `http_b_user_agent` | `AIAgentUserAgent` | Extra User-Agent substrings (case-insensitive) that identify AI agents |
| `http_b_builtin_user_agents` | `AIAgentBuiltinUserAgents` | Use the built-in list of AI agent User-Agents (default On) |
| `http_b_site_name` | `AIAgentSiteName` | Human-readable name of the site |
| `http_b_site_description` | `AIAgentSiteDescription` | One-paragraph English description of what the site offers |
| `http_b_contact` | `AIAgentContact` | Contact information shown to agents |
| `http_b_prefix` | `AIAgentPrefix` | URL prefix of the agent endpoints (default /.well-known/ai) |
| `http_b_describe` | `AIAgentDescribe` | AIAgentDescribe /url "English description" ["Title"] |
| `http_b_exclude` | `AIAgentExclude` | URL prefixes never listed, searched or transformed |
| `http_b_faq` | `AIAgentFAQ` | AIAgentFAQ "keywords" "English answer" -- direct answers for /ask |
| `http_b_max_pages` | `AIAgentMaxPages` | Maximum pages in the sitemap / search (default 500) |
| `http_b_max_buffer` | `AIAgentMaxBuffer` | Maximum response bytes buffered for guide/text mode (default 4 MB) |
| `http_b_max_text` | `AIAgentMaxText` | Maximum characters of page text in a guide (default 20000) |
| `http_b_check_access` | `AIAgentCheckAccess` | Only list pages an anonymous visitor may access (default On) |
| `http_b_llms_txt` | `AIAgentLlmsTxt` | Generate /llms.txt if the site has none (default On) |
| `http_b_advertise` | `AIAgentAdvertise` | Send a Link header pointing to the agent entry point on every response (default On) |
| `http_b_ai_service` | `AIAgentAIService` | AI service that answers questions: anthropic, openai, compatible (any OpenAI-compatible API) or none |
| `http_b_ai_key` | `AIAgentAIKey` | API key for the AI service: the key itself, file:/path/to/keyfile or env:VARIABLE |
| `http_b_ai_model` | `AIAgentAIModel` | Model name for the AI service |
| `http_b_ai_endpoint` | `AIAgentAIEndpoint` | API URL (required for 'compatible'; optional override for the others) |
| `http_b_ai_timeout` | `AIAgentAITimeout` | Seconds to wait for the AI service (default 30) |
| `http_b_ai_max_tokens` | `AIAgentAIMaxTokens` | Maximum length of an AI answer in tokens (default 1024) |
| `http_b_ai_rate_limit` | `AIAgentAIRateLimit` | Maximum AI calls per minute for this site, across all processes (default 30; 0 = unlimited) |
| `http_b_ai_context` | `AIAgentAIContext` | Characters of site content sent with each question (default 24000) |
| `http_b_activation` | `AIAgentActivation` | learn (default): answer only after the learning period; immediate: answer right away |
| `http_b_learn_period` | `AIAgentLearnPeriod` | How long to learn from traffic before answering (default 7d) |
| `http_b_learn_min_pages` | `AIAgentLearnMinPages` | Pages that must have been learned before answering (default 20) |
| `http_b_learn_max_pages` | `AIAgentLearnMaxPages` | Maximum pages kept in the knowledge store (default 2000) |
| `http_b_learn_traffic` | `AIAgentLearnTraffic` | Learn pages from anonymous visitor traffic (default On when an AI service is set) |
| `http_b_learn_query_strings` | `AIAgentLearnQueryStrings` | Also learn URLs with query strings, skipping sensitive parameters (default On) |
| `http_b_knowledge_dir` | `AIAgentKnowledgeDir` | Directory for the knowledge store (default /var/lib/apache2/http_b/<host>) |
| `http_b_log_questions` | `AIAgentLogQuestions` | Log agents' questions to questions.log in the knowledge directory (default On) |
| `http_b_learning` | `AIAgentLearning` | always (default): keep learning forever; until-ready: stop once the AI is on; off |
| `http_b_code_scan` | `AIAgentCodeScan` | Read the site's PHP/JavaScript code to find pages that require login (default On) |
| `http_b_source_dir` | `AIAgentSourceDir` | AIAgentSourceDir /path/to/code [/url-prefix] -- where the site's code lives (default: DocumentRoot) |
| `http_b_code_scan_interval` | `AIAgentCodeScanInterval` | Seconds between re-scans of the code (default 300) |
| `http_b_actions` | `AIAgentActions` | Let agents do things on the site (submit forms, log in, buy): on (default), read-only or off |
| `http_b_act_rate_limit` | `AIAgentActRateLimit` | Maximum actions per minute for the site (default 60) |
| `http_b_ads` | `AIAgentAds` | Relay advertising to agents, clearly marked as sponsored (default On) |
| `http_b_ad` | `AIAgentAd` | AIAgentAd "Advertiser" "Ad text" "https://destination" ["/path-prefix keyword ..."] -- an ad for agent traffic |
| `http_b_ads_file` | `AIAgentAdsFile` | JSON file with agent ads: [{"advertiser", "text", "url", "paths": [...], "keywords": [...], "weight"}] |
| `http_b_ads_max` | `AIAgentAdsMax` | Maximum agent ads per response (default 2) |
| `http_b_ads_required` | `AIAgentAdsRequired` | Ask agents to show the ads to their users in return for access to the content (default Off) |
| `http_b_ads_terms` | `AIAgentAdsTerms` | Your own sentence explaining why agents should show the ads (used with AIAgentAdsRequired) |
| `http_b_list_protected` | `AIAgentListProtected` | List login-only pages (marked as such) in the sitemap; admin areas are never listed (default On) |

Relative paths (`http_b_ads_file`, `http_b_ai_key file:...`) are relative to the
nginx prefix (`nginx -p`, usually `/etc/nginx`). The default knowledge
directory is `/var/lib/apache2/http_b/<host>` or `/var/lib/httpd/http_b/<host>`
when those exist, otherwise `<prefix>/http_b/<host>`; set
`http_b_knowledge_dir` to choose. nginx's master process creates it and gives
it to the worker user.

## Differences from Apache

- **Threads.** Endpoints (`/.well-known/ai/...`, chat turns, MCP) run in
  nginx's `default` thread pool, because they may wait for an AI service or
  make requests back to this server (actions, access checks). Add
  `thread_pool default threads=32;` to the main context to size it. Without
  thread support they run in the worker, which blocks it; do not use that in
  production.
- **Requests to itself.** Where Apache uses sub-requests and internal
  redirects (access checks while listing pages, `/.well-known/ai/page`), nginx
  makes a real HTTP request to its own address. These requests carry the
  agent's address, and nginx applies it the way the realip module does, so
  `allow`/`deny`, `limit_req` and logs see the agent, not `127.0.0.1`.
- **Compression and caching.** For guide/text responses the module turns
  nginx's own gzip off and asks upstreams for an uncompressed body; it also
  removes conditional and range headers, as the Apache module does.
- **Error pages.** nginx's own error pages are explained like any other page
  (`?ai=guide` on a 404 says it is a 404).

## Tests

All of the repository's tests run unchanged against nginx:

```sh
../../tests/smoke.sh http://localhost:8080                       # 11 checks
../../tests/agent_shop_demo.sh http://localhost:8080              # PHP shop at /shop/ via PHP-FPM
python3 ../../tests/chat_demo.py http://localhost:8080
python3 ../../tests/mcp_test.py http://localhost:8080
node ../../extension/test/core.test.mjs http://localhost:8080
```

`tests/nginx-test.conf` is the configuration they were run with (nginx 1.26.2,
PHP-FPM, the mock AI service from `../../tests/mock_ai.py`).
