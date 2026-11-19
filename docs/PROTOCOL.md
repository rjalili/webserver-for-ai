# Webserver For AI — Protocol Specification

Version: **ai-chat/1**, implemented by mod_ai_agent 1.5.0.
Status: draft, proposed for wider adoption. Media type in use: `application/vnd.ai-chat+json` (vendor tree, unregistered).

This document specifies how an AI agent (or a browser plugin acting for a person) uses a website through a web server that implements this protocol. The protocol has **five layers**. A server may implement any subset; mod_ai_agent implements all five.

| Layer | What it gives an agent | Entry point |
|---|---|---|
| 1. Exact | The page exactly as a human's browser receives it | any URL (default), `?ai=exact` |
| 2. Read | The page explained in English (JSON) or as Markdown | `?ai=guide`, `?ai=text`, `Accept` negotiation |
| 3. Discover | The site described in English; what exists | `/.well-known/ai`, `/.well-known/ai/sitemap.json`, `/llms.txt` |
| 4. Ask | Plain-English questions, plain-English answers | `/.well-known/ai/ask` |
| 5. Act / Chat | Doing what a person does by clicking, as a conversation | `/.well-known/ai/actions`, `/.well-known/ai/act`, **any URL with `Accept: application/vnd.ai-chat+json`** |

All responses meant for agents are in **English**. Content quoted from a page keeps its original language and is flagged (`page_language`).

---

## 0. Discovery

Every HTML response carries:

```
Link: </.well-known/ai>; rel="ai-agent"; type="application/json",
      </.well-known/ai/sitemap.json>; rel="ai-sitemap"; type="application/json",
      </current/path>; rel="alternate"; type="application/vnd.ai-chat+json"
```

A client that sees `rel="alternate"; type="application/vnd.ai-chat+json"` knows it can switch the current page into a conversation. A browser plugin should use this header to show a "chat with this site" control.

`GET /.well-known/ai` returns a welcome document: the site's name and description, the list of ways to talk to it, the status of the conversational AI (`keyword-search`, `learning`, `ready`), and a summary of login requirements.

## 1. Modes on any URL

| Mode | Request | Response |
|---|---|---|
| exact | default, `?ai=exact`, `X-AI-Mode: exact` | Unmodified response; agents also get `X-AI-Agent-Help` |
| guide | `?ai=guide`, `X-AI-Mode: guide`, `Accept: application/vnd.ai+json` | `application/json` page guide (below) |
| text | `?ai=text`, `X-AI-Mode: text`, `Accept: text/markdown` | `text/markdown` with an English preface |
| chat | `?ai=chat`, `Accept: application/vnd.ai-chat+json`, or a POST with that Content-Type | ai-chat/1 turn (section 5) |

The `ai` query parameter is removed before the application sees the request.
Guide/text responses send `Vary: Accept, X-AI-Mode`; when chosen by header, also `Cache-Control: private, no-cache`.

### Page guide (excerpt)

```json
{
  "type": "page_guide", "language": "en", "url": "/contact.html", "status": 200,
  "summary": "This page of Bluewhite is titled \"Contact\". ...",
  "title": "Contact", "description": null, "page_language": "en",
  "access": {"requires_login": false, "is_login_page": false, "enforced": null, "explanation": "..."},
  "outline": [{"level": 1, "heading": "Contact us"}],
  "forms": [{"explanation": "Form #1 sends a POST request to /send.php. It asks for: ...",
             "action_id": "0442192024d7", "method": "POST", "action": "/send.php", "fields": [...]}],
  "links": [{"text": "About us", "url": "/about.html", "kind": "internal", "requires_login": false, "about": "..."}],
  "images": [{"src": "/img/whale.png", "alt": "Whale widget"}],
  "text": "# Contact us\n...", "text_truncated": false,
  "next_steps": ["..."]
}
```

## 2. Site description

`GET /.well-known/ai/sitemap.json`: every page with `url`, `title`, English `description`, `kind`, `page_language`, `requires_login`, `access`, and the `guide`/`text` URLs. Pages that require login are listed without content. Admin areas are never listed.

`GET /llms.txt` (when the site has none): the same in llmstxt.org Markdown form.

## 3. Ask

`GET /.well-known/ai/ask?q=...&conversation=<id>` or `POST` JSON `{"question": "...", "conversation_id": "..."}`.

```json
{"type": "answer", "language": "en", "mode": "ai|search|learning", "question": "...", "answer": "...",
 "conversation_id": "...", "results": [...], "suggested_actions": [...], "learning": {...}, "follow_up": [...]}
```

