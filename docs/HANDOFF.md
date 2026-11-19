# Webserver For AI: Handoff and Maintenance Guide

For whoever takes over maintenance and the next generation of **mod_ai_agent**: a developer, a team, or an AI coding agent (Claude Code, Cursor, …).

> **Important for Lovable users.** Lovable builds web apps (React/TypeScript). It is the right tool for the **distribution website** (see `LOVABLE_BRIEF.md`). It cannot compile, test or maintain this module, which is ~6,800 lines of C running inside Apache httpd. Maintenance needs a C developer, or an AI coding agent with a Linux shell (for example Claude Code), using the instructions below.

---

## 1. What the project is

An open-source Apache httpd module that gives any website an interface for AI agents, without changing the website. It lets agents:

1. **read** every page exactly as humans see it, or explained in English;
2. **discover** the whole site through an English sitemap;
3. **ask** questions in English, answered by the site's own configured AI. The AI only switches on after it has learned the site; until then it says "I don't know this site well enough yet";
4. **act**: do everything a person can do by clicking (search, forms, log in, basket, checkout), with the agent's own session;
5. **be used from Claude, ChatGPT and other assistants** through an MCP server at `/.well-known/ai/mcp`;
6. **respect advertising**: page ads and configured agent ads are relayed in a clearly labelled `sponsored` block, never mixed into content or AI answers;
7. **chat**: any URL can become a conversation (`ai-chat/1`). The server tells the agent where it is, what it can do and where it can go; the agent replies with `say`, `do` or `go`.

All of this respects logins and never leaks login-protected content. The module reads the site's PHP and JavaScript code to know which pages need a login.

Vision (from the project owner): servers spend enormous effort making pages beautiful for humans. For utility, the content type can simply be "chat". Agents and servers hold a conversation over the site's resources, and **browsers gain a plugin, eventually built in, that turns browsing into chat or a hybrid of chat and HTML.**

## 2. Repository layout

```
mod_ai_agent/
  START_HERE.md         entry point for everyone (owner, Lovable, maintainers)
  mod_ai_agent.c        the whole module (one C file, ~7,500 lines)
  Makefile              build (apxs), install, Debian enable, tests
  ai_agent.conf         every directive, commented
  README.md             user documentation (install, configure, features)
  docs/
    PROTOCOL.md         the protocol: modes, endpoints, ai-chat/1, security rules
    HANDOFF.md          this file
    LOVABLE_BRIEF.md    brief + content for the distribution website
    CHANGELOG.md        version history
    SECURITY.md         security model and reporting
  extension/            "AI Chat" browser extension (Chrome, Brave, Edge, Opera, Vivaldi, Firefox, Safari)
    src/                chat-core.js (protocol client), panel.* (UI), background.js
    build.py            per-browser manifests, icons, zips  ->  dist/
    test/               core.test.mjs (Node vs live server), ui.test.mjs (Playwright + Chrome)
    README.md           install, store publishing, Safari conversion, permissions
  tools/
    prime.sh            teach the module a site in one pass
    chat_client.py      reference ai-chat/1 client (what a browser plugin does)
  tests/
    smoke.sh            11 HTTP-level checks against a running server
    agent_shop_demo.sh  end-to-end agent walkthrough of the PHP shop fixture
    mock_ai.py          mock Anthropic/OpenAI API for testing without a key
    mcp_test.py         drives /.well-known/ai/mcp with the official MCP Python SDK
    fixtures/           PHP site, PHP shop (sessions + CSRF), Laravel routes,
                        Express app, React/Vue/Next SPA, expected access map
```

## 3. Build, install, test

```sh
# Debian/Ubuntu (tested on Ubuntu, Apache 2.4.58, both prefork+mod_php and event MPM)
sudo apt install apache2 apache2-dev libcurl4-openssl-dev
make                      # apxs -c mod_ai_agent.c -lcurl
sudo make enable-debian   # installs, a2enmod, reloads

# RHEL/Fedora
sudo dnf install httpd-devel libcurl-devel && make && sudo make install

# Tests (against a running server)
tests/smoke.sh http://localhost
tests/agent_shop_demo.sh http://localhost     # needs tests/fixtures/php-shop served at /shop/ and PHP
python3 tools/chat_client.py http://localhost /shop/   # interactive chat
```

**Recommended dev setup.** Use a throwaway VM or container with Apache, mod_php and the fixtures, served like this:
- DocumentRoot = a copy of `tests/fixtures/php-site`
- the shop at `/shop/`
- `AIAgentSourceDir` lines for the fixture apps (see `tests/fixtures/README.md`)
- **the mock AI server** in `tests/mock_ai.py`: it speaks the Anthropic `/v1/messages` and OpenAI `/v1/chat/completions` formats, records every request to `/tmp/mock_calls.jsonl` and can simulate failures. Point `AIAgentAIEndpoint` at it.

