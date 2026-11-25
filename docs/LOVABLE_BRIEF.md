# Brief for Lovable: the Webserver For AI distribution site

Paste this whole file into Lovable as the project prompt. Upload `README.md`, `docs/PROTOCOL.md` and `docs/CHANGELOG.md` with it: the site renders them as its documentation.

This brief is part of the single project archive. Everything Lovable needs is in it: this file, plus the Markdown docs to render (README.md, docs/PROTOCOL.md, docs/CHANGELOG.md, docs/SECURITY.md, extension/README.md).

Placeholders to replace before publishing: `{PRODUCT_NAME}` (working name: **Webserver For AI**; module name `mod_http_b`), `{DOMAIN}`, `{GITHUB_URL}`, `{CONTACT_EMAIL}`, `{VERSION}` (currently 1.4.0).

---

## 1. What we are building

A marketing and distribution website for **{PRODUCT_NAME}**, an open-source module for the Apache web server. It turns any website into one that AI agents can **read, ask, use and talk to**, without changing the website.

The site must:
1. Explain the idea in 10 seconds to site owners, developers and browser/AI people.
2. Let developers install the module in minutes (copy-paste commands, a download link to GitHub releases).
3. Host the documentation: README, protocol specification and changelog, rendered from Markdown.
4. Present the **http-b/1 protocol** as an open proposal and invite implementers (other web servers, browser plugins, AI agents).
5. Collect interest: a waitlist for the browser plugin and a newsletter.

## 2. Audiences, in priority order

1. **Web developers and hosting operators** running Apache (WordPress, PHP, Laravel, Node behind Apache). They want: what it does, whether it's safe, and how to install it.
2. **Site owners / businesses**, who want AI assistants (ChatGPT, Claude, Perplexity…) to understand their site, answer about it correctly and complete purchases or bookings.
3. **AI-agent and browser builders**, who want the protocol spec and a reference client.

## 3. Core message

**Headline:** "Your website, ready for AI agents. No rebuild required."

**Sub-headline:** "One Apache module lets AI agents read every page, ask questions, and do anything a visitor can do by clicking, in plain English, while logins stay private."

**The big idea, for the "Why" section; use the owner's words:**
> Web servers spend enormous effort making pages beautiful for humans. For utility, the content type can simply be *chat*: the AI and the web server hold a conversation, and the server offers a set of resources the agent uses. Browsers will gain a plugin, and eventually native support, that turns browsing into a chat interface or a hybrid of chat and HTML.

**Five things it does,** as a feature grid with icons:

| Feature | One-liner |
|---|---|
| Read | Every page exactly as people see it, or explained in English, or as clean Markdown. |
| Discover | An English sitemap of the whole site, plus llms.txt, generated automatically. |
| Ask | Agents ask in plain English; your site answers from its own content. Its AI only switches on once it knows your site. |
| Act | Search, forms, log in, add to basket, check out: everything a person does by clicking, done for real. |
| Chat | Any page can become a conversation: "You're in the shop. You can search, add the whale widget to your basket, or check out." |
| Works with Claude & ChatGPT | Every site is also an MCP connector: paste one URL into your assistant and it can browse and act on the site. |
| Browser extension | Chat with any supporting site beside the page, in Chrome, Brave, Edge, Opera, Firefox and Safari. |
| Fair to advertisers | Ads reach agents too, always clearly labelled "Sponsored" and never mixed into answers. |

**Trust section,** because it's critical for adoption:
- Logins are never bypassed. Agents act with their own session, like a browser.
- Nothing behind a login is ever learned, cached, listed or shown to an AI. The module even reads your PHP and JavaScript code to know which pages need a login.
- Payments, deletions and password changes always need explicit confirmation.
- Agents keep their real IP address, so your IP rules still apply.
- Cross-site request forgery protection is built in.
- Audit logs record what was done, never what was typed.
- Advertising is passed on honestly: labelled "Sponsored", kept out of content and AI answers, with counted impressions and clicks.
- Open source, runs on your own server, works with your choice of AI provider (Anthropic, OpenAI or any compatible API) or none at all.