`mode: "learning"` means the server's AI is still learning the site and answers only with facts the owner configured, plus *"I don't know this site well enough yet…"* and the earliest ready time.

## 4. Actions

### Catalog

`GET /.well-known/ai/actions`, `GET /.well-known/ai/actions/<id>`

```json
{"id": "b3c3550324e3", "name": "Add to basket: Whale widget - 29 EUR", "kind": "add to basket",
 "description": "...", "requires_login": false, "needs_confirmation": false,
 "method": "POST", "url": "/shop/cart.php", "learned_from": "form on a page", "found_on": ["/shop/"],
 "fields": [{"name": "qty", "label": "Quantity", "expects": "one of: \"1\", \"2\", \"3\"", "required": false,
             "choices": ["1", "2", "3"]}],
 "automatic_fields": 2}
```

Kinds: `search`, `log in`, `sign up`, `change password`*, `purchase / payment`*, `delete / cancel`*, `add to basket`, `newsletter sign-up`, `send a message`, `upload a file`, `look something up`, `submit information`. Kinds marked * need confirmation.

Action ids are stable: a hash of the method, the URL path, the field names, and any hidden values that identify the item (such as a product id).

### Execution

`POST /.well-known/ai/act` with `Content-Type: application/json`:

| Body | Meaning |
|---|---|
| `{"action": "<id>", "fields": {...}, "button": "...", "confirm": true}` | Do a catalog action |
| `{"page": "/path", "form": 1, "fields": {...}}` | Use form n of a page (including pages only visible after login) |
| `{"open": "/path"}` (alias `click`) | Follow a link |

Field values can be keyed by field name or label. A choice can be given as its value or its visible text. Checkboxes take `true`/`false`, and multi-selects take arrays.

Semantics: the server opens the page with the agent's cookies, fills in the form like a browser (hidden fields, CSRF tokens, defaults, checked boxes, selected options, the clicked button), submits it through the real site, and follows same-site redirects. Redirects to other sites are reported, never followed.

Response (`Cache-Control: no-store`; cookies the site set are sent as `Set-Cookie` and also listed in `cookies`):

```json
{"type": "action_result", "action": {...}, "done": true, "http_status": 200, "result_url": "/shop/cart.php?added=1",
 "external_redirect": null, "page": {"title": "...", "summary": "...", "text": "...", "forms": [...], "links": [...]},
 "cookies": {"PHPSESSID": "..."}, "message": "I submitted ... The site redirected to ...", "next_steps": [...]}
```

Problems (`done: false`, nothing was submitted): `needs_login` (with `login_actions`), `needs_confirmation`, `invalid_fields`, `unknown_action`, `form_not_found`, `read_only`, `busy` (429), `forbidden` (403), `bad_request`, `unreachable` (502).

## 5. Chat — ai-chat/1

The conversational form of everything above. Any URL can be the start.

### Opening

```
GET /shop/  Accept: application/vnd.ai-chat+json
```

### Server turn

```json
{
  "type": "chat", "protocol": "ai-chat/1", "language": "en",
  "conversation": "12ae832c506473f8bea690f28020afd6", "turn": 1,
  "at": {"url": "/shop/", "title": "Bluewhite shop", "requires_login": false},
  "message": "You are at \"Bluewhite shop\" (/shop/). Here you can \"Search\", \"Add to basket: Whale widget - 29 EUR\" ...",
  "content": "# Shop\n...readable text of the page...",
  "you_can": [{"id": "...", "name": "Search", "kind": "search", "fields": [...], "do": {"form": 1}}],
  "you_can_go": [{"text": "Basket", "go": "/shop/cart.php"}, {"text": "My account", "go": "/shop/account.php", "requires_login": true}],
  "also_on_this_site": [{"id": "c767b930b6d7", "name": "Log in", "kind": "log in", "do": {"action": "c767b930b6d7"}}],
  "result": {"done": true, "problem": null, "http_status": 200, "external_redirect": null, "cookies": {...}},
  "answer": {"question": "...", "text": "...", "answered_by": "ai|search|learning", "related_pages": [...], "suggested_actions": [...]},
  "how_to_reply": "POST to /shop/ ..."
}
```

`result` is present after a `do` or `go` (or when there is a problem); `answer` is present after a `say`.

### Agent turn

`POST <current url>` or `POST /.well-known/ai/chat`, with `Content-Type: application/vnd.ai-chat+json`. Send exactly one of `say`, `do` or `go`:

