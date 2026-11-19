// Drives the real extension in Chrome: node test/ui.test.mjs [chrome-binary] [site]
// Needs: npm i playwright; a Chrome or Chrome for Testing binary (branded Chrome >= 137 ignores --load-extension);
// the PHP shop fixture at /shop/ and tests/fixtures marina page at /marina.html.
import { chromium } from "playwright";
import { fileURLToPath } from "node:url";
import path from "node:path";
const ext = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../dist/chromium");
const chromeBin = process.argv[2] || "/usr/bin/google-chrome";
const SITE = (process.argv[3] || "http://localhost:8080").replace(/\/+$/, "");
const shots = "/tmp";
const ctx = await chromium.launchPersistentContext("/tmp/chrome-prof-" + Date.now(), {
  executablePath: chromeBin, headless: true, viewport: { width: 420, height: 900 },
  args: [`--disable-extensions-except=${ext}`, `--load-extension=${ext}`, "--no-sandbox"],
});
let [sw] = ctx.serviceWorkers(); if (!sw) sw = await ctx.waitForEvent("serviceworker");
const extId = sw.url().split("/")[2];
const site = await ctx.newPage();
await site.goto(SITE + "/shop/");
const tabId = await sw.evaluate(async (site) => (await chrome.tabs.query({ url: site + "/shop/*" }))[0].id, SITE);
await site.waitForLoadState("load"); await site.waitForTimeout(800);
const badge = await sw.evaluate((id) => chrome.action.getBadgeText({ tabId: id }), tabId);
console.log("1. discovery badge on the shop tab:", JSON.stringify(badge));

const panel = await ctx.newPage();
await panel.goto(`chrome-extension://${extId}/panel.html?tab=${tabId}`);
const lastSite = async () => (await panel.locator(".msg.site, .msg.error").last().textContent()).slice(0, 110);
await panel.waitForSelector(".msg.site");
console.log("2. opening turn:", await lastSite());
await panel.screenshot({ path: `${shots}/1-open.png`, fullPage: true });

// add 2 gift-wrapped whale widgets using the generated form
const whale = panel.locator(".card", { hasText: "Add to basket: Whale" }).first();
console.log("   quantity preselected:", await whale.locator("select").inputValue()); await whale.locator("select").selectOption("2");
await whale.locator("input[type=checkbox]").check();
await whale.locator("button[type=submit]").click();
await panel.waitForFunction(() => document.body.innerText.includes("Your basket"));
console.log("3. add to basket:", await lastSite());

// say something
await panel.fill("#q", "do you ship to Canada?"); await panel.click("#ask button");
await panel.waitForFunction(() => [...document.querySelectorAll(".msg.you")].some(e => e.textContent.includes("Canada")) && !document.querySelector("#q").disabled);
console.log("4. question answered:", await lastSite());

// go to checkout -> login page
await panel.locator(".chip", { hasText: "Go to checkout" }).last().click();
await panel.waitForFunction(() => document.querySelector("#title").textContent === "Log in");
const login = panel.locator(".card", { hasText: "Log in" }).last();
await login.locator("input[type=text]").fill("reza");
await login.locator("input[type=password]").fill("s3cret");
await login.locator("button[type=submit]").click();
await panel.waitForFunction(() => document.querySelector("#title").textContent === "My account");
console.log("5. logged in, now at:", await panel.locator("#title").textContent());

// the same session now shows in the normal tab too (hybrid): open the account page there
await site.goto(SITE + "/shop/account.php");
console.log("6. normal tab shares the login:", (await site.locator("h1").textContent()));

// checkout with confirmation card
await panel.fill("#q", ""); 
await panel.evaluate(() => {}); 
const goCheckout = async () => { await panel.locator(".chip").filter({ hasText: /checkout/i }).last().click().catch(() => {}); };
await site.goto(SITE + "/shop/checkout.php");   // follow-tab: the panel follows the user's browsing
await panel.waitForFunction(() => document.querySelector("#title").textContent === "Checkout", null, { timeout: 15000 });
console.log("7. panel followed the tab to:", await panel.locator("#title").textContent());
const order = panel.locator(".card", { hasText: "Place order" }).last();
const inputs = order.locator("input[type=text]");
await inputs.nth(0).fill("1 Rue de la Loge, Montpellier");
await inputs.nth(1).fill("4242424242424242");
await order.locator("button[type=submit]").click();
await panel.waitForSelector(".card.warn");
console.log("   your bubble:", (await panel.locator(".msg.you").last().textContent())); console.log("8. confirmation card:", (await panel.locator(".card.warn").textContent()).slice(0, 80));
await panel.screenshot({ path: `${shots}/2-confirm.png`, fullPage: false });
await panel.locator(".card.warn button", { hasText: "Yes" }).click();
await panel.waitForFunction(() => document.querySelector("#title").textContent === "Order placed");
console.log("9. after confirming:", await lastSite());

// ads: clearly separated block on the marina page
await site.goto(SITE + "/marina.html");
await panel.waitForSelector(".sponsored");
console.log("10. sponsored box:", (await panel.locator(".sponsored .badge").last().textContent()), "| items:", await panel.locator(".sponsored").last().locator(".item").count());
await panel.locator(".sponsored").last().scrollIntoViewIfNeeded();
await panel.screenshot({ path: `${shots}/3-ads.png`, fullPage: false });

// a site without ai-chat
await site.goto("about:blank");
console.log("ALL UI STEPS PASSED");
await ctx.close();
