// http-b/1 protocol client: shared by the browser extension and the Node tests.
// No browser APIs here: give it a fetch function (the browser's, or one with a cookie jar).

export const CHAT_TYPE = "application/vnd.ai-chat+json";

export class AiChat {
  /**
   * @param {string} origin   e.g. "https://example.com"
   * @param {object} [opts]   { fetch: custom fetch, conversation: resume an id, agentName: X-AI-Agent value }
   */
  constructor(origin, opts = {}) {
    this.origin = origin.replace(/\/+$/, "");
    this.fetch = opts.fetch || ((...a) => fetch(...a));
    this.conversation = opts.conversation || null;
    this.agentName = opts.agentName || "AI Chat browser extension";
    this.at = "/";
    this.last = null;
  }

  /** Does this page offer a chat version? (Link: <...>; rel="alternate"; type="application/vnd.ai-chat+json") */
  static advertisesChat(linkHeader) {
    return !!linkHeader && /type="?application\/vnd\.ai-chat\+json"?/i.test(linkHeader);
  }

  async _send(method, path, body) {
    const headers = { Accept: CHAT_TYPE, "X-AI-Agent": this.agentName };
    if (body !== undefined) headers["Content-Type"] = CHAT_TYPE;
    const res = await this.fetch(this.origin + path, {
      method,
      headers,
      credentials: "include",          // the person's own cookies: their logins just work
      body: body === undefined ? undefined : JSON.stringify(body),
    });
    const type = res.headers.get("content-type") || "";
    if (!type.includes("json")) {
      const err = new Error("This site does not speak ai-chat (it answered with " + (type || "nothing") + ").");
      err.notSupported = true;
      throw err;
    }
    const turn = await res.json();
    if (turn.type === "chat") {
      this.conversation = turn.conversation || this.conversation;
      if (turn.at && turn.at.url) this.at = turn.at.url;
    }
    this.last = turn;
    return turn;
  }

  /** Open the conversation at a page (path + query). */
  start(path = "/") {
    this.at = path || "/";
    return this._send("GET", this.at);
  }

  _turn(extra) {
    return this._send("POST", this.at, { conversation: this.conversation, at: this.at, ...extra });
  }

  /** Ask in plain English. */
  say(text) { return this._turn({ say: text }); }

  /** Follow a link on the site. */
  go(path) { return this._turn({ go: path }); }

  /** Use form n of the current page, or an action id. */
  do({ form, action, fields = {}, button, confirm = false }) {
    const d = { fields };
    if (action) d.action = action; else d.form = form;
    if (button) d.button = button;
    if (confirm) d.confirm = true;
    return this._turn({ do: d });
  }

  /** Full description of a site-wide action (with its fields). */
  async describeAction(id) {
    const res = await this.fetch(`${this.origin}/.well-known/ai/actions/${encodeURIComponent(id)}`,
      { headers: { Accept: "application/json" }, credentials: "include" });
    return res.ok ? res.json() : null;
  }
}
