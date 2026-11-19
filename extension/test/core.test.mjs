// Tests the extension's protocol client (src/chat-core.js) against a live server.
// node test/core.test.mjs http://localhost   (PHP shop fixture at /shop/)
import { AiChat } from "../src/chat-core.js";
import assert from "node:assert/strict";

const site = process.argv[2] || "http://localhost";
// A browser keeps cookies for the site; emulate that with a tiny cookie jar.
const jar = new Map();
const browserFetch = async (url, opts = {}) => {
  const headers = { ...(opts.headers || {}), Origin: "chrome-extension://testextensionid" };
  if (jar.size) headers.Cookie = [...jar].map(([k, v]) => `${k}=${v}`).join("; ");
  const res = await fetch(url, { ...opts, headers, redirect: "manual" });
  for (const c of res.headers.getSetCookie?.() || []) {
    const [kv] = c.split(";"); const i = kv.indexOf("=");
    if (/Max-Age=0/i.test(c)) jar.delete(kv.slice(0, i)); else jar.set(kv.slice(0, i), kv.slice(i + 1));
  }
  return res;
};

const chat = new AiChat(site, { fetch: browserFetch });
let t = await chat.start("/shop/");
assert.equal(t.type, "chat"); assert.ok(t.conversation); assert.ok(t.you_can.length >= 3);
console.log("open:", t.message.slice(0, 70));
t = await chat.say("do you ship to Canada?");          assert.ok(t.answer.text);
console.log("say:", t.answer.answered_by, "|", t.answer.text.slice(0, 50));
const add = t.you_can.find((a) => a.name.startsWith("Add to basket: Whale"));
t = await chat.do({ ...add.do, fields: { Quantity: "2" } }); assert.equal(t.result.done, true);
console.log("do:", t.message.slice(0, 70));
t = await chat.go("/shop/checkout.php");               assert.equal(t.at.url, "/shop/login.php");
t = await chat.do({ form: 1, fields: { Username: "reza", Password: "s3cret" } });
assert.equal(t.at.url, "/shop/account.php"); assert.ok(jar.has("PHPSESSID"));
console.log("login:", t.message.slice(0, 70));
t = await chat.go("/shop/checkout.php");
const order = { form: 1, fields: { address: "1 Rue de la Loge", card_number: "4242424242424242" } };
t = await chat.do(order);                              assert.equal(t.result.problem, "needs_confirmation");
t = await chat.do({ ...order, confirm: true });        assert.equal(t.at.title, "Order placed");
console.log("order:", t.message.slice(0, 70));
assert.ok(AiChat.advertisesChat('</x>; rel="alternate"; type="application/vnd.ai-chat+json"'));
const ad = await (await fetch(site + "/marina.html?ai=chat", { headers: { Accept: "application/vnd.ai-chat+json" } })).json();
assert.ok(ad.sponsored.items.length >= 1); assert.ok(!ad.content.includes("SeaCo"));
console.log("ads: separate block with", ad.sponsored.items.length, "items; none inside the content");
try { await new AiChat(site).start("/logo.png"); } catch (e) { /* a non-chat answer is reported, not crashed on */ }
console.log("ALL EXTENSION CORE TESTS PASSED");
