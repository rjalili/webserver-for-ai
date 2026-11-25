# Changelog

## 1.6.0 — 2026-11-25
- **nginx support.** `nginx/ngx_http_b_module` brings every feature to nginx: modes, discovery, sitemap and `llms.txt`, ask (keyword and AI), learning, the code scanner and access map, actions and `/act`, chat on any URL, MCP, sponsored answers. It compiles `mod_http_b.c` itself through a small Apache-API layer on APR, so both servers share one implementation. Endpoints run in nginx's thread pool; requests the module makes to its own server carry the agent's address, which nginx applies like the realip module. Every Apache directive `AIAgentXyz` is the nginx directive `http_b_xyz`. All repository tests pass on nginx 1.26.2 (smoke, shop walkthrough, chat, MCP, extension core).
- **New names.** The protocol is now **http-b/1** (was ai-chat/1) and the Apache module is **mod_http_b** (`mod_http_b.c`, `http_b_module`, `http_b.conf`; was mod_ai_agent). Directive names, endpoints, headers and media types are unchanged. Extension packages are now `http-b-<browser>-<version>.zip`.

## 1.5.0 — 2026-11-19
- **Advertising, respected and delineated.** Ads on pages (ad containers, `rel="sponsored"` links) are taken out of the content and relayed in a separate `sponsored` block, never learned and never given to the AI. Site owners can add agent ads (`AIAgentAd`, `AIAgentAdsFile`), targeted by path or keyword, with weighted rotation, `AIAgentAdsRequired` / `AIAgentAdsTerms`, impression and click logging (`ads.log`), and a click redirect limited to configured destinations. The AI is instructed never to put ads in answers.
- **MCP server** at `/.well-known/ai/mcp`, so Claude, ChatGPT and other assistants can use any site as a connector. Tools: `site_overview`, `open_page`, `ask`, `list_actions`, `do_action`. A server-side cookie jar per `Mcp-Session-Id` keeps logins working across calls. Tested with the official MCP Python SDK.
- **Browser extension "AI Chat"** for Chrome, Brave, Edge, Opera, Vivaldi, Firefox and Safari:
  - side panel or sidebar or popup, with forms built from field descriptions
  - a confirmation card, a labelled Sponsored box, and secrets masked in your own messages
  - shared login with the tab, and a chat that follows your browsing
  - tested in real Chrome; the Firefox build lints clean
- The knowledge directory is now created whenever actions are on, so chat and MCP sessions work on sites without an AI service.
- `chat_core` refactor shared by chat and MCP.


## 1.4.0 — 2026-11-19
- **Chat on any URL (ai-chat/1).** `Accept: application/vnd.ai-chat+json` or `?ai=chat` turns any page into a conversation. Each server turn says where the agent is, what it can do (`you_can`, with fields) and where it can go (`you_can_go`). The agent replies with `say` / `do` / `go`. The server keeps the location; the client keeps the cookies.
- Every HTML page advertises its chat version: `Link: <url>; rel="alternate"; type="application/vnd.ai-chat+json"`.
- Browser-extension origins (`chrome-extension://`, `moz-extension://`, `safari-web-extension://`) are accepted, ready for the browser plugin.
- `/act` refactored into a shared engine (`act_core`) used by both `/act` and chat.
- Fixed: the cached action catalog could point to freed request memory after a cache refresh.
- Added `tools/chat_client.py` (reference client), `tests/mock_ai.py` (mock AI provider), and docs/PROTOCOL.md, HANDOFF.md, LOVABLE_BRIEF.md.

## 1.3.0 — 2026-11-17
- **Actions:** `/actions` catalog and `/act`. Agents do what a person does by clicking: forms opened fresh with the agent's own session, CSRF tokens, defaults, chosen button, redirects followed, cookies returned.
- Functionality learned from public forms, PHP handlers (`$_POST`, required fields), Express/Laravel POST routes, and `fetch`/`axios`/`$.post` calls.
- Gates: `needs_login`, `needs_confirmation` (payments, deletions, password changes), `invalid_fields`, read-only mode.
- Security: CSRF hardening, the agent's real IP kept on internal requests, `actions.log` without field values, rate limit.
- Fixed: a PHP guard false positive (the guard consequence must be inside the `if` body); PHP's `no-store` no longer blocks learning of anonymous pages.

## 1.2.0 — 2026-11-17
- Learning modes: `always` / `until-ready` / `off`. Pages that disappear are forgotten.
- Code scan (PHP, Laravel, Express, React, Vue/Angular, Next.js, .htaccess) to find which pages require login. Used for learning, guides, sitemap and the AI. Admin report in `access.txt`.

## 1.1.0 — 2026-11-17
- AI service (Anthropic / OpenAI / OpenAI-compatible), configured once.
- Learning gate: "I don't know this site well enough yet" until the learning period and page minimum are met. `ready` file override, `immediate` activation.
- Conversations, answer cache, rate limit, fallback on AI failure, `tools/prime.sh`.

## 1.0.0 — 2026-11-17
- First version: agent detection, exact / guide / text modes, English sitemap, `/ask` keyword answers, `/llms.txt`, access-checked crawl.
