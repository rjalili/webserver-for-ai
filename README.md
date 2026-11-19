# mod_ai_agent

An Apache httpd module that gives any website an interface for AI agents that speaks English. Load the module and every site on the server can be:

- **browsed headlessly exactly as humans see it.** AI agents get the same bytes as a browser.
- **explored through an English sitemap.** A JSON list of every page, each described in plain English.
- **questioned in plain English.** An agent asks a question and gets an English answer with the relevant pages. This means *your* agent can hold a conversation with *the website*.
- **a conversation.** Any URL can become a chat (`Accept: application/vnd.ai-chat+json`). The server tells the agent where it is, what it can do and where it can go; the agent replies with `say`, `do` or `go`. Browser plugins can use the same protocol to turn browsing into chat, or a hybrid of chat and HTML.
- **used from Claude, ChatGPT and other assistants.** Every site is also an MCP server (`/.well-known/ai/mcp`). Add the site as a custom connector and the assistant can browse, ask and act on it.
- **chatted with in the browser.** The included extension for Chrome, Brave, Edge, Opera, Firefox and Safari shows a chat beside any page that speaks ai-chat.
- **fair to advertisers.** Ads are relayed to agents in a separate, clearly labelled "sponsored" block, never mixed into content or answers.
- **operated like a person clicking.** It learns everything a visitor can do: search, forms, log in, add to basket, check out, and the API calls behind app buttons. An agent can then do any of it through one endpoint. The site's real pages and forms are used, with the agent's own session, and the result comes back in English.
- **aware of which pages need a login.** It reads the site's PHP and JavaScript code to work out which pages require signing in. Agents are told, and those pages are never learned or quoted.
- **answered by an AI that has learned the site.** You set an AI service (Anthropic, OpenAI or any OpenAI-compatible API) once. The module learns the site from anonymous visitor traffic, and until it knows the site well enough it says so instead of guessing.

It works for static sites and for dynamic ones (PHP, CGI, WordPress, reverse-proxied apps). The English page views are produced by an output filter on the real response. Nothing on the site has to change.

## Quick start

```sh
# Debian / Ubuntu
sudo apt install apache2-dev libcurl4-openssl-dev
make && sudo make enable-debian

# RHEL / Fedora / source builds
sudo dnf install httpd-devel libcurl-devel   # or yum
make && sudo make install && sudo systemctl reload httpd
```

You can then check the install:

```sh
curl https://your-site/.well-known/ai
make test URL=https://your-site
```

## How an AI agent talks to the site

