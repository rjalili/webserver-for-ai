# Webserver For AI (mod_http_b and ngx_http_b_module 1.6.0): start here

**This one archive contains everything:**
- the Apache module source and configuration,
- the nginx module (`nginx/ngx_http_b_module`, built from the same source),
- the "AI Chat" browser extension (Chrome, Brave, Edge, Opera, Vivaldi, Firefox, Safari),
- tests, tools and fixtures,
- all documentation, **including the full instructions for Lovable** (`docs/LOVABLE_BRIEF.md`).

| You are… | Read |
|---|---|
| The owner, handing off the project | this file, then `docs/HANDOFF.md` |
| **Lovable** (building the distribution website) | `docs/LOVABLE_BRIEF.md`. Also upload `README.md`, `docs/PROTOCOL.md`, `docs/CHANGELOG.md`, `docs/SECURITY.md` and `extension/README.md` as site content |
| A developer or AI coding agent taking over maintenance | `docs/HANDOFF.md` (architecture, build, test, extend, roadmap) |
| Publishing the browser extension | `extension/README.md` (build, stores, Safari conversion, permissions) |
| Connecting a site to Claude / ChatGPT | the "Use from Claude, ChatGPT…" section of `README.md` (connector URL: `https://your-site/.well-known/ai/mcp`) |
| Implementing the protocol elsewhere | `docs/PROTOCOL.md`, `extension/src/chat-core.js`, `tools/chat_client.py` |
| A site owner installing it | `README.md` and `http_b.conf` (Apache), `nginx/ngx_http_b_module/README.md` (nginx) |

**Before publishing:**
1. Confirm the licence (see `LICENSE`).
2. Choose the product name and domain; replace the `{PRODUCT_NAME}`, `{DOMAIN}`, `{GITHUB_URL}` and `{CONTACT_EMAIL}` placeholders in the docs. Also change the Firefox add-on id `ai-chat@webserver-for-ai.org` in `extension/build.py` to your own domain.
3. Create the GitHub repository from this folder, and a release with the module tarball and the extension zips (`python3 extension/build.py`).
4. Give Lovable `docs/LOVABLE_BRIEF.md` plus the Markdown docs.

Note: Lovable builds the website, not the C module or the extension. Maintaining `mod_http_b.c` and `extension/` needs a developer or an AI coding agent with a Linux shell. `docs/HANDOFF.md` is written for them.
