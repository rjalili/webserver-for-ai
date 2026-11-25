// AI Chat background: opens the chat UI and marks sites that offer ai-chat.
// Works as a Chromium service worker (Chrome, Brave, Edge, Opera) and as a Firefox/Safari background script.
const api = globalThis.browser ?? globalThis.chrome;
const CHAT_RE = /type="?application\/vnd\.ai-chat\+json"?/i;
const supported = new Map();   // tabId -> boolean

// Toolbar button: side panel where the browser has one, Firefox sidebar, else a popup.
if (api.sidePanel?.setPanelBehavior) {
  api.sidePanel.setPanelBehavior({ openPanelOnActionClick: true }).catch(() => {});
} else if (api.sidebarAction?.toggle) {
  api.action.onClicked.addListener(() => api.sidebarAction.toggle());
} else if (api.action?.setPopup) {
  api.action.setPopup({ popup: "panel.html" });   // Opera, and Safari if no default_popup
}

function showBadge(tabId) {
  const ok = supported.get(tabId);
  api.action.setBadgeText({ tabId, text: ok ? "AI" : "" });
  if (ok) api.action.setBadgeBackgroundColor?.({ tabId, color: "#0b5cad" });
  api.action.setTitle({ tabId, title: ok ? "Chat with this site" : "AI Chat (this site has no chat interface yet)" });
}

// Discovery: every page served with mod_http_b says Link: <...>; rel="alternate"; type="application/vnd.ai-chat+json"
if (api.webRequest?.onHeadersReceived) {
  api.webRequest.onHeadersReceived.addListener((d) => {
    if (d.tabId < 0) return;
    const link = (d.responseHeaders || []).filter((h) => h.name.toLowerCase() === "link").map((h) => h.value).join(", ");
    supported.set(d.tabId, CHAT_RE.test(link));
  }, { urls: ["http://*/*", "https://*/*"], types: ["main_frame"] }, ["responseHeaders"]);
}

api.tabs.onUpdated.addListener((tabId, info) => { if (info.status === "complete" && supported.has(tabId)) showBadge(tabId); });
api.tabs.onRemoved.addListener((tabId) => supported.delete(tabId));
