// AI Chat panel: turns the current site into a conversation (ai-chat/1).
// Security: server text is only ever inserted with textContent, never as HTML.
import { AiChat } from "./chat-core.js";

const api = globalThis.browser ?? globalThis.chrome;
const $ = (id) => document.getElementById(id);
const log = $("log");

let chat = null;       // AiChat for the current site
let tabId = null;      // the tab we are attached to
let pendingDo = null;  // last "do" request, to repeat with confirm=true

// ---------- small DOM helpers (textContent only) ----------
function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text != null) e.textContent = text;
  return e;
}
function add(node) { log.appendChild(node); log.scrollTop = log.scrollHeight; return node; }
function say(text, who = "site") { return add(el("div", "msg " + who, text)); }
function busy(on) { for (const b of document.querySelectorAll("button, #q")) b.disabled = on; }

// ---------- talking to the site ----------
async function run(fn, youText) {
  if (youText) say(youText, "you");
  busy(true);
  try {
    render(await fn());
  } catch (e) {
    say(e.notSupported
      ? "This site doesn't offer an AI chat interface yet. (It needs a web server with the ai-chat protocol, such as Apache with mod_ai_agent.)"
      : "Couldn't reach the site: " + e.message, "error");
  } finally {
    busy(false);
    $("q").focus();
  }
}

// panel.html?tab=<id> attaches to a given tab (used when the chat is popped out into its own window).
const pinnedTab = Number(new URLSearchParams(location.search).get("tab")) || null;

async function attachToActiveTab(force = false) {
  const tab = pinnedTab ? await api.tabs.get(pinnedTab).catch(() => null)
                        : (await api.tabs.query({ active: true, currentWindow: true }))[0];
  if (!tab || !/^https?:/.test(tab.url || "")) {
    tabId = tab?.id ?? null;
    $("title").textContent = "AI Chat";
    $("url").textContent = "Open a website to chat with it.";
    return;
  }
  const u = new URL(tab.url);
  const path = u.pathname + u.search;
  if (!force && chat && chat.origin === u.origin && chat.at === path && tabId === tab.id) return;
  tabId = tab.id;
  if (!(await hasSiteAccess(u.origin))) {
    log.textContent = "";
    $("url").textContent = u.origin;
    const c = add(el("div", "card"));
    c.appendChild(el("div", null, "Allow AI Chat to talk to " + u.host + "? It will use this site's chat interface with your own session on the site, as your browser would."));
    const b = c.appendChild(el("button", null, "Allow for " + u.host));
    b.onclick = async () => { if (await api.permissions.request({ origins: [u.origin + "/*"] })) attachToActiveTab(true); };
    return;
  }
  const saved = await loadConversation(u.origin);
  chat = new AiChat(u.origin, { conversation: saved });
  log.textContent = "";
  $("url").textContent = u.origin + path;
  await run(() => chat.start(path));
}

// Host access: granted at install in Chromium; Firefox (and cautious users) grant it per site.
async function hasSiteAccess(origin) {
  try { return await api.permissions.contains({ origins: [origin + "/*"] }); } catch { return true; }
}

// Keep one conversation per site for this browser session.
async function loadConversation(origin) {
  try { return (await api.storage.session.get(origin))[origin] || null; } catch { return null; }
}
async function saveConversation() {
  if (!chat?.conversation) return;
  try { await api.storage.session.set({ [chat.origin]: chat.conversation }); } catch { /* not available */ }
}

// ---------- rendering a server turn ----------
function render(t) {
  if (!t || t.type !== "chat") { say(t?.message || "Unexpected answer from the site.", "error"); return; }
  saveConversation();
  $("title").textContent = t.at?.title || "AI Chat";
  $("url").textContent = chat.origin + (t.at?.url || "");

  // the site's own words
  const answer = t.answer?.text;
  say(answer || t.message || "");

  // what went wrong, if anything, and how to continue
  const p = t.result?.problem;
  if (p === "needs_confirmation" && pendingDo) {
    const c = add(el("div", "card warn"));
    c.appendChild(el("h3", null, "Please confirm"));
    c.appendChild(el("div", null, "This has real consequences (for example a payment). Do you want to go ahead?"));
    const row = c.appendChild(el("div", "chips"));
    const yes = row.appendChild(el("button", null, "Yes, go ahead"));
    const no = row.appendChild(el("button", "ghost", "Cancel"));
    const req = pendingDo;
    yes.onclick = () => { c.remove(); pendingDo = null; run(() => chat.do({ ...req, confirm: true }), "Confirmed."); };
    no.onclick = () => { c.remove(); pendingDo = null; say("Cancelled. Nothing was sent.", "site"); };
  }
  if (p === "needs_login") say("You need to log in to this site first. Use its log-in form below.", "error");

  // the page itself (hybrid: read it here, or show it in the tab)
  if (t.content) {
    const d = add(el("details", "card"));
    d.appendChild(el("summary", null, "Page text: " + (t.at?.title || t.at?.url || "")));
    d.appendChild(el("div", "content", t.content));
  }

  // what you can do here: real forms built from the field descriptions
  if (t.you_can?.length) {
    const box = add(el("div"));
    box.appendChild(el("div", "section-title", "You can do here"));
    for (const a of t.you_can) box.appendChild(actionForm(a, a.do || {}));
  }

  // where you can go
  if (t.you_can_go?.length) {
    const box = add(el("div"));
    box.appendChild(el("div", "section-title", "Go to"));
    const chips = box.appendChild(el("div", "chips"));
    for (const l of t.you_can_go) {
      const b = chips.appendChild(el("button", "chip" + (l.requires_login ? " locked" : ""), l.text || l.go));
      b.title = l.go + (l.requires_login ? " (requires login)" : "");
      b.onclick = () => run(() => chat.go(l.go), "Go to " + (l.text || l.go));
    }
  }

  // elsewhere on the site
  if (t.also_on_this_site?.length) {
    const d = add(el("details"));
    d.appendChild(el("summary", null, "More things this site can do (" + t.also_on_this_site.length + ")"));
    const chips = d.appendChild(el("div", "chips"));
    for (const a of t.also_on_this_site) {
      const b = chips.appendChild(el("button", "chip" + (a.requires_login ? " locked" : ""), a.name));
      b.onclick = async () => {
        const full = await chat.describeAction(a.id);
        if (full) add(actionForm(full, { action: a.id }));
      };
    }
  }

  // advertising: shown as the site asks, but always clearly separated and labelled
  if (t.sponsored?.items?.length) add(sponsoredBox(t.sponsored));
}

