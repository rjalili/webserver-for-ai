/*
 * ngx_http_b_module - http-b for nginx (port of mod_http_b, stage 1).
 *
 * Stage 1: configuration, agent detection, mode selection (?ai=, X-AI-Mode,
 * Accept), removal of the ai= argument, discovery Link headers, the agent
 * help header and the welcome document at the prefix (/.well-known/ai).
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define HB_NONE  0
#define HB_EXACT 1
#define HB_GUIDE 2
#define HB_TEXT  3
#define HB_CHAT  4

typedef struct {
    ngx_flag_t   enable;
    ngx_uint_t   default_mode;
    ngx_array_t *user_agents;   /* ngx_str_t, lower case */
    ngx_flag_t   builtin_ua;
    ngx_str_t    site_name;
    ngx_str_t    site_desc;
    ngx_str_t    contact;
    ngx_str_t    prefix;
    ngx_array_t *excludes;      /* ngx_str_t */
    ngx_flag_t   advertise;
} ngx_http_b_loc_conf_t;

typedef struct {
    ngx_uint_t   mode;
    ngx_str_t    agent;
    const char  *source;
    ngx_uint_t   endpoint;
    ngx_str_t    public_uri;   /* the URI the client asked for */
} ngx_http_b_ctx_t;

static ngx_int_t ngx_http_b_init(ngx_conf_t *cf);
static void *ngx_http_b_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_b_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_http_b_set_mode(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_b_add_list(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static const char *builtin_agents[] = {
    "gptbot", "chatgpt-user", "oai-searchbot", "claudebot", "claude-user",
    "claude-searchbot", "claude-web", "anthropic-ai", "perplexitybot",
    "perplexity-user", "google-extended", "google-cloudvertexbot",
    "gemini-deep-research", "googleagent", "bytespider", "ccbot", "cohere-ai",
    "cohere-training-data-crawler", "meta-externalagent",
    "meta-externalfetcher", "facebookbot", "applebot-extended", "amazonbot",
    "duckassistbot", "mistralai-user", "youbot", "ai2bot", "diffbot",
    "timpibot", "omgili", "petalbot", "kangaroo bot", "ai-agent", "llm",
    "agent-browser", NULL
};

static ngx_command_t ngx_http_b_commands[] = {
    { ngx_string("http_b"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, enable), NULL },
    { ngx_string("http_b_default_mode"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_b_set_mode, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },
    { ngx_string("http_b_user_agent"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_b_add_list, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, user_agents), (void *) 1 },
    { ngx_string("http_b_builtin_user_agents"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, builtin_ua), NULL },
    { ngx_string("http_b_site_name"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, site_name), NULL },
    { ngx_string("http_b_site_description"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, site_desc), NULL },
    { ngx_string("http_b_contact"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, contact), NULL },
    { ngx_string("http_b_prefix"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, prefix), NULL },
    { ngx_string("http_b_exclude"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_b_add_list, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, excludes), NULL },
    { ngx_string("http_b_advertise"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot, NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_b_loc_conf_t, advertise), NULL },
    ngx_null_command
};

static ngx_http_module_t ngx_http_b_module_ctx = {
    NULL, ngx_http_b_init, NULL, NULL, NULL, NULL,
    ngx_http_b_create_loc_conf, ngx_http_b_merge_loc_conf
};

ngx_module_t ngx_http_b_module = {
    NGX_MODULE_V1, &ngx_http_b_module_ctx, ngx_http_b_commands, NGX_HTTP_MODULE,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NGX_MODULE_V1_PADDING
};

/* ---------------------------------------------------------------- */
/* small helpers                                                     */
/* ---------------------------------------------------------------- */

typedef struct { ngx_pool_t *p; u_char *s; size_t len, cap; } hb_buf_t;

static void hb_init(hb_buf_t *b, ngx_pool_t *p, size_t cap)
{ b->p = p; b->cap = cap; b->len = 0; b->s = ngx_pnalloc(p, cap); }

static void hb_putn(hb_buf_t *b, const u_char *s, size_t n)
{
    if (b->s == NULL) return;
    if (b->len + n > b->cap) {
        size_t nc = (b->cap + n) * 2;
        u_char *ns = ngx_pnalloc(b->p, nc);
        if (ns == NULL) { b->s = NULL; return; }
        ngx_memcpy(ns, b->s, b->len); b->s = ns; b->cap = nc;
    }
    ngx_memcpy(b->s + b->len, s, n); b->len += n;
}
static void hb_puts(hb_buf_t *b, const char *s) { hb_putn(b, (const u_char *) s, ngx_strlen(s)); }
static void hb_putc(hb_buf_t *b, char c) { hb_putn(b, (u_char *) &c, 1); }

static void hb_json(hb_buf_t *b, const u_char *s, size_t n)
{
    size_t i; u_char tmp[8];
    hb_putc(b, '"');
    for (i = 0; i < n; i++) {
        u_char c = s[i];
        if (c == '"' || c == '\\') { hb_putc(b, '\\'); hb_putc(b, c); }
        else if (c == '\n') hb_puts(b, "\\n");
        else if (c == '\r') hb_puts(b, "\\r");
        else if (c == '\t') hb_puts(b, "\\t");
        else if (c < 0x20) { tmp[0]='\\'; tmp[1]='u'; tmp[2]='0'; tmp[3]='0'; tmp[4]="0123456789abcdef"[c >> 4]; tmp[5]="0123456789abcdef"[c & 15]; hb_putn(b, tmp, 6); }
        else hb_putc(b, c);
    }
    hb_putc(b, '"');
}
static void hb_jstr(hb_buf_t *b, const char *k, const char *v)
{ hb_json(b, (u_char *) k, ngx_strlen(k)); hb_putc(b, ':'); hb_json(b, (u_char *) v, ngx_strlen(v)); }
static void hb_jnstr(hb_buf_t *b, const char *k, ngx_str_t *v)
{ hb_json(b, (u_char *) k, ngx_strlen(k)); hb_putc(b, ':'); hb_json(b, v->data, v->len); }

static const char *hb_cstr(ngx_pool_t *p, ngx_str_t *s)
{
    u_char *c = ngx_pnalloc(p, s->len + 1);
    if (c == NULL) return "";
    ngx_memcpy(c, s->data, s->len); c[s->len] = 0;
    return (const char *) c;
}

static ngx_uint_t hb_parse_mode(u_char *v, size_t n)
{
#define IS(w) (n == sizeof(w) - 1 && ngx_strncasecmp(v, (u_char *) w, n) == 0)
    if (v == NULL || n == 0) return HB_NONE;
    if (IS("exact") || IS("raw") || IS("html")) return HB_EXACT;
    if (IS("guide") || IS("json")) return HB_GUIDE;
    if (IS("text") || IS("markdown") || IS("md")) return HB_TEXT;
    if (IS("chat") || IS("conversation")) return HB_CHAT;
#undef IS
    return HB_NONE;
}

#if (NGX_DEBUG)
static const char *hb_mode_name(ngx_uint_t m)
{
    return m == HB_GUIDE ? "guide" : m == HB_TEXT ? "text" : m == HB_EXACT ? "exact"
         : m == HB_CHAT ? "chat" : "none";
}
#endif

static ngx_table_elt_t *hb_header_in(ngx_http_request_t *r, const char *name)
{
    ngx_list_part_t *part = &r->headers_in.headers.part;
    ngx_table_elt_t *h = part->elts;
    size_t n = ngx_strlen(name);
    ngx_uint_t i;
    for (i = 0; ; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) break;
            part = part->next; h = part->elts; i = 0;
        }
        if (h[i].hash && h[i].key.len == n && ngx_strncasecmp(h[i].key.data, (u_char *) name, n) == 0)
            return &h[i];
    }
    return NULL;
}

static u_char *hb_strcasestr(ngx_str_t *hay, const char *needle)
{
    size_t n = ngx_strlen(needle), i;
    if (hay == NULL || hay->len < n) return NULL;
    for (i = 0; i + n <= hay->len; i++)
        if (ngx_strncasecmp(hay->data + i, (u_char *) needle, n) == 0) return hay->data + i;
    return NULL;
}

static ngx_int_t hb_add_header(ngx_http_request_t *r, const char *k, ngx_str_t *v)
{
    ngx_table_elt_t *h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) return NGX_ERROR;
    h->hash = 1; h->next = NULL;
    h->key.data = (u_char *) k; h->key.len = ngx_strlen(k);
    h->value = *v;
    return NGX_OK;
}

