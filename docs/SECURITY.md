# Security

## Model
mod_ai_agent adds an interface for AI agents to an existing website. Its rule: **an AI interface must never become a back door.**

1. **Authentication is never bypassed.** Reading, actions and chat run as the client, with the client's own cookies and Authorization header. The site's own code decides what the client may see and do.
2. **Nothing behind a login leaks.** Only requests without cookies or Authorization are learned. Also excluded from learning: responses with `Cache-Control: private`, and URLs the code scan marks as login-only (stale copies are deleted). Forms and code endpoints known only from login-only areas are never listed. Logged-in results are returned only to that client (`Cache-Control: no-store`). They are never cached, never learned and never sent to the AI.
3. **Consequential actions need consent.** Payments/orders, deletions/cancellations and password changes require `"confirm": true`.
4. **CSRF.** State-changing endpoints (`/act`, chat POST) only accept JSON content types, which forces a CORS preflight that is never granted. They reject `Sec-Fetch-Site: cross-site|same-site` and foreign `Origin` values. Only same-site and browser-extension origins are accepted.
5. **Identity.** Internal requests made for a client carry a per-start random secret. The client's real IP is restored, so `Require ip`, logs and applications see the real client. The header is ignored from non-local connections or with a wrong secret.
6. **No SSRF.** Actions only target the same site (connections are pinned to the local address). Redirects to other sites are reported, never followed.
7. **No secrets in logs.** `actions.log` never records field values; `questions.log` records questions only.
8. **AI keys** can be read from a root-only file (`file:`) or the environment (`env:`).
9. **Abuse limits.** Per-site rate limits apply to AI calls and actions.

## Known limits
Login detection is static analysis and can miss unusual authentication schemes. Always use `AIAgentExclude` for sensitive areas. Set `AIAgentActions read-only` or `off` if you don't want agents submitting forms.

## Reporting a vulnerability
Email {CONTACT_EMAIL} with details and steps to reproduce. Please allow 90 days before public disclosure.