## 4. Site map

1. **Home** (`/`)
   - hero
   - an animated demo conversation (section 6)
   - five-feature grid
   - "how it works" diagram (section 7)
   - trust section
   - install teaser (3 commands)
   - comparison (section 8)
   - call to action: Install / Read the protocol / Join the plugin waitlist
2. **How it works** (`/how-it-works`): a longer explanation of the five layers with diagrams, the learning gate ("I don't know this site well enough yet"), and login detection.
3. **Install** (`/install`): tabs for Ubuntu/Debian, RHEL/Fedora and Docker (coming soon). Copy buttons on every command. A minimal configuration block and a "verify it works" section with curl commands. Commands are in section 9.
4. **Docs** (`/docs`): README.md rendered with an auto-generated sidebar table of contents.
5. **Protocol** (`/protocol`): docs/PROTOCOL.md rendered. A banner: "http-b/1: an open proposal. Implement it, comment on it."
6. **Browser extension** (`/extension`): "Browse by talking". A screenshot-style mock-up of the side panel beside a shop page, showing generated forms, a "Please confirm" card and a dashed "Sponsored" box (the panel design is in extension/src/panel.css: reuse its colours).
   - Store buttons: Chrome Web Store, Firefox Add-ons, Edge, Opera, and the App Store for Safari. Point them at `{GITHUB_URL}/releases` until store listings exist.
   - Render extension/README.md below the buttons.
   - Status: "Beta. Install from the store or load it yourself."
7. **Use with AI assistants** (`/assistants`): how to add a site as a connector in Claude or ChatGPT (paste `https://your-site/.well-known/ai/mcp`), the five tools, and how sessions stay private. Content: the "Use from Claude, ChatGPT…" section of README.md.
8. **For advertisers & publishers** (`/ads`): how ads are respected and labelled, agent ads, measurement. Content: the "Advertising" section of README.md.
9. **Changelog** (`/changelog`): docs/CHANGELOG.md rendered.
10. **Security** (`/security`): docs/SECURITY.md rendered.
11. **Community** (`/community`): GitHub link, how to contribute, "implement the protocol" invitation, contact.
10. Footer on every page: GitHub, licence, contact, "This site speaks http-b" badge (once hosted on Apache with the module).

## 5. Design direction