static int hb_excluded(ngx_http_b_loc_conf_t *c, ngx_str_t *uri)
{
    ngx_uint_t i; ngx_str_t *e;
    if (c->excludes == NULL) return 0;
    e = c->excludes->elts;
    for (i = 0; i < c->excludes->nelts; i++)
        if (uri->len >= e[i].len && ngx_strncmp(uri->data, e[i].data, e[i].len) == 0) return 1;
    return 0;
}

static int hb_isalpha(u_char c) { c |= 0x20; return c >= 'a' && c <= 'z'; }

static ngx_int_t hb_detect_agent(ngx_http_request_t *r, ngx_http_b_loc_conf_t *c, ngx_str_t *out)
{
    ngx_table_elt_t *xa = hb_header_in(r, "X-AI-Agent");
    ngx_str_t ua; u_char *l; ngx_uint_t i; ngx_str_t *pat;
    if (xa) {
        if (xa->value.len) { *out = xa->value; } else { ngx_str_set(out, "X-AI-Agent"); }
        return 1;
    }
    if (r->headers_in.user_agent == NULL) return 0;
    ua = r->headers_in.user_agent->value;
    l = ngx_pnalloc(r->pool, ua.len + 1);
    if (l == NULL) return 0;
    ngx_strlow(l, ua.data, ua.len); l[ua.len] = 0;
    if (c->user_agents) {
        pat = c->user_agents->elts;
        for (i = 0; i < c->user_agents->nelts; i++)
            if (ngx_strstr(l, hb_cstr(r->pool, &pat[i]))) { *out = pat[i]; return 1; }
    }
    if (c->builtin_ua) {
        for (i = 0; builtin_agents[i]; i++) {
            if (ngx_strcmp(builtin_agents[i], "llm") == 0) {
                u_char *s = (u_char *) ngx_strstr(l, "llm");
                if (s && (s == l || !hb_isalpha(s[-1])) && !hb_isalpha(s[3])) {
                    ngx_str_set(out, "llm"); return 1;
                }
                continue;
            }
            if (ngx_strstr(l, builtin_agents[i])) {
                out->data = (u_char *) builtin_agents[i]; out->len = ngx_strlen(builtin_agents[i]);
                return 1;
            }
        }
    }
    return 0;
}