Build with full warnings; the code base is warning-clean:
`apxs -c -Wc,-Wall -Wc,-Wextra -Wc,-Wno-unused-parameter -Wc,-Wno-missing-field-initializers -Wc,-Wno-sign-compare mod_ai_agent.c -lcurl`

## 4. Architecture

### Request flow (Apache hooks)

| Hook | Function | Job |
|---|---|---|
| post_config | `ai_post_config` | Validate AI config, compile regexes, create knowledge dirs, generate the internal-request secret |
| child_init | `ai_child_init` | Per-process cache pool and mutex |
| post_read_request | `ai_post_read` | Recognise internal requests made for agents (restore the agent's IP); detect agents; pick the mode; strip `?ai=` |
| translate_name | `ai_translate` | Claim `/.well-known/ai/*`, `/llms.txt` and chat-mode requests for the module |
| map_to_storage | `ai_map_storage` | Skip the filesystem for claimed requests |
| handler | `ai_handler` | Endpoints: welcome, sitemap, ask, page, actions, act, chat |
| insert_filter / insert_error_filter | `ai_insert_filter` | Add the guide/text filter or the learning filter; Link headers |
| output filter | `AI_AGENT_GUIDE` | Buffer the HTML and rewrite it as an English guide or Markdown |
| output filter | `AI_AGENT_LEARN` | Copy public responses into the knowledge store; forget 404/410 pages |

### Code map (mod_ai_agent.c, top to bottom)

| Section | What's there |
|---|---|
| Configuration | `ai_cfg`, directive handlers, `ai_cmds[]` (all `AIAgent*` directives), defaults via `CFG_ON` |
| String buffer + JSON helpers | `sb_t` growable buffer, `sb_json` (with UTF-8 repair), `jstr`/`jkey` |
| HTML entity decoding, URL helpers | `decode_entities`, `resolve_url`, `normalize_path` |
| HTML parser → page model | Tolerant tag scanner → `page_t`: title, description, language, headings, links, images, forms (`form_t`/`field_t`/`button_t`), Markdown text |
| English descriptions | `page_summary`, `form_english`, `field_type_english` |
| Mode detection | `parse_mode`, `detect_agent` (built-in UA list), `ai_post_read` |
| Site crawl | DocumentRoot walk with access checks (`collect_pages`) → `sm_page` |
| Endpoint responses | welcome, sitemap, llms.txt |
| Code scan | Login detection from source: PHP guards/includes/guard functions, Laravel, Express (routes, router mounts, middleware), React/Vue/Angular route guards, Next.js middleware, .htaccess → `access_t` map; endpoint discovery (`endpoint_t`) |
| Mini JSON parser | `jparse`/`jget`/`jwrite`, for agent request bodies |
| Actions | `action_t`/`af_t`, `form_to_action`, `classify_action` (kinds, sensitivity, naming), `compute_action_id` |
| Knowledge store | Learned pages (`pages/*.page` text + `*.html`), readiness (`get_state`), per-process caches (access map, catalog), rate limits, answer cache, conversations, question log |
| AI client | libcurl calls to Anthropic / OpenAI / OpenAI-compatible APIs, system prompt, context builder |
| Action catalog | `build_catalog`, `action_catalog` (cached 60 s), `/actions` |
| /act | `act_core` (the engine: loopback requests via libcurl, cookie jar, form filling, redirects, gates), `agent_post_guard` (CSRF), `do_act` |
| /ask | `rank_pages`, `keyword_answer`, `ai_context`, `do_ask` |
| Advertising | `select_ads` (targeting, weighted rotation), `json_sponsored`, `md_sponsored`, `do_ad_click`; the parser's `is_ad_element` / `end_ad` keep ads out of page text |
| Chat | `chat_core` (one turn), `do_chat` (HTTP wrapper), `chat_answer`, conversation state |
| MCP | `do_mcp` (JSON-RPC over Streamable HTTP), `mcp_tools_list`, `mcp_tools_call`, `mcp_load_cookies` / `mcp_save_cookies` (server-side cookie jar per session) |
| Filters, hooks, module record | `ai_guide_filter`, `ai_learn_filter`, `ai_insert_filter`, `ai_register_hooks` |

### Key design decisions (and why)

1. **One C file, no dependencies beyond APR and libcurl.** It's easy to package and has no runtime surprises. If it grows further, split it along the sections above into `src/*.c`.
2. **Actions run as real HTTP requests to the same server** (libcurl pinned to the local address with `CURLOPT_RESOLVE`), not as Apache subrequests. The whole stack runs exactly as for a browser: rewrites, proxies, PHP, auth. A secret header (regenerated at every start) marks these requests, and the agent's real IP is restored so `Require ip` rules and logs see the agent.
3. **Learning only from anonymous traffic.** Any request with `Cookie` or `Authorization` is never learned, and neither are `Cache-Control: private` responses or pages the code scan marks as login-only. This is the core no-leak guarantee.
4. **Static analysis for logins**, combined with Apache sub-request access checks. The scanner errs towards protecting pages.
5. **Per-process caches with pool rotation.** The access map and catalog are rebuilt into a fresh pool. The previous pool is freed one cycle later, so concurrent readers stay safe. **Rule: anything stored in a cached structure must be allocated from the cache's pool, never from `r->pool`.** Breaking this rule caused a real bug: dangling found_on URLs, fixed in 1.4.0.
6. **English for intent, structure for action** (see PROTOCOL.md). The server never interprets free text to act. The client's own intelligence chooses from exact actions.

### Knowledge directory (per site)

Default `/var/lib/apache2/ai_agent/<host>/` (or `/var/lib/httpd/...`), mode 0750, owned by the Apache user:

| File | Contents |
|---|---|
| `started` | When learning began |
| `ready` | Touch this to switch the AI on early |
| `pages/` | `<md5>.page` (text) and `<md5>.html` (raw) for each learned page |
| `cache/` | Cached answers |
| `conv/` | Conversations: `.txt` for ask history, `.chat` for location |
| `access.txt` | The code scan report, with evidence (admin only) |
| `questions.log`, `actions.log` | Audit logs (never field values) |
| `rate`, `rate-act` | Rate counters |

## 5. How to extend

**Add a directive.** Add a field to `ai_cfg` and set it to UNSET in `create_cfg`. Add a `MERGE_INT`/`MERGE_STR` line in `merge_cfg` and an `AP_INIT_*` line in `ai_cmds[]`. Read it with `CFG_ON(c->field, default)`. Document it in `ai_agent.conf` and README.md.

**Add a language to the code scanner** (Ruby, Python and Java are next):
1. Recognise the file extension in `scan_walk` and give it a new `kind`.
2. Add regexes to the `RX` struct and to `compile_regexes`. They are PCRE syntax through `ap_pregcomp`.
3. Write an `xxx_scan(sc, cf)` that calls `add_access(...)` for each page or route found. Queue POST endpoints on `sc->late`.
4. Call it from `run_code_scan`.
5. Add fixtures plus their expected `access.txt` lines in `tests/fixtures`.

The patterns to recognise for each framework:
- **Rails:** `before_action :authenticate_user!`, `authenticate :user do` in routes.rb, `devise`.
- **Django:** `@login_required`, `LoginRequiredMixin`, `path(...)` in urls.py.
- **Spring:** `@PreAuthorize`, `.authenticated()` in SecurityFilterChain, `@RequestMapping`.

Always check that the guard's *consequence* sits inside the guarded block (see `guard_body_ok`). Version 1.3 had a false positive that ignored this.

**Add an action kind.** Extend `classify_action`: the keyword tests, `a->kind`, the `sensitive` flag and the description sentence. Anything with real-world consequences must set `sensitive = 1`.

**Add an endpoint.** Add a route in `ai_handler`. Write the response with `send_body` (public, cacheable) or `act_reply` (private, no CORS). Any state-changing POST must call `agent_post_guard` first.

## 6. Known limitations / technical debt

- **Static analysis is heuristic.** Unusual auth, such as permissions stored in a database or deep class hierarchies, can be missed. The README tells owners to use `AIAgentExclude` for sensitive areas.
- **Buttons that only run JavaScript** are covered only when the scanner finds their `fetch`/`axios`/`$.post` call.
- **No file uploads** in actions.
- **CAPTCHAs, two-factor codes and payment pages on other websites** need a person; the agent is told so.
- **Keyword retrieval** is used for the AI context (no embeddings or vector search).
- **No sitemap cache.** The docroot is walked per sitemap/ask request (capped by `AIAgentMaxPages`), and the catalog is cached for 60 s per process.
- **AI calls and action loopbacks run inside Apache workers.** Size `MaxRequestWorkers` accordingly. Every action uses one extra worker while it runs.
- **One 7,500-line file**: splitting it is overdue.
- **Ad detection** relies on markup conventions (class/id/aria/data-ad tokens and `rel="sponsored"`); unusual ad markup may be missed. Script-filled ad slots have nothing to relay, which is why agent ads exist.
- **MCP:** tool results return the ai-chat turn as text (no `structuredContent` yet). There is no OAuth: public-site access only. Behind a cookie-based login, sessions are server-side per MCP session.
- **Extension:**
  - the discovery badge can miss the first page loaded while the browser is starting;
  - Safari packaging needs a Mac with Xcode;
  - no file uploads;
  - Opera has no side-panel API, so it uses a popup there.
- **Not yet done:** packaging (.deb/.rpm), CI, unit tests for the parser and scanner, and memory testing with ASan/Valgrind. The fuzzing so far was HTTP-level only.
- **Licence:** the source header says Apache License 2.0. **The owner must confirm this choice** and add the full LICENSE text before publishing.

## 7. Next-generation roadmap (suggested order)

1. **Publish:**
   - a GitHub repo with CI (build + smoke tests in Docker, both MPMs),
   - .deb/.rpm packages,
   - a Docker image with Apache, the module and the demo shop,
   - the distribution site (LOVABLE_BRIEF.md).
2. **Browser extension: publish it.** It's built and tested (`extension/`). Next: publish to the stores (see `extension/README.md`); add a full-screen chat-only mode; offer a per-site "always chat" preference; localise it.
3. **Hybrid responses:** let the server embed the first chat turn inside HTML (`<script type="application/vnd.ai-chat+json">`), so one request serves both views.
4. **WebMCP bridge:** publish the action catalog as WebMCP tool definitions (Google/Microsoft W3C draft; ChatGPT's browser already calls them), so every Apache site supports WebMCP for free.
5. **MCP: improve it.** It works already (`/.well-known/ai/mcp`). Next: `structuredContent` output schemas; OAuth for sites that want to identify assistants; listing in assistants' connector directories.
6. **Standardisation:** register the media type and write an Internet-Draft from PROTOCOL.md. Coordinate with llms.txt and WebMCP.
7. **Ports:** an nginx module, a Caddy plugin, and Node/PHP middleware sharing one core.
8. **Code scanner:** Ruby, Python and Java (see section 5).
9. **AI quality:** embeddings for retrieval, page translation, answer citations.
10. **Uploads** in actions, and a **sitemap cache**.

## 8. Development history and test evidence

Built in one development session (November 2026). Each version was compiled warning-free against Apache 2.4 and tested on a live server:

- **1.0** Modes, endpoints, parser. Fuzzed with 800 malformed HTML pages, no crashes.
- **1.1** AI service plus the learning gate, tested against a mock Anthropic/OpenAI server:
  - learning period and page minimum, `ready` override, immediate activation
  - privacy filters, conversations, cache, rate limit, fallback on AI failure
  - both API formats, emoji/UTF-8, refusing to start with an incomplete config
- **1.2** Learning modes plus the PHP/JS code scan. Fixture apps were sorted correctly. A 2,800-file codebase scanned in ~0.15 s. 300 random code files fuzzed without crashes.
- **1.3** The action catalog plus `/act`, tested end-to-end on a PHP shop with sessions and CSRF:
  - search, basket, login and checkout with confirmation
  - no private content anywhere; CSRF, cross-origin and IP-rule checks
  - 300 malformed `/act` bodies, all answered correctly
- **1.5** Advertising, MCP and the browser extension:
  - Ads were separated from content and kept out of learning and the AI. Decoys (`add-to-cart`, `site-header`) stayed as content. Targeting by path and keyword worked, clicks redirect only to configured URLs, and 150 ad-markup fuzz pages caused no crashes.
  - MCP was tested with the official SDK: login carried over between calls, the confirmation gate held, a second session was isolated, and no cookies reached the client. It also works on a site with no configuration at all.
  - The extension was tested by loading it into real Chrome and running a full shopping flow, with screenshots reviewed. Firefox `web-ext lint` reports 0 errors and 0 warnings.
- **1.4** ai-chat/1 on every URL. A full shop purchase was done purely by conversation, including a session-backed `say` answered by the AI. Browser-extension origins work; other origins get 403. Fixed a memory-lifetime bug in the cached catalog.

The mock AI server (`tests/mock_ai.py`): a Python `http.server` with POST handlers. `/v1/messages` checks `x-api-key` and `anthropic-version`, then returns `{"content":[{"type":"text","text":...}]}`. `/v1/chat/completions` checks `Authorization: Bearer` and returns `{"choices":[{"message":{"content":...}}]}`. `/control/fail` toggles 529 errors. Every request is appended to a JSONL file so tests can check what context the module sent.
