# AI Chat: browser extension

Talk to websites instead of clicking through them. On any site that speaks the open **http-b/1** protocol (for example Apache with mod_http_b), the extension shows a chat beside the page:
- where you are, and the page text,
- **what you can do here:** real forms, generated from the site's own description of its fields,
- **where you can go:** links, with 🔒 on pages that need a login,
- a box to **ask the site anything** in plain English,
- the site's advertising, in its own clearly labelled **Sponsored** box.

It is a **hybrid**. The chat and the normal tab share your login: log in from the chat and the tab is logged in too, and the other way round. *Show in tab* opens the chat's current page in the tab. *Follow tab* restarts the chat whenever you browse to another page. Payments, deletions and password changes always show a **Please confirm** card first.

The toolbar icon shows an **AI** badge on sites that support chat. Sites announce it with `Link: <url>; rel="alternate"; type="application/vnd.ai-chat+json"`. *Known limitation:* the badge can miss the very first page loaded while the browser is starting; opening the chat always works.

## Builds

```bash
python3 build.py        # needs Python 3; Pillow only to regenerate icons
```

| Folder / zip | Browsers | Chat appears in |
|---|---|---|
| `dist/chromium`, `http-b-chromium-*.zip` | Chrome, Brave, Microsoft Edge, Opera, Vivaldi | Side panel (Opera: toolbar popup) |
| `dist/firefox`, `http-b-firefox-*.zip` | Firefox 140+, Firefox for Android 142+ | Sidebar |
| `dist/safari` | Safari 16.4+ on macOS and iOS (after conversion, below) | Toolbar popup |

## Try it locally (developer mode)

- **Chrome / Brave / Edge / Opera / Vivaldi:** open `chrome://extensions` (Brave `brave://extensions`, Edge `edge://extensions`, Opera `opera://extensions`). Turn on Developer mode, click **Load unpacked**, choose `dist/chromium`.
- **Firefox:** open `about:debugging#/runtime/this-firefox`, click **Load Temporary Add-on**, choose `dist/firefox/manifest.json`. On first use per site, the panel asks to *Allow* access (Firefox grants site access per site).
- **Safari:** see below.

## Publishing

| Store | Upload | Notes |
|---|---|---|
| Chrome Web Store | `http-b-chromium-*.zip` | One-time $5 developer fee. Justify each permission (see *Permissions*). Brave and Vivaldi users install from the Chrome Web Store. |
| Microsoft Edge Add-ons | the same zip | Free Partner Center account. |
| Opera add-ons | the same zip | addons.opera.com developer account. |
| Firefox Add-ons (AMO) | `http-b-firefox-*.zip` | Passes `web-ext lint` with 0 errors and 0 warnings. The add-on id is `ai-chat@webserver-for-ai.org`; change it to your own domain before the first upload. |
| Mac App Store / iOS App Store | the Xcode project (below) | Needs an Apple Developer account ($99/year). |

### Safari (needs a Mac with Xcode)

```bash
xcrun safari-web-extension-converter dist/safari --app-name "AI Chat" --bundle-identifier org.webserver-for-ai.aichat
```

Open the generated Xcode project, set your Team, then **Run**. In Safari, open Settings → Extensions and enable *AI Chat* (allow unsigned extensions in the Develop menu while testing). For iOS, pick the iOS target. For distribution, **Archive** and upload to App Store Connect.

## Permissions (for store reviews)

| Permission | Why |
|---|---|
| `host_permissions` http/https | Talk to the site you are visiting, with your own session, through its chat interface |
| `webRequest` | Read the `Link` response header, to show the **AI** badge on sites that offer chat (observe only, never blocking) |
| `tabs` | Know which page the chat belongs to, follow your browsing, and open the chat's page in the tab |
| `storage` | Remember the conversation per site for this browser session |
| `sidePanel` (Chromium) | Show the chat beside the page |

The extension has no servers of its own and collects nothing: it only talks to the site you are on. Server text is always inserted as text, never as HTML.

## Tests

```bash
node test/core.test.mjs http://localhost          # protocol client against a live server (shop fixture)
npm i playwright && node test/ui.test.mjs /path/to/chrome http://localhost   # the real extension in Chrome
```

Branded Google Chrome 137+ ignores `--load-extension`; use **Chrome for Testing** or Chromium for automated UI tests.

The UI test covers, in real Chrome:
- the extension loading, and the generated forms (with preselected choices)
- adding to the basket, a question answered, and logging in from the chat
- the normal tab sharing that login, and the chat following the tab
- the confirmation card, then the order placed
- card numbers masked in your own messages
- the separate *Sponsored* box

## Files

```
src/manifest  (generated per browser by build.py)
src/background.js   toolbar button behaviour per browser, discovery badge
src/panel.html/.css/.js   the chat UI (side panel, sidebar or popup)
src/chat-core.js    the http-b/1 client (no browser APIs: shared with the tests)
src/icons/          16, 32, 48, 128 px (generated by build.py)
test/               core.test.mjs (Node), ui.test.mjs (Playwright + Chrome)
```