/* Remove ai=... from the arguments; returns 1 and the value if present. */
static int hb_strip_ai_arg(ngx_http_request_t *r, ngx_str_t *val)
{
    u_char *p, *end, *tok, *amp, *o, *start;
    int found = 0;
    if (r->args.len == 0) return 0;
    start = ngx_pnalloc(r->pool, r->args.len);
    if (start == NULL) return 0;
    o = start; p = r->args.data; end = p + r->args.len;
    while (p < end) {
        tok = p;
        amp = ngx_strlchr(p, end, '&');
        if (amp == NULL) amp = end;
        if (amp - tok >= 3 && ngx_strncmp(tok, "ai=", 3) == 0) {
            val->data = tok + 3; val->len = amp - tok - 3; found = 1;
        } else if (amp - tok == 2 && ngx_strncmp(tok, "ai", 2) == 0) {
            ngx_str_set(val, "guide"); found = 1;
        } else if (amp > tok) {
            if (o > start) *o++ = '&';
            o = ngx_cpymem(o, tok, amp - tok);
        }
        p = amp + 1;
    }
    if (found) {
        r->args.data = start; r->args.len = o - start;
        r->valid_unparsed_uri = 0;   /* upstreams rebuild the URI without ai= */
    }
    return found;
}

/* The module context is wiped by internal redirects (index files,
 * error_page, try_files); keep it reachable through a pool cleanup. */
static void hb_ctx_marker(void *data) { (void) data; }