```json
{"conversation": "12ae…", "say": "do you ship to Canada?"}
{"conversation": "12ae…", "do": {"form": 2, "fields": {"Quantity": "2"}}}
{"conversation": "12ae…", "do": {"action": "c767b930b6d7", "fields": {"Username": "...", "Password": "..."}}}
{"conversation": "12ae…", "do": {"form": 1, "fields": {...}, "confirm": true}}
{"conversation": "12ae…", "go": "/shop/checkout.php"}
```

Optional: `"at": "/path"`, to state the location explicitly (useful for stateless clients).

### State

- **The server keeps** the location and turn count for each conversation (24-hour lifetime). Questions in the same conversation share context.
- **The client keeps** the site's cookies (the session). A client must store `Set-Cookie` headers and send them back, as a browser does.
- **No server AI is needed** to run the conversation. `say` uses the site's AI if one is configured and ready, and keyword search otherwise.

### Design principle

**English for intent and explanation; exact structure for action.** Every server turn tells the client, in English, where it is and what happened, and gives it the exact actions (with fields) available next. The client's own intelligence chooses. The server never has to interpret free text to perform an action.

## 6. Advertising

Any response may carry a `sponsored` object. Implementations must keep advertising **out of** `message`, `content`, `text` and `answer`, and deliver it only here:

```json
"sponsored": {
  "notice": "Advertising shown by <site>. If you pass it on to your user, label it clearly as an advertisement ...",
  "show_to_user": "requested by the site" | "optional",
  "items": [{"label": "Sponsored", "from": "the site's sponsors" | "an advertisement on this page" | "a sponsored link inside this page's content",
             "id": "9f12e0edb3", "advertiser": "SeaCo", "text": "Charter a sailing boat...", "link": "https://site/.well-known/ai/ad/9f12e0edb3",
             "destination": "https://seaco.example/charter"}]
}
```

**What a client should do.** If it shows the ads, it labels them as advertising, keeps them visually and textually separate, leaves the text unchanged, and never presents them as the site's answer or its own recommendation. When `show_to_user` is `"requested by the site"`, the site asks clients to show them in exchange for access to its content. Ad links (`/.well-known/ai/ad/<id>`) count the click and redirect only to the configured destination.

In Markdown views, ads appear as a final quoted block starting with **Sponsored**.

## 7. MCP binding

The same capabilities are exposed as a Model Context Protocol server at `/.well-known/ai/mcp`: Streamable HTTP transport, JSON responses, protocol versions 2025-06-18, 2025-03-26 and 2024-11-05.

| Tool | Arguments | Maps to |
|---|---|---|
| `site_overview` | — | welcome + sitemap + action catalog, as text |
| `open_page` | `url` | chat `go` |
| `ask` | `question` | chat `say` |
| `list_actions` | — | `/actions` |
| `do_action` | `form` or `action`, `fields`, `button`, `confirm` | chat `do` |

Every tool result is the English `message` followed by the full ai-chat turn (JSON). The `Mcp-Session-Id` is created at `initialize`; the server keeps the site's cookies and the location per session (24 hours). `DELETE` ends the session and removes them. No Set-Cookie is ever sent to MCP clients.

## 8. Security requirements (for any implementation)

1. **Never bypass the site's authentication.** Actions run as the client, with the client's own cookies and credentials.
2. **Never learn, cache, list, or give an AI anything that requires login.** Learn only from requests without cookies or Authorization headers.
3. **Consequential actions** (payments, deletions, password changes) require `"confirm": true`.
4. **CSRF:** state-changing endpoints accept only JSON content types, which forces a CORS preflight that is never granted. Reject `Sec-Fetch-Site: cross-site|same-site` and foreign `Origin` values. Browser-extension origins (`chrome-extension://`, `moz-extension://`, `safari-web-extension://`) are allowed, because web pages cannot forge them.
5. **Identity is preserved.** Requests made on a client's behalf must carry the client's real IP address into authorization and logging.
6. **Never log field values.**
7. **Keep advertising separate** from content and answers (section 6), and never let ads reach the AI as site facts.
8. **MCP sessions:** cookie jars are private to one random session id, are only sent to the same site, and expire.

## 9. Headers summary

| Header | Direction | Purpose |
|---|---|---|
| `Accept: application/vnd.ai-chat+json` | request | Chat mode on any URL |
| `Accept: application/vnd.ai+json` / `text/markdown` | request | Guide / text mode |
| `X-AI-Mode: exact\|guide\|text\|chat` | request | Explicit mode |
| `X-AI-Agent: <name>` | request | Identify as an agent (recommended for agents and plugins) |
| `Link: …rel="alternate"; type="application/vnd.ai-chat+json"` | response | This page can be a conversation |
| `X-AI-Agent-Help` | response | Pointer to the agent interface |
| `X-AI-Mode` | response | Which mode was served |