function actionForm(a, target) {
  const card = el("div", "card");
  card.appendChild(el("h3", null, a.name + (a.needs_confirmation ? "  (asks for confirmation)" : "")));
  const f = card.appendChild(el("form", "action"));
  const inputs = [];
  for (const fd of a.fields || []) {
    const isCheck = /checkbox/.test(fd.expects) && !(fd.choices && fd.choices.length > 1);
    const lab = el("label", isCheck ? "check" : null);
    const caption = el("span", fd.required ? "req" : null, fd.label || fd.name);
    let input;
    if (fd.choices && fd.choices.length) {
      input = el("select");                       // like a browser: first choice preselected
      for (const ch of fd.choices) input.appendChild(el("option", null, ch));
    } else if (isCheck) {
      input = el("input"); input.type = "checkbox";
    } else if (/multi-line/.test(fd.expects)) {
      input = el("textarea"); input.rows = 3;
    } else {
      input = el("input");
      input.type = /password/.test(fd.expects) ? "password" : /email/.test(fd.expects) ? "email" : "text";
      if (fd.default) input.value = fd.default;
    }
    input.required = !!fd.required && !isCheck;
    if (isCheck) { lab.appendChild(input); lab.appendChild(caption); } else { lab.appendChild(caption); lab.appendChild(input); }
    f.appendChild(lab);
    inputs.push([fd, input, isCheck]);
  }
  const submit = f.appendChild(el("button", null, a.name.length < 40 ? a.name : "Submit"));
  submit.type = "submit";
  f.onsubmit = (ev) => {
    ev.preventDefault();
    const fields = {};
    for (const [fd, input, isCheck] of inputs) {
      if (isCheck) fields[fd.name] = input.checked;
      else if (input.value !== "") fields[fd.name] = input.value;
    }
    const req = { ...target, fields };
    pendingDo = req;
    // echo what was sent, but never secrets (passwords, card numbers, codes)
    const secret = (fd) => /password/.test(fd.expects) || /card|cvv|cvc|iban|pin\b|secret|ssn|security.?code|token/i.test(fd.name + " " + (fd.label || ""));
    const shown = inputs.filter(([fd]) => fields[fd.name] !== undefined)
      .map(([fd]) => `${fd.label || fd.name}: ${secret(fd) ? "••••" : fields[fd.name]}`).join(", ");
    run(() => chat.do(req), a.name + (shown ? " (" + shown + ")" : ""));
  };
  return card;
}

function sponsoredBox(sp) {
  const box = el("aside", "sponsored");
  box.setAttribute("aria-label", "Advertising");
  box.appendChild(el("div", "badge", "Sponsored"));
  for (const it of sp.items) {
    const row = box.appendChild(el("div", "item"));
    if (it.advertiser) row.appendChild(el("span", "who", it.advertiser + ": "));
    row.appendChild(el("span", null, it.text || "Advertisement"));
    if (it.link) {
      row.appendChild(document.createTextNode(" "));
      const a = row.appendChild(el("a", null, "Learn more"));
      a.href = it.link; a.target = "_blank"; a.rel = "noopener sponsored";
    }
  }
  box.appendChild(el("div", "notice", sp.notice || "Advertising, not part of the site's content."));
  return box;
}

// ---------- wiring ----------
$("ask").onsubmit = (ev) => {
  ev.preventDefault();
  const q = $("q").value.trim();
  if (!q || !chat) return;
  $("q").value = "";
  run(() => chat.say(q), q);
};

// Hybrid view: show the page the chat is on, in the normal tab.
$("show").onclick = () => { if (chat && tabId != null) api.tabs.update(tabId, { url: chat.origin + chat.at }); };

// Follow the user's browsing: switching tabs or navigating restarts the chat on that page.
api.tabs.onActivated.addListener(() => { if ($("follow").checked && !pinnedTab) attachToActiveTab(); });
api.tabs.onUpdated.addListener((id, info) => {
  if ($("follow").checked && id === tabId && info.status === "complete") attachToActiveTab();
});

attachToActiveTab(true);