static ngx_http_b_ctx_t *hb_get_ctx(ngx_http_request_t *r)
{
    ngx_http_b_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_b_module);
    ngx_pool_cleanup_t *cln;
    if (ctx) return ctx;
    for (cln = r->pool->cleanup; cln; cln = cln->next) {
        if (cln->handler == hb_ctx_marker) {
            ctx = cln->data;
            ngx_http_set_ctx(r, ctx, ngx_http_b_module);
            return ctx;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------- */
/* welcome document                                                  */
/* ---------------------------------------------------------------- */

static ngx_int_t hb_send(ngx_http_request_t *r, ngx_uint_t status, const char *ctype, hb_buf_t *b)
{
    ngx_buf_t *buf; ngx_chain_t out; ngx_int_t rc;
    if (b->s == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    r->headers_out.status = status;
    r->headers_out.content_type.data = (u_char *) ctype;
    r->headers_out.content_type.len = ngx_strlen(ctype);
    r->headers_out.content_type_len = r->headers_out.content_type.len;
    r->headers_out.content_length_n = b->len;
    if (r->method == NGX_HTTP_HEAD) r->header_only = 1;
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) return rc;
    buf = ngx_calloc_buf(r->pool);
    if (buf == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    buf->pos = b->s; buf->last = b->s + b->len; buf->memory = 1;
    buf->last_buf = (r == r->main) ? 1 : 0; buf->last_in_chain = 1;
    out.buf = buf; out.next = NULL;
    return ngx_http_output_filter(r, &out);
}

static const char *hb_base_url(ngx_http_request_t *r)
{
    ngx_str_t host = r->headers_in.host ? r->headers_in.host->value : r->headers_in.server;
    const char *scheme = "http";
    u_char *p;
#if (NGX_HTTP_SSL)
    if (r->connection->ssl) scheme = "https";
#endif
    p = ngx_pnalloc(r->pool, host.len + 16);
    if (p == NULL) return "";
    ngx_sprintf(p, "%s://%V%Z", scheme, &host);
    return (const char *) p;
}

static ngx_int_t hb_welcome(ngx_http_request_t *r, ngx_http_b_loc_conf_t *c)
{
    hb_buf_t b; char *msg; size_t ml;
    const char *pre = hb_cstr(r->pool, &c->prefix);
    const char *name = c->site_name.len ? hb_cstr(r->pool, &c->site_name) : hb_cstr(r->pool, &r->headers_in.server);
    const char *desc = c->site_desc.len ? hb_cstr(r->pool, &c->site_desc) : NULL;
    const char *base = hb_base_url(r);
    char tmp[512];

    hb_init(&b, r->pool, 4096);
    hb_putc(&b, '{');
    hb_jstr(&b, "type", "welcome"); hb_putc(&b, ',');
    hb_jstr(&b, "language", "en"); hb_putc(&b, ',');
    ml = ngx_strlen(name) + ngx_strlen(base) + (desc ? ngx_strlen(desc) : 0) + 400;
    msg = ngx_pnalloc(r->pool, ml);
    if (msg == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    ngx_snprintf((u_char *) msg, ml,
        "Hello. You are connected to %s (%s).%s%s This website has an interface for AI agents: "
        "you can browse it exactly as a human would, or talk to it in plain English. Every response from the "
        "endpoints listed below is in English, so you can have a conversation with this site on behalf of your user.%Z",
        name, base, desc ? " " : "", desc ? desc : "");
    hb_jstr(&b, "message", msg); hb_putc(&b, ',');
    hb_puts(&b, "\"site\":{"); hb_jstr(&b, "name", name); hb_putc(&b, ',');
    hb_jstr(&b, "url", base);
    if (desc) { hb_putc(&b, ','); hb_jstr(&b, "description", desc); }
    if (c->contact.len) { hb_putc(&b, ','); hb_jnstr(&b, "contact", &c->contact); }
    hb_puts(&b, "},\"how_to_talk_to_this_site\":[");
#define STEP(say, req, alt) do { hb_putc(&b, '{'); hb_jstr(&b, "you_want", say); hb_putc(&b, ','); \
        hb_jstr(&b, "request", req); { const char *alt_ = (alt); if (alt_) { hb_putc(&b, ','); hb_jstr(&b, "alternative", alt_); } } \
        hb_putc(&b, '}'); } while (0)
    ngx_snprintf((u_char *) tmp, sizeof(tmp), "GET %s/sitemap.json%Z", pre);
    STEP("A list of every page with an English description of each", tmp, (char *) NULL); hb_putc(&b, ',');
    {
        char a[256];
        ngx_snprintf((u_char *) tmp, sizeof(tmp), "GET %s/ask?q=<your question>%Z", pre);
        ngx_snprintf((u_char *) a, sizeof(a), "POST %s/ask with JSON {\"question\": \"...\"}%Z", pre);
        STEP("To ask the site a question in plain English", tmp, a); hb_putc(&b, ',');
        ngx_snprintf((u_char *) a, sizeof(a), "GET %s/page?url=<path>, or send the header 'X-AI-Mode: guide' "
                     "or 'Accept: application/vnd.ai+json'%Z", pre);
        STEP("An English explanation of one page (summary, outline, text, links, forms)", "GET <page URL>?ai=guide", a);
        hb_putc(&b, ',');
        ngx_snprintf((u_char *) tmp, sizeof(tmp), "GET %s/actions to see what is possible, then POST %s/act with "
                     "JSON {\"action\": \"<id>\", \"fields\": {...}}%Z", pre, pre);
        ngx_snprintf((u_char *) a, sizeof(a), "POST %s/act with {\"open\": \"/page\"} to follow a link, or "
                     "{\"page\": \"/page\", \"form\": 1, \"fields\": {...}} to use any form you see%Z", pre);
        STEP("To do something on the site exactly as a person would by clicking (search, fill in a form, log in, "
             "add to basket, order)", tmp, a); hb_putc(&b, ',');
        ngx_snprintf((u_char *) a, sizeof(a), "then POST the same URL or %s/chat with {\"conversation\": \"<id>\", "
                     "\"say\" | \"do\" | \"go\": ...}%Z", pre);
        STEP("A conversation with the site, starting at any page (the server tells you where you are, what you can "
             "do and where you can go; you reply with say/do/go)",
             "GET <page URL> with 'Accept: application/vnd.ai-chat+json' (or <page URL>?ai=chat)", a);
        hb_putc(&b, ',');
    }
    STEP("A page as readable Markdown text", "GET <page URL>?ai=text",
         "send the header 'X-AI-Mode: text' or 'Accept: text/markdown'"); hb_putc(&b, ',');
    STEP("A page exactly as a human's browser receives it", "GET <page URL>?ai=exact",
         "send the header 'X-AI-Mode: exact'");
#undef STEP
    hb_puts(&b, "],\"conversational_ai\":{");
    hb_jstr(&b, "status", "keyword-search"); hb_putc(&b, ',');
    hb_jstr(&b, "about", "Questions are answered by searching the site's pages; no AI service is configured.");
    hb_puts(&b, "},\"login_requirements\":{");
    hb_jstr(&b, "about", "Not analysed (code scanning is off).");
    hb_puts(&b, "},");
    hb_jstr(&b, "protocol", "http-b/1");
    hb_puts(&b, "}\n");
    return hb_send(r, NGX_HTTP_OK, "application/json; charset=utf-8", &b);
}

static ngx_int_t hb_endpoint_handler(ngx_http_request_t *r)
{
    ngx_http_b_loc_conf_t *c = ngx_http_get_module_loc_conf(r, ngx_http_b_module);
    ngx_str_t rest;
    ngx_int_t rc;
    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD | NGX_HTTP_POST))) return NGX_HTTP_NOT_ALLOWED;
    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) return rc;
    rest.data = r->uri.data + c->prefix.len; rest.len = r->uri.len - c->prefix.len;
    if (rest.len == 0 || (rest.len == 1 && rest.data[0] == '/')
        || (rest.len == 11 && ngx_strncmp(rest.data, "/index.json", 11) == 0))
        return hb_welcome(r, c);
    /* sitemap, ask, page, actions, act, chat, mcp: later stages */
    return NGX_HTTP_NOT_FOUND;
}