- Clean and technical, but friendly: think Stripe docs crossed with Linear.
- Light mode by default with a dark mode toggle.
- One accent colour (a deep ocean blue suits the owner's "Bluewhite" brand), generous whitespace, and a monospace font for code.
- Code blocks with copy buttons and syntax highlighting (bash, apache config, JSON).
- Show **real JSON** from the protocol. Developers trust real payloads.
- Accessible (WCAG AA), fast, fully responsive, nothing heavy above the fold.
- No stock photos. Use diagrams, terminal snippets and the chat demo.

## 6. Home-page demo conversation

An animated chat panel, typed out turn by turn, looping. Use these real exchanges from the test shop:

```
Agent → GET /shop/  (Accept: application/vnd.ai-chat+json)
Site  → You are at "Bluewhite shop". Here you can "Search", "Add to basket: Whale widget - 29 EUR"
        or "Add to basket: Dolphin gadget - 19 EUR".
Agent → do: Add to basket: Whale widget, Quantity 2
Site  → Done. The site showed "Your basket": 2 x Whale widget.
Agent → go: /shop/checkout.php
Site  → This page needs you to log in. Here you can "Log in".
Agent → do: Log in (username, password)
Site  → Logged in. You're on "My account".
Agent → do: Place order
Site  → "Place order" has real consequences. Ask your user to confirm.
Agent → do: Place order, confirm: true
Site  → Thank you! Your order #1002 is confirmed.
```

Next to it, a toggle: **"What a human sees"** (a simple mock shop page) vs **"What an agent sees"** (the JSON turn).

## 7. "How it works" diagram

```
Browser (human) ──HTML──▶ ┐
AI agent ──Accept: chat──▶ ├─ Apache + {PRODUCT_NAME} ─▶ your site (PHP, WordPress, Node, static…) unchanged
Browser plugin ──chat────▶ ┘        │
                                    ├─ learns public pages (never anything behind a login)
                                    ├─ reads your code to find login-only pages
                                    └─ optional: your AI provider for open questions
```

## 8. Comparison table ("How is this different?")

Keep the tone neutral and factual.

| | {PRODUCT_NAME} | Markdown-for-agents (CDNs, plugins) | NLWeb | WebMCP |
|---|---|---|---|---|
| Works without changing the site | ✔ | ✔ | needs data ingestion | needs code in the site |
| Readable pages for agents | ✔ | ✔ | — | — |
| Q&A in English | ✔ | — | ✔ | — |
| Do actions (forms, login, checkout) | ✔ server-side | — | — | ✔ in the browser |
| Conversation on any URL | ✔ http-b/1 | — | — | — |
| Usable from Claude / ChatGPT (MCP) | ✔ built in | — | ✔ | — |
| Browser extension | ✔ all major browsers | — | — | native (Chrome preview) |
| Ads relayed and labelled | ✔ | — | — | — |
| Knows which pages need login | ✔ reads PHP/JS code | — | — | — |
| Runs on your own server | ✔ Apache module | CDN / CMS | separate service | browser |

Add one line under the table: "We plan to publish our action catalog as WebMCP tools, so these approaches work together."

## 9. Install page content

```bash
# Ubuntu / Debian
sudo apt install apache2-dev libcurl4-openssl-dev
tar xzf mod_http_b-{VERSION}.tar.gz && cd mod_http_b
make && sudo make enable-debian

# RHEL / Fedora
sudo dnf install httpd-devel libcurl-devel
make && sudo make install && sudo systemctl reload httpd
```

Minimal configuration (in your VirtualHost):

```apache
AIAgentSiteName        "Your Company"
AIAgentSiteDescription "One paragraph about what your site offers."
# Optional: answers from your own AI provider, after it has learned your site
AIAgentAIService anthropic
AIAgentAIKey     file:/etc/apache2/http_b.key
```

Verify:

```bash
curl https://your-site/.well-known/ai
curl -H "Accept: application/vnd.ai-chat+json" https://your-site/
curl https://your-site/.well-known/ai/actions
```

## 10. Technical requirements for the Lovable build

- React + TypeScript + Tailwind (Lovable defaults are fine).
- Docs pages render the uploaded Markdown files at build time or from `/content/*.md`, with heading anchors, a table of contents and copy buttons on code.
- Download buttons link to `{GITHUB_URL}/releases/latest`. Don't host binaries on the site.
- Extension downloads: link to the release assets `http-b-chromium-*.zip`, `http-b-firefox-*.zip` and the store listings when they exist.
- Waitlist and newsletter forms: store in Supabase (Lovable's built-in backend). Fields: email, role (site owner / developer / AI or browser builder), optional website URL. Double opt-in email and GDPR consent text (the owner is EU-based). No tracking without consent.
- SEO: per-page titles and descriptions, an Open Graph image, sitemap.xml and robots.txt.
- **Dogfooding:** serve `/llms.txt` and `.md` versions of the docs pages. When the site is later hosted behind Apache with the module, it will speak ai-chat natively.
- Performance target: Lighthouse 95+ on all four scores.

## 11. Copy snippets

- **Footer tagline:** "Make the web conversational: for people, for agents, for browsers."
- **Plugin page headline:** "Browse by talking."
- **Plugin page body:** "Every site running {PRODUCT_NAME} can become a conversation. Our browser plugin will let you switch any page between the normal view, a chat, or both side by side, with your own logins and your own choices."
- **Security page intro:** "An AI interface must never become a back door. Here is exactly how {PRODUCT_NAME} keeps private things private."
- **Call to action for implementers:** "http-b/1 is open. Build a client, a server or a browser feature — and tell us."