| The agent wants... | Request |
|---|---|
| To be introduced to the site | `GET /.well-known/ai` |
| Every page, described in English | `GET /.well-known/ai/sitemap.json` |
| To ask a question | `GET /.well-known/ai/ask?q=do+you+ship+to+canada` or `POST` JSON `{"question": "..."}` |
| An English explanation of a page | `GET /any/page?ai=guide`, or `GET /.well-known/ai/page?url=/any/page` |
| A conversation, starting at any page | `GET /any/page` with `Accept: application/vnd.ai-chat+json` (or `?ai=chat`), then POST `say`/`do`/`go` turns |
| What can be done on the site | `GET /.well-known/ai/actions` |
| Do it, exactly like a person clicking | `POST /.well-known/ai/act` with JSON `{"action": "<id>", "fields": {...}}` |
| A page as readable Markdown | `GET /any/page?ai=text` |
| A page exactly as humans get it | `GET /any/page?ai=exact` (or just request it) |
| A summary for LLM tools | `GET /llms.txt` (generated if the site doesn't ship one) |

Agents can also choose the view with headers: `X-AI-Mode: guide|text|exact`, `Accept: application/vnd.ai+json` (guide), or `Accept: text/markdown` (text). The module removes the `ai=` parameter before the application sees the request, so your PHP or app code never notices it.

### The page guide (`?ai=guide`)

```json
{
  "type": "page_guide",
  "language": "en",
  "url": "/contact.html",
  "status": 200,
  "summary": "This page of Bluewhite is titled \"Contact\". It is organised into 1 main section: \"Contact us\". It contains about 40 words of text, ... 1 form and 0 images.",
  "outline": [{"level": 1, "heading": "Contact us"}],
  "forms": [{
    "explanation": "Form #1 sends a POST request to /send.php. It asks for: Your name (text, required, field name \"name\"); Email (email address, required); topic (a choice of: Order question, Wholesale, Press) ... Button: \"Send message\".",
    "method": "POST", "action": "/send.php", "fields": [ ... ]
  }],
  "links": [{"text": "About us", "url": "/about.html", "kind": "internal",
             "about": "A page on this site. Ask for its English guide at /about.html?ai=guide"}],
  "images": [...],
  "text": "# Contact us\n\n...the page as Markdown...",
  "next_steps": ["See this page exactly as humans do: GET /contact.html?ai=exact", "..."]
}
```

Every link carries the URL of its own guide, so an agent can keep digging through the site, and every step of the way it gets English back. Error pages (404, 403) and redirects are explained in English as well, and they point the agent back to the sitemap. Non-HTML resources (PDF, images, JSON) get an English description and are never mangled.

### Detecting agents

A request counts as coming from an AI agent if it:

- has a User-Agent on the built-in list (GPTBot, ChatGPT-User, OAI-SearchBot, ClaudeBot, Claude-User, anthropic-ai, PerplexityBot, Google-Extended, CCBot, Meta-ExternalAgent, Applebot-Extended, Amazonbot, MistralAI-User and others), or one you add with `AIAgentUserAgent`
- sends an `X-AI-Agent` header (recommended for your own agents)
- asks for one of the AI views

By default detected agents get the **exact** page plus `Link` and `X-AI-Agent-Help` headers that point them to the English interface. Set `AIAgentDefaultMode guide` if you want them to get the English guide automatically.

Every response also carries `Link: </.well-known/ai>; rel="ai-agent"`, so agents that don't identify themselves (for example a headless Chrome) can still find the interface.

## Chat: any URL as a conversation (ai-chat/1)

Pages are made beautiful for people. For agents, the useful form is a conversation: the server says where you are, what you can do and where you can go; the agent decides. **The full specification is in [docs/PROTOCOL.md](docs/PROTOCOL.md).**

```
GET /shop/   Accept: application/vnd.ai-chat+json
→ {"message": "You are at \"Bluewhite shop\". Here you can \"Search\", \"Add to basket: Whale widget - 29 EUR\" ...",
   "conversation": "12ae…", "at": {...}, "content": "...", "you_can": [{"name": "Search", "fields": [...], "do": {"form": 1}}, ...],
   "you_can_go": [{"text": "Basket", "go": "/shop/cart.php"}, ...], "also_on_this_site": [...]}

POST /shop/  Content-Type: application/vnd.ai-chat+json
{"conversation": "12ae…", "do": {"form": 2, "fields": {"Quantity": "2"}}}       → added to the basket
{"conversation": "12ae…", "go": "/shop/checkout.php"}                            → "needs login; here you can Log in"
{"conversation": "12ae…", "say": "do you ship to Canada?"}                       → English answer
```

- **What the server and client each keep.** The server keeps each conversation's location; the client keeps the site's cookies, as a browser does.
- **No AI needed to run it.** No AI service is needed for the conversation itself. `say` uses the site's AI when it's configured and ready, and keyword search otherwise.
- **Same rules as actions.** Every action gate applies: login, confirmation, read-only mode.
- **Ready for browser plugins.** Every HTML page carries `Link: <url>; rel="alternate"; type="application/vnd.ai-chat+json"`, so plugins can offer "chat with this page". Browser-extension origins are accepted; other websites can't drive the conversation.
- **Reference client.** `tools/chat_client.py` shows what a plugin or agent does.

## Use from Claude, ChatGPT and other AI assistants (MCP)

Chat assistants can't send custom headers or POST requests from a conversation, but they can use **MCP connectors**. Every site running the module is also an MCP server:

```
https://your-site/.well-known/ai/mcp
```

**How to connect it.** In Claude, add it as a custom connector (Settings → Connectors). Other MCP clients (ChatGPT's developer mode and apps, Cursor, VS Code, …) take the same URL. No authentication is needed; it gives the same access as the public website.

**Tools:**

| Tool | What it does |
|---|---|
| `site_overview` | What the site is, its main pages, everything you can do |
| `open_page` | Open a page; returns where you are, the text, forms (numbered) and links |
| `ask` | A plain-English question, answered from the site's content |
| `list_actions` | Every action with its fields |
| `do_action` | Fill in and submit a form (`form` number or `action` id, `fields`, `confirm`) |

**Sessions.** MCP clients run in the AI provider's cloud and don't keep cookies, so for MCP the module keeps the site's cookies **server-side, per `Mcp-Session-Id`**: a 128-bit random id, kept for 24 hours, in private files, deleted when the client ends the session. They are only ever sent back to the same site. Logging in through `do_action` therefore works across calls, and one assistant session never sees another's.

**Gates and ads.** The same gates apply (login, `confirm` for payments, deletions and password changes; read-only mode). The server's instructions tell the assistant to ask its user before confirming, and to label any sponsored block as advertising.

**What the site needs:**
- public HTTPS with a valid certificate (connectors connect from the provider's servers, not from your computer);
- `AIAgentActions` not `off`. MCP sessions use the knowledge directory, which is created automatically.

**Without a connector,** an assistant's ordinary web reading can still use the read-only parts: `?ai=text`, `?ai=guide`, `/llms.txt` and `/.well-known/ai/sitemap.json`.

## Advertising: respected, and always clearly marked

Ads pay for much of the web, so the module relays them to agents rather than stripping them out, and always keeps them apart from content.

**Ads on the page.** These are recognised by markup:
- containers whose class, id, `aria-label` or `data-ad-*` attributes say *ad*, *advert*, *sponsored*, *promoted* or `adsbygoogle` (matched as whole words, so `add-to-cart` or `header` never match),
- `rel="sponsored"` links.

They are taken out of the page text, kept out of learning and out of the AI's context, and delivered in a separate block. Inline sponsored links stay in the text, marked "(sponsored link)". Ad slots that scripts fill in a browser contain nothing on the server, so they are skipped.

**Agent ads you configure.** These are needed because script-loaded ads don't exist server-side:

```apache
AIAgentAd "SeaCo" "Charter a sailing boat in Montpellier, 20% off." "https://seaco.example/charter" "/shop/ sailing boat"
AIAgentAdsFile /etc/apache2/agent-ads.json   # [{"advertiser","text","url","paths":[...],"keywords":[...],"weight":1}]
AIAgentAdsMax 2
AIAgentAdsRequired On                        # ask agents to show the ads in return for access to the content
```

Targeting tokens starting with `/` are path prefixes; the others are keywords, matched against the page title or the agent's question. Ads without targeting are shown anywhere, and weighted rotation spreads impressions.

**Where ads appear:**
- Chat turns, page guides, answers and MCP results carry a `"sponsored"` object: `{"notice": "...", "show_to_user": "requested by the site" | "optional", "items": [{"label": "Sponsored", "advertiser", "text", "link", "destination"}]}`. The notice tells agents to label ads clearly, keep them separate, leave them unchanged, and never present them as the site's answer or their own recommendation.
- Markdown views end with a quoted **Sponsored** block.
- The AI is instructed never to put ads in its answers.

**Measurement.** Impressions and clicks are logged to `ads.log` (time, ad, where, page, agent). Click links (`/.well-known/ai/ad/<id>`) count the click, then redirect, only to the configured destination (never an open redirect).

## Browser extension

`extension/` contains **AI Chat**, the browser plugin for ai-chat: one codebase for Chrome, Brave, Edge, Opera, Vivaldi, Firefox and Safari. It shows the conversation beside the page:
- forms generated from the site's field descriptions, links, and questions in plain English,
- a confirmation card before payments,
- a clearly labelled Sponsored box,
- the same login as the tab, which makes it a hybrid of chat and normal browsing.

Build with `python3 extension/build.py`. Installation, store publishing and Safari conversion steps are in [extension/README.md](extension/README.md).

## Doing things on the site: actions

The module learns **all the site's functionality** and lets an agent use it exactly as a person would by clicking.

### What it learns

`GET /.well-known/ai/actions` lists every action. Each has an English name and description, its fields (label, what it expects, required or not, the allowed choices) and flags for `requires_login` and `needs_confirmation`.

| Source | Examples |
|---|---|
| Forms on public pages | Static HTML, and dynamic pages learned from visitor traffic: search, contact, log in, sign up, add to basket… |
| PHP handlers | `$_POST`/`$_GET`/`filter_input` reads, used to confirm fields and find **required** ones (`empty($_POST['message'])`) |
| Express / Laravel routes | `app.post('/newsletter', ...)` with the `req.body` fields the handler reads |
| JavaScript app buttons | `fetch('/api/x', {method:'POST', body: JSON.stringify({...})})`, `axios.post(...)`, `$.post(...)` |

Similar forms are kept apart. A product page with one "Add to basket" form per product produces "Add to basket: Whale widget" and "Add to basket: Dolphin gadget".

### Doing it

```json
POST /.well-known/ai/act
{"action": "b3c3550324e3", "fields": {"Quantity": "2", "giftwrap": true}}
```

Exactly like a person:
1. **It opens the page** with the form, using the agent's own cookies. Hidden fields and anti-forgery (CSRF) tokens are therefore fresh and valid.
2. **It fills in the form** as a browser would: your values (by field name *or* label; a choice can be given by its visible text), defaults, ticked boxes, selected options, and the clicked button.
3. **It submits** through the real site (PHP, proxied apps, rewrites, everything) and follows redirects.
4. **It answers in English:** "I submitted 'Log in'. The site redirected to /shop/account.php and showed 'My account'…". The answer includes the resulting page (text, forms with their action ids, links), the HTTP status, and any **cookies the site set**. Cookies are returned both as `Set-Cookie` headers and in JSON, so logging in works for any agent.

Other ways to call it:
- `{"open": "/shop/cart.php"}` to click a link.
- `{"page": "/shop/checkout.php", "form": 1, "fields": {...}}` to use any form the agent is looking at, including forms on pages it can only see after logging in.

If an action can't go ahead, nothing is submitted and the agent is told why in English:
- **unknown fields:** a person couldn't fill those in either
- **invalid choices:** with the allowed values listed
- **missing required fields**
- **the same form shown again:** "which often means something was wrong" (for example, a wrong password)
- **a redirect to another website**, such as a payment provider: reported, never followed

### Safety rules

- **Logins are respected, never bypassed.** The site's own code decides who sees what. Without a session, a login-only action returns `needs_login` and the ids of the log-in actions.
- **Nothing behind a login leaks.** Only anonymous pages are learned. Forms on login-only pages and endpoints known only from login-only code are never listed. Logged-in results go only to that agent: they are never cached, learned, given to the AI, or written to logs.
- **Consequential actions need consent.** Payments and orders, deletions and cancellations, and password changes return `needs_confirmation` until the agent sends `"confirm": true`, which it should only do after its user agreed.
- **The agent's real IP is kept.** Requests made on an agent's behalf go to this same server, tagged with a secret header, so the agent's real IP applies: `Require ip` rules, logs and PHP's `REMOTE_ADDR` all see the agent, not localhost. A forged header is ignored.
- **Web pages can't trigger actions (CSRF protection).** `/act` only accepts `application/json`, which forces a CORS preflight that is never granted. Cross-site browser requests (`Sec-Fetch-Site`, a foreign `Origin`) are refused, so a malicious page can't make a visitor's browser act through it.
- **Limits and audit.** A per-site rate limit applies (`AIAgentActRateLimit`, default 60/min). `actions.log` records time, IP, action, status and result page, **never field values**.
- **Read-only mode.** `AIAgentActions read-only` only allows actions that look things up; `off` disables actions entirely.

### Limits

- Buttons that only run JavaScript are covered only when the scanner finds the API call behind them (`fetch`/`axios`/`$.post`).
- File uploads are not supported yet.
- CAPTCHAs, two-factor codes and payment pages on other websites need a person, and the agent is told so.

## Conversational AI: configure once, learn, then answer

Without an AI service, `/ask` answers by keyword search. With one, a language model answers, using only this site's content, always in English (it translates non-English pages), and with the URLs it relied on.

```apache
AIAgentAIService anthropic                     # or: openai | compatible
AIAgentAIKey     file:/etc/apache2/ai_agent.key   # chmod 600; or env:ANTHROPIC_API_KEY
#AIAgentAIModel  claude-sonnet-5-5             # required for openai/compatible
#AIAgentAIEndpoint http://127.0.0.1:11434/v1/chat/completions   # for 'compatible'
```

**Learning before answering.** Once an AI service is set, the module starts building a knowledge store for the site.

1. **It listens to traffic.** Every successful, public HTML response to an anonymous visitor (human or agent) is converted to text and stored. That includes PHP, CGI and proxied pages, so dynamic content gets learned too. Each URL is refreshed at most once an hour.
2. **It holds back until it's ready.** It only answers after `AIAgentLearnPeriod` (default 7 days) **and** `AIAgentLearnMinPages` (default 20). Until then every question gets an answer like this:

   > I don't know Bluewhite well enough yet to answer questions about it. I started learning about this site 2 days ago and have read 7 pages so far. I will start answering once I have been learning for another 5 days and have read at least 20 pages. In the meantime you can browse the site yourself...

   The response also includes a `learning` object (pages learned, earliest ready time), so agents can come back later. Answers you've written yourself with `AIAgentFAQ` are still given during learning. Pages, guides and the sitemap work from day one.
3. **Or configure it once and skip the wait.** You have three options:
   - `AIAgentActivation immediate`: answer right away, from the files and your FAQ, describes and site description.
   - `tools/prime.sh https://your-site`: walks the site once so it is learned in minutes.
   - `touch <KnowledgeDir>/ready`: switch it on whenever you're satisfied.

**Learning modes** (`AIAgentLearning`):

| Mode | Behaviour |
|---|---|
| `always` (default) | Keeps learning forever. Pages are refreshed at most hourly as visitors view them, new pages are added, and pages that start returning 404/410 are forgotten. |
| `until-ready` | Learns until the AI switches on, then freezes the knowledge store. |
| `off` | Never learns from traffic. The AI uses the files, your FAQ and describes, and anything you primed. |

**Conversations.** Each AI answer includes a `conversation_id`. If you pass it back (`?conversation=…` or `"conversation_id"` in POST JSON), follow-up questions like "and is it waterproof?" are answered in context. Conversations expire after 24 hours.

**Cost and safety controls.**
- A global rate limit (`AIAgentAIRateLimit`, default 30 calls/minute per site). Over the limit, or if the AI service fails, the agent gets the keyword answer with a note instead of an error.
- Identical questions are cached for an hour.
- The page content sent per question is capped by `AIAgentAIContext`.
- The AI is instructed to answer only from the site's content, to treat that content as data rather than instructions, and to decline off-topic requests.

**What is never learned.** Requests with cookies or `Authorization` headers, responses marked `Cache-Control: private/no-store`, non-200 responses, excluded paths, and URLs whose query strings contain sensitive-looking parameters (`token`, `password`, `email`, `session`, `code`...).

**Knowledge directory** (default `/var/lib/apache2/ai_agent/<host>/`, or `/var/lib/httpd/...`; mode 0750, owned by the Apache user):

| File | Purpose |
|---|---|
| `started` | When learning began |
| `ready` | Touch to switch the AI on early |
| `pages/*.page` | Learned pages (readable text, with the URL/title header) |
| `access.txt` | The code scan's findings with evidence (admin only) |
| `ads.log` | Ad impressions and clicks |
| `conv/*.cookies` | Server-side cookie jars of MCP sessions (24 h, private) |
| `actions.log` | Every action agents performed: time, IP, action, status, result page (no field values) |
| `questions.log` | Every question agents asked, and whether it was answered by `learning`, `ai` or `search`. Useful for spotting gaps to cover with `AIAgentFAQ` |
| `cache/`, `conv/`, `rate` | Answer cache, conversations, rate counter |

Learned pages also fill in the sitemap and keyword search, so a dynamic site gets a full sitemap once it has been visited.

## Which pages require logging in (code scan)

The module reads the site's source code. It never runs it. From the code it builds an **access map**: which URLs require a login, which are public, and which are login pages. This is how it uses the map:

- **Learning:** pages that require login are never learned, even if they return HTTP 200 with a "please log in" message. Stale copies are deleted.
- **Page guides:** each guide gets an `access` object:
  `{"requires_login": true, "enforced": "on the server", "explanation": "This page requires logging in: ..."}`.
  Links to such pages are flagged too.
- **Sitemap:** every entry has `requires_login`. Routes that only exist in code are added, such as Express and Laravel routes and React/Vue screens, so a Node or Laravel app gets a real sitemap. Login-only pages are listed without content. Admin-like areas (`/admin`, `/backoffice`, ...) are never listed.
- **The AI:** it is told which pages need login, so it can answer "you need to log in to see your invoices".

**What it recognises**

| Code | Detected as requiring login |
|---|---|
| PHP | `if (!isset($_SESSION[...])) { header('Location: ...'); exit; }` and similar (`empty($_SESSION…)`, `!is_user_logged_in()`, `!Auth::check()`, `!isLoggedIn()` …, followed by redirect/exit/die/401/403) |
| PHP | Guard calls: `require_login()`, `auth_redirect()`, `check_login()`, `denyAccessUnlessGranted()` … |
| PHP | Your own guard functions, including wrappers of wrappers. Class methods are ignored. |
| PHP | Guards pulled in through `include`/`require` (followed through several levels), and HTTP basic auth (`PHP_AUTH_USER` + `WWW-Authenticate`) |
| Laravel | `Route::get(...)->middleware('auth')`, and routes inside `Route::middleware(['auth'])->prefix('x')->group(...)` or `Route::group(['middleware' => 'auth'], ...)` |
| Express-style JS | Route middleware (`app.get('/x', ensureLoggedIn(), h)`); `router.use(auth)` before routes; routers mounted with auth (`app.use('/account', requireLogin, accountRouter)`) followed through `require`/`import`; handlers that check `req.session`/`req.user` and redirect or return 401/403 |
| Next.js | `middleware.ts` matchers when the middleware checks a token/session |
| React Router | `<Route element={<RequireAuth/>}>` layout routes and their children, elements wrapped in `ProtectedRoute`/`PrivateRoute`…, `<PrivateRoute path=…>` |
| Vue / Angular route tables | `meta: { requiresAuth: true }` (inherited by `children`), `canActivate`, `beforeEnter` guards |
| Apache | `.htaccess` with `AuthType` + `Require valid-user/user/group` |

Browser-side guards are reported as `"enforced": "in the browser"`, because the server may still send the app shell to anyone.

**Where the code is.** By default the scanner reads `DocumentRoot`. For apps whose code lives elsewhere (a Node app behind a reverse proxy, or a Laravel project above `public/`), list each folder and the URL it serves:

```apache
AIAgentSourceDir /var/www/html /
AIAgentSourceDir /srv/node-app /app        # Express app proxied at /app
AIAgentSourceDir /srv/laravel /
```

`node_modules`, `vendor`, `.git`, `dist` and `build` are skipped, and minified files are ignored. The scan is redone every `AIAgentCodeScanInterval` seconds (default 300), so code changes are picked up. In testing, 2,800 files took about 0.15 s.

**The admin report.** `<KnowledgeDir>/access.txt` lists every finding with its evidence, file and line, for example:

```
LOGIN-REQUIRED  /orders.php  [php] orders.php:2: calls login guard must_login() [lib/functions.php:5: function must_login() calls require_admin()]
LOGIN-REQUIRED  /app/account/*  [express] server.js:8: router mounted at '/account' uses middleware requireLogin
LOGIN-REQUIRED  /spa/profile  [react, browser] src/App.jsx:8: nested inside a guarded layout route
PUBLIC          /app/pricing  [express] routes/public.js:3: router.get('/pricing') has no auth middleware
```

File names and code details appear only in this private report. Agents only ever see the English explanation.

**Limits.** This is pattern-based static analysis, not execution:
- Checks done in unusual ways (a custom framework, permissions loaded from a database, guards behind several layers of classes) can be missed. Such pages show `requires_login: null` or `false`.
- **Use `AIAgentExclude` for anything sensitive.** The scanner adds protection; it doesn't replace that.
- Ruby, Python and Java aren't scanned yet.

## Configuration

All directives work at server level or inside `<VirtualHost>`. See `ai_agent.conf` for a commented example.

| Directive | Default | Purpose |
|---|---|---|
| `AIAgent On\|Off` | On | Master switch |
| `AIAgentSiteName "..."` | hostname | Site name used in English replies |
| `AIAgentSiteDescription "..."` | — | One-paragraph English description of the site |
| `AIAgentContact "..."` | — | Contact shown to agents |
| `AIAgentDefaultMode exact\|guide\|text` | exact | What detected agents get when they don't ask |
| `AIAgentUserAgent pattern...` | — | Extra case-insensitive User-Agent substrings |
| `AIAgentBuiltinUserAgents On\|Off` | On | Use the built-in agent list |
| `AIAgentDescribe /url "description" ["title"]` | — | Describe app routes that aren't files, or override a description |
| `AIAgentFAQ "keywords" "answer"` | — | Direct English answers for `/ask` (all keywords must appear in the question) |
| `AIAgentExclude /prefix...` | — | Never list, search or transform these paths |
| `AIAgentCheckAccess On\|Off` | On | Only list pages an anonymous visitor may open |
| `AIAgentLlmsTxt On\|Off` | On | Generate `/llms.txt` when none exists |
| `AIAgentAdvertise On\|Off` | On | `Link` header on every response |
| `AIAgentPrefix /path` | `/.well-known/ai` | Endpoint prefix |
| `AIAgentMaxPages n` | 500 | Pages scanned for the sitemap and `/ask` |
| `AIAgentMaxBuffer bytes` | 4 MB | Largest response transformed into guide/text |
| `AIAgentMaxText chars` | 20000 | Page text included in a guide |
| `AIAgentAIService anthropic\|openai\|compatible\|none` | none | AI service that answers `/ask` |
| `AIAgentAIKey key\|file:/path\|env:VAR` | — | API key (use `file:` or `env:` in production) |
| `AIAgentAIModel name` | claude-sonnet-5-5 (anthropic) | Model; required for openai/compatible |
| `AIAgentAIEndpoint url` | provider default | API URL; required for compatible |
| `AIAgentAITimeout s` / `AIAgentAIMaxTokens n` | 30 / 1024 | Request limits |
| `AIAgentAIRateLimit n` | 30 | AI calls per minute per site (0 = unlimited) |
| `AIAgentAIContext chars` | 24000 | Site content sent per question |
| `AIAgentActivation learn\|immediate` | learn | Wait for learning, or answer at once |
| `AIAgentLearnPeriod 7d` | 7d | Minimum learning time (s/m/h/d/w; 0 = none) |
| `AIAgentLearnMinPages n` | 20 | Minimum pages learned |
| `AIAgentLearnMaxPages n` | 2000 | Knowledge store cap |
| `AIAgentLearnTraffic On\|Off` | On with an AI service | Learn from anonymous traffic |
| `AIAgentLearnQueryStrings On\|Off` | On | Learn `?query` URLs (sensitive params skipped) |
| `AIAgentKnowledgeDir path` | see above | Where learned knowledge lives |
| `AIAgentLogQuestions On\|Off` | On | Write `questions.log` |
| `AIAgentLearning always\|until-ready\|off` | always (with an AI service) | Learning mode (replaces `AIAgentLearnTraffic`, which still works) |
| `AIAgentCodeScan On\|Off` | On | Read PHP/JS code to find login-only pages |
| `AIAgentSourceDir /dir [/url]` | DocumentRoot | Where the code lives and which URL it serves (repeatable) |
| `AIAgentCodeScanInterval s` | 300 | How often the code is re-read |
| `AIAgentActions on\|read-only\|off` | on | Let agents use the site's functionality through `/act` |
| `AIAgentActRateLimit n` | 60 | Actions per minute per site |
| `AIAgentAds On\|Off` | On | Relay page ads and agent ads in a separate "sponsored" block |
| `AIAgentAd "Advertiser" "Text" "URL" ["/path keyword …"]` | — | An ad for agent traffic (repeatable) |
| `AIAgentAdsFile path` | — | JSON list of agent ads |
| `AIAgentAdsMax n` | 2 | Agent ads per response |
| `AIAgentAdsRequired On\|Off` | Off | Ask agents to show the ads to their users |
| `AIAgentAdsTerms "…"` | — | Your own sentence explaining why (with AdsRequired) |
| `AIAgentListProtected On\|Off` | On | List login-only pages (no content) in the sitemap; admin areas never listed |

For logging, the module sets the request notes `ai-agent` and `ai-mode` and the environment variables `AI_AGENT` and `AI_MODE`:

```apache
LogFormat "%h %t \"%r\" %>s agent=%{ai-agent}n mode=%{ai-mode}n" ai_agent
CustomLog ${APACHE_LOG_DIR}/ai_agents.log ai_agent env=AI_MODE
```

## Security notes

- The sitemap and `/ask` only include pages that an anonymous request is allowed to fetch. Each page is checked with an Apache sub-request, so `Require`, auth and deny rules are honoured. Dotfiles and `cgi-bin` are never scanned. Use `AIAgentExclude` for anything else you want hidden.
- The endpoints only bypass `.htaccess` processing for their own URLs. You can still protect them with `<Location /.well-known/ai>`.
- Guide and text responses get `Vary: Accept, X-AI-Mode`, and when the view was chosen by a header they also get `Cache-Control: private`. That stops shared caches from serving JSON to humans.

## Limitations

- **Page content is quoted, not translated.** Every explanation, summary and answer is written in English. If a page is in another language (detected from `<html lang>`), its text is quoted untranslated and the reply says so. Translating it would need an LLM backend, which isn't part of this module yet.
- **Without an AI service, `/ask` is keyword search** with English answer templates plus your FAQ entries. With one, the AI picks the relevant pages using the same keyword scoring (no vector search yet), so very large sites with many similar pages may need better titles or FAQs.
- **Dynamic pages are only known once they've been visited** (or primed). Pages behind a login are never learned, by design.
- **AI calls are made inside the Apache worker thread** (with a timeout). Size your MPM thread count with that in mind if `/ask` gets heavy use.
- **There is no sitemap cache yet.** Each sitemap or `/ask` request walks the docroot (capped by `AIAgentMaxPages`). That's fine for small and medium sites. Put a cache in front of it for big ones.
- Guide and text views buffer the whole response (up to `AIAgentMaxBuffer`), so streaming pages arrive in one piece.

## Testing

The AI features were tested against a mock Anthropic/OpenAI server. The tests covered: the learning gate (time and page count), the `ready` override, immediate activation, privacy filters, learning from CGI pages behind mod_rewrite, conversations, the cache, the rate limit, fallback on AI errors, both API formats, emoji/UTF-8 in answers, and refusing to start with an incomplete config.

Actions were tested end-to-end against a PHP shop with sessions and CSRF tokens (`tests/fixtures/php-shop`, `tests/agent_shop_demo.sh`). The run covered:
- search, add to basket with options, invalid choices refused
- login failure detection, then login, and the session carried across requests
- checkout refused without `confirm`, then completed with it
- a required field caught from the PHP code
- no private content in the knowledge store, the AI context, the catalog or the sitemap
- CSRF and cross-origin rejection, IP rules applied to the agent's real IP
- 300 malformed `/act` bodies, all answered with valid JSON

`tests/smoke.sh URL` runs 11 checks: endpoints, all three modes, header negotiation, the byte-identical exact mode, `llms.txt` and 404 handling. The parser was fuzzed with 800 malformed HTML documents and produced valid JSON with no crashes.

## License

Apache License 2.0.