/* ---------------------------------------------------------------- */
/* request phase: mode and endpoint selection                        */
/* ---------------------------------------------------------------- */

static ngx_int_t ngx_http_b_rewrite_handler(ngx_http_request_t *r)
{
    ngx_http_b_loc_conf_t *c;
    ngx_http_b_ctx_t *ctx;
    ngx_table_elt_t *hv, *acc, *ct;
    ngx_str_t qv = ngx_null_string, agent = ngx_null_string;
    ngx_uint_t mode = HB_NONE;
    int has_agent;

    ngx_pool_cleanup_t *cln;

    if (r != r->main) return NGX_DECLINED;
    if (hb_get_ctx(r)) return NGX_DECLINED;   /* already decided before a redirect */
    c = ngx_http_get_module_loc_conf(r, ngx_http_b_module);
    if (!c->enable) return NGX_DECLINED;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_b_ctx_t));
    if (ctx == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    ngx_http_set_ctx(r, ctx, ngx_http_b_module);
    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    cln->handler = hb_ctx_marker; cln->data = ctx;
    ctx->public_uri = r->uri;

    if (r->uri.len >= c->prefix.len && ngx_strncmp(r->uri.data, c->prefix.data, c->prefix.len) == 0
        && (r->uri.len == c->prefix.len || r->uri.data[c->prefix.len] == '/')) {
        ctx->endpoint = 1;
        r->content_handler = hb_endpoint_handler;
        return NGX_DECLINED;
    }
    if (hb_excluded(c, &r->uri)) return NGX_DECLINED;

    has_agent = hb_detect_agent(r, c, &agent);
    hb_strip_ai_arg(r, &qv);
    hv = hb_header_in(r, "X-AI-Mode");
    acc = hb_header_in(r, "Accept");
    ct = r->headers_in.content_type;

    if ((mode = hb_parse_mode(qv.data, qv.len))) ctx->source = "query";
    else if (hv && (mode = hb_parse_mode(hv->value.data, hv->value.len))) ctx->source = "header";
    else if ((acc && hb_strcasestr(&acc->value, "application/vnd.ai-chat+json"))
             || (ct && hb_strcasestr(&ct->value, "application/vnd.ai-chat+json"))) {
        mode = HB_CHAT; ctx->source = "header";
    } else if (acc && hb_strcasestr(&acc->value, "application/vnd.ai+json")) {
        mode = HB_GUIDE; ctx->source = "header";
    } else if (acc && hb_strcasestr(&acc->value, "text/markdown")) {
        u_char *md = hb_strcasestr(&acc->value, "text/markdown");
        u_char *html = hb_strcasestr(&acc->value, "text/html");
        if (html == NULL || md < html) { mode = HB_TEXT; ctx->source = "header"; }
    }
    if (!mode && has_agent) { mode = c->default_mode; ctx->source = "default"; }
    if (mode && !has_agent && mode != HB_EXACT) { ngx_str_set(&agent, "explicit-request"); has_agent = 1; }
    ctx->mode = mode;
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "http-b mode %s from %s",
                   hb_mode_name(mode), ctx->source ? ctx->source : "none");
    if (has_agent) ctx->agent = agent;

    if (mode == HB_GUIDE || mode == HB_TEXT) {
        /* the transformation (stage 2) needs the full, uncompressed body */
        r->headers_in.if_modified_since = NULL;
        r->headers_in.if_none_match = NULL;
        r->headers_in.if_range = NULL;
        r->headers_in.range = NULL;
#if (NGX_HTTP_GZIP)
        r->gzip_tested = 1; r->gzip_ok = 0;
#endif
    }
    return NGX_DECLINED;
}

/* ---------------------------------------------------------------- */
/* header filter: discovery and help headers                         */
/* ---------------------------------------------------------------- */

static ngx_int_t ngx_http_b_header_filter(ngx_http_request_t *r)
{
    ngx_http_b_loc_conf_t *c;
    ngx_http_b_ctx_t *ctx;
    ngx_str_t v; size_t n; u_char *p; uintptr_t esc;

    if (r != r->main) return ngx_http_next_header_filter(r);
    c = ngx_http_get_module_loc_conf(r, ngx_http_b_module);
    ctx = hb_get_ctx(r);
    if (!c->enable || ctx == NULL || ctx->endpoint) return ngx_http_next_header_filter(r);

    if (c->advertise || ctx->agent.len) {
        esc = ngx_escape_uri(NULL, ctx->public_uri.data, ctx->public_uri.len, NGX_ESCAPE_URI);
        n = 3 * c->prefix.len + ctx->public_uri.len + 2 * esc + 200;
        p = ngx_pnalloc(r->pool, n);
        if (p == NULL) return NGX_ERROR;
        v.data = p;
        p = ngx_sprintf(p, "<%V>; rel=\"ai-agent\"; type=\"application/json\", "
                           "<%V/sitemap.json>; rel=\"ai-sitemap\"; type=\"application/json\", <",
                        &c->prefix, &c->prefix);
        p = (u_char *) ngx_escape_uri(p, ctx->public_uri.data, ctx->public_uri.len, NGX_ESCAPE_URI);
        p = ngx_sprintf(p, ">; rel=\"alternate\"; type=\"application/vnd.ai-chat+json\"");
        v.len = p - v.data;
        if (hb_add_header(r, "Link", &v) != NGX_OK) return NGX_ERROR;
    }
    if (ctx->mode && ctx->mode != HB_GUIDE && ctx->mode != HB_TEXT && ctx->agent.len) {
        n = c->prefix.len + 120;
        p = ngx_pnalloc(r->pool, n);
        if (p == NULL) return NGX_ERROR;
        v.data = p;
        p = ngx_sprintf(p, "This site speaks English to AI agents. Add ?ai=guide or ?ai=text to any URL, "
                           "or start at %V", &c->prefix);
        v.len = p - v.data;
        if (hb_add_header(r, "X-AI-Agent-Help", &v) != NGX_OK) return NGX_ERROR;
    }
    if (ctx->mode) {
        ngx_str_set(&v, "Accept, X-AI-Mode");
        if (hb_add_header(r, "Vary", &v) != NGX_OK) return NGX_ERROR;
    }
    return ngx_http_next_header_filter(r);
}

/* ---------------------------------------------------------------- */
/* configuration                                                     */
/* ---------------------------------------------------------------- */

static char *ngx_http_b_set_mode(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_b_loc_conf_t *c = conf;
    ngx_str_t *v = cf->args->elts;
    ngx_uint_t m = hb_parse_mode(v[1].data, v[1].len);
    if (c->default_mode != NGX_CONF_UNSET_UINT) return "is duplicate";
    if (m == HB_NONE || m == HB_CHAT) return "must be exact, guide or text";
    c->default_mode = m;
    return NGX_CONF_OK;
}

static char *ngx_http_b_add_list(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_array_t **a = (ngx_array_t **) ((char *) conf + cmd->offset);
    ngx_str_t *v = cf->args->elts, *s;
    ngx_uint_t i;
    if (*a == NULL || *a == NGX_CONF_UNSET_PTR) {
        *a = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
        if (*a == NULL) return NGX_CONF_ERROR;
    }
    for (i = 1; i < cf->args->nelts; i++) {
        s = ngx_array_push(*a);
        if (s == NULL) return NGX_CONF_ERROR;
        *s = v[i];
        if (cmd->post) {   /* lower-case user agent patterns */
            s->data = ngx_pnalloc(cf->pool, v[i].len);
            if (s->data == NULL) return NGX_CONF_ERROR;
            ngx_strlow(s->data, v[i].data, v[i].len);
        }
    }
    return NGX_CONF_OK;
}

static void *ngx_http_b_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_b_loc_conf_t *c = ngx_pcalloc(cf->pool, sizeof(ngx_http_b_loc_conf_t));
    if (c == NULL) return NULL;
    c->enable = NGX_CONF_UNSET;
    c->default_mode = NGX_CONF_UNSET_UINT;
    c->user_agents = NGX_CONF_UNSET_PTR;
    c->builtin_ua = NGX_CONF_UNSET;
    c->excludes = NGX_CONF_UNSET_PTR;
    c->advertise = NGX_CONF_UNSET;
    return c;
}

static char *ngx_http_b_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_b_loc_conf_t *prev = parent, *c = child;
    ngx_conf_merge_value(c->enable, prev->enable, 0);
    ngx_conf_merge_uint_value(c->default_mode, prev->default_mode, HB_EXACT);
    ngx_conf_merge_ptr_value(c->user_agents, prev->user_agents, NULL);
    ngx_conf_merge_value(c->builtin_ua, prev->builtin_ua, 1);
    ngx_conf_merge_str_value(c->site_name, prev->site_name, "");
    ngx_conf_merge_str_value(c->site_desc, prev->site_desc, "");
    ngx_conf_merge_str_value(c->contact, prev->contact, "");
    ngx_conf_merge_str_value(c->prefix, prev->prefix, "/.well-known/ai");
    ngx_conf_merge_ptr_value(c->excludes, prev->excludes, NULL);
    ngx_conf_merge_value(c->advertise, prev->advertise, 1);
    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_b_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    ngx_http_handler_pt *h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
    if (h == NULL) return NGX_ERROR;
    *h = ngx_http_b_rewrite_handler;
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_b_header_filter;
    return NGX_OK;
}
