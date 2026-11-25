/*
 * hb_core.c - runs mod_http_b.c (the Apache module, unchanged) inside nginx.
 *
 * mod_http_b.c is included verbatim. Everything it needs from Apache httpd is
 * provided here on top of APR: request/server/connection records, the few
 * ap_* functions it calls, output filters fed from nginx, and sub-requests,
 * which are made as real HTTP requests back to this nginx server.
 */
/* Apache module struct: STANDARD20_MODULE_STUFF leaves "flags" implicit */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "mod_http_b.c"
#include "hb_core.h"
#include "apr_buckets.h"
#include "ap_mpm.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* globals                                                             */
/* ------------------------------------------------------------------ */

static apr_pool_t *hb_root;
static hb_log_fn hb_log;
static const char *hb_server_root = "/etc/nginx";
AP_DECLARE_DATA unixd_config_rec ap_unixd_config;

struct hb_server {
    server_rec s;
    void *vec[1];
    const char *docroot;
};

typedef struct { char *s; size_t len, cap; } hb_obuf;

struct hb_req {
    request_rec r;              /* must stay first: request_rec* -> hb_req* */
    conn_rec c;
    hb_server *srv;
    hb_obuf out;
    const char *body; size_t blen, bpos;
    int filters, removed;
    ap_filter_t fguide, flearn, sink_out, sink_null;
    hb_header *hdrs; int nh;
};

static void obuf_put(apr_pool_t *p, hb_obuf *o, const char *s, size_t n)
{
    if (o->len + n + 1 > o->cap) {
        size_t nc = (o->cap + n + 1) * 2;
        char *ns = apr_palloc(p, nc);
        if (o->len) memcpy(ns, o->s, o->len);
        o->s = ns; o->cap = nc;
    }
    memcpy(o->s + o->len, s, n); o->len += n; o->s[o->len] = 0;
}

static void hb_vlog(int level, const char *fmt, va_list ap)
{
    char buf[2048];
    apr_vsnprintf(buf, sizeof(buf), fmt, ap);
    if (hb_log) hb_log(level, buf);
}

static int hb_level(int aplevel)
{
    aplevel &= APLOG_LEVELMASK;
    return aplevel <= APLOG_ERR ? HB_LOG_ERR : aplevel <= APLOG_WARNING ? HB_LOG_WARN
         : aplevel <= APLOG_INFO ? HB_LOG_INFO : HB_LOG_DEBUG;
}

/* ------------------------------------------------------------------ */
/* Apache API used by mod_http_b.c                                     */
/* ------------------------------------------------------------------ */

AP_DECLARE(void) ap_log_error_(const char *file, int line, int module_index, int level,
                               apr_status_t status, const server_rec *s, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); hb_vlog(hb_level(level), fmt, ap); va_end(ap); }

AP_DECLARE(void) ap_log_rerror_(const char *file, int line, int module_index, int level,
                                apr_status_t status, const request_rec *r, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); hb_vlog(hb_level(level), fmt, ap); va_end(ap); }

AP_DECLARE(void) ap_log_perror_(const char *file, int line, int module_index, int level,
                                apr_status_t status, apr_pool_t *p, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); hb_vlog(hb_level(level), fmt, ap); va_end(ap); }

AP_DECLARE(char *) ap_strcasestr(const char *s1, const char *s2)
{
    size_t n = strlen(s2);
    if (!n) return (char *) s1;
    for (; *s1; s1++) if (!strncasecmp(s1, s2, n)) return (char *) s1;
    return NULL;
}

AP_DECLARE(void) ap_str_tolower(char *s) { for (; *s; s++) *s = apr_tolower(*s); }
AP_DECLARE(void) ap_str_toupper(char *s) { for (; *s; s++) *s = apr_toupper(*s); }

AP_DECLARE(void) ap_set_content_type(request_rec *r, const char *ct) { r->content_type = ct; }
AP_DECLARE(void) ap_set_content_length(request_rec *r, apr_off_t length) { r->clength = length; }

AP_DECLARE(int) ap_rwrite(const void *buf, int nbyte, request_rec *r)
{
    hb_req *q = (hb_req *) r;
    obuf_put(r->pool, &q->out, buf, (size_t) nbyte);
    return nbyte;
}

AP_DECLARE(const char *) ap_document_root(request_rec *r)
{
    hb_req *q = (hb_req *) r;
    return q->srv->docroot ? q->srv->docroot : "/usr/share/nginx/html";
}

AP_DECLARE(char *) ap_server_root_relative(apr_pool_t *p, const char *fname)
{
    if (!fname) return apr_pstrdup(p, hb_server_root);
    if (*fname == '/') return apr_pstrdup(p, fname);
    return apr_pstrcat(p, hb_server_root, "/", fname, NULL);
}

static const char *hb_scheme(const request_rec *r)
{
    return apr_table_get(r->notes, "hb-https") ? "https" : "http";
}

AP_DECLARE_NONSTD(const char *) ap_run_http_scheme(const request_rec *r) { return hb_scheme(r); }

AP_DECLARE(char *) ap_construct_url(apr_pool_t *p, const char *uri, request_rec *r)
{
    const char *host = apr_table_get(r->headers_in, "Host");
    if (!host) host = r->hostname ? r->hostname : r->server->server_hostname;
    return apr_pstrcat(p, hb_scheme(r), "://", host, uri, NULL);
}

AP_DECLARE(char *) ap_os_escape_path(apr_pool_t *p, const char *path, int partial)
{
    static const char hex[] = "0123456789ABCDEF";
    char *out = apr_palloc(p, 3 * strlen(path) + 3), *o = out;
    const unsigned char *s = (const unsigned char *) path;
    if (!partial && strchr(path, ':') && (!strchr(path, '/') || strchr(path, ':') < strchr(path, '/'))) {
        *o++ = '.'; *o++ = '/';
    }
    for (; *s; s++) {
        if (apr_isalnum(*s) || strchr("$-_.+!*'(),:@&=/~", *s)) *o++ = *s;
        else { *o++ = '%'; *o++ = hex[*s >> 4]; *o++ = hex[*s & 15]; }
    }
    *o = 0;
    return out;
}

AP_DECLARE(int) ap_unescape_urlencoded(char *query)
{
    char *s = query, *o = query;
    if (!query) return OK;
    while (*s) {
        if (*s == '+') { *o++ = ' '; s++; }
        else if (*s == '%' && apr_isxdigit(s[1]) && apr_isxdigit(s[2])) {
            char h[3] = { s[1], s[2], 0 };
            *o++ = (char) strtol(h, NULL, 16); s += 3;
        } else *o++ = *s++;
    }
    *o = 0;
    return OK;
}

AP_DECLARE(const char *) ap_get_status_line(int status)
{
    switch (status) {
    case 200: return "200 OK"; case 201: return "201 Created"; case 204: return "204 No Content";
    case 301: return "301 Moved Permanently"; case 302: return "302 Found"; case 303: return "303 See Other";
    case 304: return "304 Not Modified"; case 307: return "307 Temporary Redirect";
    case 400: return "400 Bad Request"; case 401: return "401 Unauthorized"; case 403: return "403 Forbidden";
    case 404: return "404 Not Found"; case 405: return "405 Method Not Allowed"; case 409: return "409 Conflict";
    case 410: return "410 Gone"; case 413: return "413 Request Entity Too Large"; case 415: return "415 Unsupported Media Type";
    case 422: return "422 Unprocessable Entity"; case 429: return "429 Too Many Requests";
    case 500: return "500 Internal Server Error"; case 502: return "502 Bad Gateway"; case 503: return "503 Service Unavailable";
    case 504: return "504 Gateway Timeout";
    }
    return status < 300 ? "200 OK" : status < 400 ? "302 Found" : status < 500 ? "400 Bad Request" : "500 Internal Server Error";
}

/* request bodies (read by nginx before the handler runs) */
AP_DECLARE(int) ap_setup_client_block(request_rec *r, int read_policy) { return OK; }
AP_DECLARE(int) ap_should_client_block(request_rec *r) { return ((hb_req *) r)->blen > 0; }
AP_DECLARE(long) ap_get_client_block(request_rec *r, char *buffer, apr_size_t bufsiz)
{
    hb_req *q = (hb_req *) r;
    size_t n = q->blen - q->bpos;
    if (n > bufsiz) n = bufsiz;
    memcpy(buffer, q->body + q->bpos, n);
    q->bpos += n;
    return (long) n;
}

/* regular expressions: PCRE2, as Apache 2.4 does */
static apr_status_t hb_regfree(void *preg) { ap_regfree(preg); return APR_SUCCESS; }

AP_DECLARE(void) ap_regfree(ap_regex_t *preg)
{
    if (preg->re_pcre) pcre2_code_free(preg->re_pcre);
    preg->re_pcre = NULL;
}

AP_DECLARE(int) ap_regcomp(ap_regex_t *preg, const char *pattern, int cflags)
{
    int err; PCRE2_SIZE off;
    uint32_t opt = 0;
    if (cflags & AP_REG_ICASE) opt |= PCRE2_CASELESS;
    if (cflags & AP_REG_NEWLINE) opt |= PCRE2_MULTILINE;
    if (cflags & AP_REG_DOTALL) opt |= PCRE2_DOTALL;
    preg->re_pcre = pcre2_compile((PCRE2_SPTR) pattern, PCRE2_ZERO_TERMINATED, opt, &err, &off, NULL);
    preg->re_erroffset = off;
    if (!preg->re_pcre) return AP_REG_INVARG;
    {
        uint32_t cnt = 0;
        pcre2_pattern_info(preg->re_pcre, PCRE2_INFO_CAPTURECOUNT, &cnt);
        preg->re_nsub = cnt;
    }
    return 0;
}

AP_DECLARE(ap_regex_t *) ap_pregcomp(apr_pool_t *p, const char *pattern, int cflags)
{
    ap_regex_t *preg = apr_palloc(p, sizeof(*preg));
    if (ap_regcomp(preg, pattern, cflags)) return NULL;
    apr_pool_cleanup_register(p, preg, hb_regfree, apr_pool_cleanup_null);
    return preg;
}

AP_DECLARE(int) ap_regexec_len(const ap_regex_t *preg, const char *buff, apr_size_t len,
                               apr_size_t nmatch, ap_regmatch_t *pmatch, int eflags)
{
    pcre2_match_data *md;
    int rc; apr_size_t i;
    uint32_t opt = 0;
    if (!preg || !preg->re_pcre) return AP_REG_INVARG;
    if (eflags & AP_REG_NOTBOL) opt |= PCRE2_NOTBOL;
    if (eflags & AP_REG_NOTEOL) opt |= PCRE2_NOTEOL;
    md = pcre2_match_data_create_from_pattern(preg->re_pcre, NULL);
    if (!md) return AP_REG_ESPACE;
    rc = pcre2_match(preg->re_pcre, (PCRE2_SPTR) buff, len, 0, opt, md, NULL);
    if (rc < 0) { pcre2_match_data_free(md); return AP_REG_NOMATCH; }
    {
        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        uint32_t cnt = pcre2_get_ovector_count(md);
        for (i = 0; i < nmatch; i++) {
            if (i < cnt && i < (apr_size_t) rc && ov[2 * i] != PCRE2_UNSET) {
                pmatch[i].rm_so = (apr_off_t) ov[2 * i]; pmatch[i].rm_eo = (apr_off_t) ov[2 * i + 1];
            } else pmatch[i].rm_so = pmatch[i].rm_eo = -1;
        }
    }
    pcre2_match_data_free(md);
    return 0;
}

AP_DECLARE(int) ap_regexec(const ap_regex_t *preg, const char *string, apr_size_t nmatch,
                           ap_regmatch_t *pmatch, int eflags)
{ return ap_regexec_len(preg, string, strlen(string), nmatch, pmatch, eflags); }

/* output filters: nginx feeds the response body in, the result is captured */
AP_DECLARE(ap_filter_rec_t *) ap_register_output_filter(const char *name, ap_out_filter_func filter_func,
                                                        ap_init_filter_func filter_init, ap_filter_type ftype)
{ return NULL; }

AP_DECLARE(ap_filter_t *) ap_add_output_filter(const char *name, void *ctx, request_rec *r, conn_rec *c)
{
    hb_req *q = (hb_req *) r;
    if (!strcmp(name, "AI_AGENT_GUIDE")) q->filters |= HB_FILTER_GUIDE;
    else if (!strcmp(name, "AI_AGENT_LEARN")) q->filters |= HB_FILTER_LEARN;
    return NULL;
}

AP_DECLARE(void) ap_remove_output_filter(ap_filter_t *f)
{
    hb_req *q = (hb_req *) f->r;
    if (f == &q->fguide) q->removed |= HB_FILTER_GUIDE;
    if (f == &q->flearn) q->removed |= HB_FILTER_LEARN;
}

AP_DECLARE(apr_status_t) ap_pass_brigade(ap_filter_t *next, apr_bucket_brigade *bb)
{
    hb_req *q = (hb_req *) next->r;
    apr_bucket *e;
    if (next == &q->sink_out) {
        for (e = APR_BRIGADE_FIRST(bb); e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) {
            const char *d; apr_size_t n;
            if (APR_BUCKET_IS_METADATA(e)) continue;
            if (apr_bucket_read(e, &d, &n, APR_BLOCK_READ) == APR_SUCCESS) obuf_put(q->r.pool, &q->out, d, n);
        }
    }
    apr_brigade_cleanup(bb);
    return APR_SUCCESS;
}

/* hooks are wired by the nginx module itself; registration is a no-op */
#define HB_NOHOOK(name) AP_DECLARE(void) ap_hook_##name(ap_HOOK_##name##_t *pf, const char * const *pre, \
                                                         const char * const *succ, int order) { }
HB_NOHOOK(child_init)
HB_NOHOOK(post_config)
HB_NOHOOK(insert_error_filter)
HB_NOHOOK(post_read_request)
HB_NOHOOK(translate_name)
HB_NOHOOK(map_to_storage)
HB_NOHOOK(handler)
HB_NOHOOK(insert_filter)

/* Sub-requests become real HTTP requests to this very server. */
static size_t hb_curl_write(char *ptr, size_t size, size_t nmemb, void *data)
{
    hb_req *q = data;
    obuf_put(q->r.pool, &q->out, ptr, size * nmemb);
    return size * nmemb;
}
static size_t hb_curl_drop(char *ptr, size_t size, size_t nmemb, void *data) { return size * nmemb; }

static CURL *hb_self_curl(request_rec *r, const char *uri, struct curl_slist **hd, struct curl_slist **res)
{
    hb_req *q = (hb_req *) r;
    const char *host = r->hostname ? r->hostname : "localhost";
    unsigned port = q->c.local_addr ? q->c.local_addr->port : 80;
    const char *ip = q->c.local_ip, *v;
    CURL *h = curl_easy_init();
    if (!h) return NULL;
    *res = curl_slist_append(NULL, apr_psprintf(r->pool, "%s:%u:%s%s%s", host, port,
                             strchr(ip, ':') ? "[" : "", ip, strchr(ip, ':') ? "]" : ""));
    curl_easy_setopt(h, CURLOPT_RESOLVE, *res);
    curl_easy_setopt(h, CURLOPT_URL, apr_psprintf(r->pool, "%s://%s:%u%s", hb_scheme(r), host, port, uri));
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    /* access rules (allow/deny, auth by address) must see the agent's address */
    *hd = curl_slist_append(*hd, apr_psprintf(r->pool, "X-Http-B-Client: %s %s", act_secret_g, r->useragent_ip));
    if ((v = apr_table_get(r->headers_in, "Cookie"))) *hd = curl_slist_append(*hd, apr_pstrcat(r->pool, "Cookie: ", v, NULL));
    if ((v = apr_table_get(r->headers_in, "Authorization"))) *hd = curl_slist_append(*hd, apr_pstrcat(r->pool, "Authorization: ", v, NULL));
    if ((v = apr_table_get(r->headers_in, "User-Agent"))) *hd = curl_slist_append(*hd, apr_pstrcat(r->pool, "User-Agent: ", v, NULL));
    return h;
}

AP_DECLARE(request_rec *) ap_sub_req_lookup_uri(const char *new_uri, const request_rec *r, ap_filter_t *next_filter)
{
    request_rec *rr = apr_pcalloc(r->pool, sizeof(*rr));
    struct curl_slist *hd = NULL, *res = NULL;
    CURL *h = hb_self_curl((request_rec *) r, new_uri, &hd, &res);
    long status = 500;
    rr->pool = r->pool;
    if (h) {
        hd = curl_slist_append(hd, "X-AI-Mode: exact");
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hd);
        curl_easy_setopt(h, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, hb_curl_drop);
        if (curl_easy_perform(h) == CURLE_OK) curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_cleanup(h);
    }
    curl_slist_free_all(hd); curl_slist_free_all(res);
    rr->status = (int) status;
    return rr;
}

AP_DECLARE(void) ap_destroy_sub_req(request_rec *r) { }

AP_DECLARE(void) ap_internal_redirect(const char *new_uri, request_rec *r)
{
    hb_req *q = (hb_req *) r;
    struct curl_slist *hd = NULL, *res = NULL;
    CURL *h = hb_self_curl(r, new_uri, &hd, &res);
    const char *m = apr_table_get(r->notes, "ai-mode");
    long status = 502; char *ct = NULL;
    if (!h) { r->status = 500; return; }
    hd = curl_slist_append(hd, apr_pstrcat(r->pool, "X-AI-Mode: ", mode_name(m ? atoi(m) : MODE_GUIDE), NULL));
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, hd);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, hb_curl_write);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, q);
    q->out.len = 0;
    if (curl_easy_perform(h) == CURLE_OK) {
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_getinfo(h, CURLINFO_CONTENT_TYPE, &ct);
        if (ct) r->content_type = apr_pstrdup(r->pool, ct);
        apr_table_setn(r->headers_out, "Content-Language", "en");
        apr_table_setn(r->headers_out, "X-AI-Mode", mode_name(m ? atoi(m) : MODE_GUIDE));
    }
    curl_easy_cleanup(h);
    curl_slist_free_all(hd); curl_slist_free_all(res);
    r->status = (int) status;
}

/* ------------------------------------------------------------------ */
/* bridge used by the nginx module                                     */
/* ------------------------------------------------------------------ */

int hb_core_global_init(hb_log_fn log, const char *server_root, unsigned uid, unsigned gid)
{
    if (hb_root) return 0;
    if (apr_initialize() != APR_SUCCESS) return -1;
    if (apr_pool_create(&hb_root, NULL) != APR_SUCCESS) return -1;
    hb_log = log;
    if (server_root) hb_server_root = apr_pstrdup(hb_root, server_root);
    ap_unixd_config.user_id = uid;
    ap_unixd_config.group_id = gid;
    http_b_module.module_index = 0;
    return 0;
}

void hb_core_set_user(unsigned uid, unsigned gid)
{
    ap_unixd_config.user_id = uid;
    ap_unixd_config.group_id = gid;
}

hb_server *hb_core_server_create(void)
{
    hb_server *s = apr_pcalloc(hb_root, sizeof(*s));
    s->s.process = NULL;
    s->s.module_config = (ap_conf_vector_t *) s->vec;
    s->vec[0] = create_cfg(hb_root, &s->s);
    return s;
}

const char *hb_core_directive(hb_server *s, const char *name, int argc, const char **argv)
{
    const command_rec *cmd;
    cmd_parms parms;
    int i;
    for (cmd = ai_cmds; cmd->name; cmd++) if (!strcasecmp(cmd->name, name)) break;
    if (!cmd->name) return "unknown http-b directive";
    memset(&parms, 0, sizeof(parms));
    parms.info = cmd->cmd_data;
    parms.pool = hb_root;
    parms.temp_pool = hb_root;
    parms.server = &s->s;
    parms.cmd = cmd;
    for (i = 0; i < argc; i++) argv[i] = apr_pstrdup(hb_root, argv[i]);
    switch (cmd->args_how) {
    case FLAG:
        if (argc != 1) return "takes one argument, on or off";
        if (!strcasecmp(argv[0], "on")) return cmd->AP_FLAG(&parms, NULL, 1);
        if (!strcasecmp(argv[0], "off")) return cmd->AP_FLAG(&parms, NULL, 0);
        return "must be on or off";
    case TAKE1:
        if (argc != 1) return "takes one argument";
        return cmd->AP_TAKE1(&parms, NULL, argv[0]);
    case TAKE2:
        if (argc != 2) return "takes two arguments";
        return cmd->AP_TAKE2(&parms, NULL, argv[0], argv[1]);
    case TAKE12:
        if (argc < 1 || argc > 2) return "takes one or two arguments";
        return cmd->AP_TAKE2(&parms, NULL, argv[0], argc > 1 ? argv[1] : NULL);
    case TAKE23:
        if (argc < 2 || argc > 3) return "takes two or three arguments";
        return cmd->AP_TAKE3(&parms, NULL, argv[0], argv[1], argc > 2 ? argv[2] : NULL);
    case TAKE3:
        if (argc != 3) return "takes three arguments";
        return cmd->AP_TAKE3(&parms, NULL, argv[0], argv[1], argv[2]);
    case ITERATE:
        for (i = 0; i < argc; i++) { const char *e = cmd->AP_TAKE1(&parms, NULL, argv[i]); if (e) return e; }
        return NULL;
    case TAKE_ARGV:
        return cmd->AP_TAKE_ARGV(&parms, NULL, argc, (char *const *) argv);
    default:
        return "unsupported directive form";
    }
}

hb_server *hb_core_server_merge(hb_server *parent, hb_server *child)
{
    child->vec[0] = merge_cfg(hb_root, parent->vec[0], child->vec[0]);
    return child;
}

void hb_core_server_identity(hb_server *s, const char *hostname, unsigned port, hb_server *next)
{
    s->s.server_hostname = hostname ? apr_pstrdup(hb_root, hostname) : NULL;
    s->s.port = (apr_port_t) port;
    s->s.next = next ? &next->s : NULL;
}

const char *hb_core_post_config(hb_server *first)
{
    apr_pool_t *tmp;
    int rc;
    apr_pool_create(&tmp, hb_root);
    rc = ai_post_config(hb_root, hb_root, tmp, &first->s);
    apr_pool_destroy(tmp);
    return rc == OK ? NULL : "http-b configuration error (see the error log)";
}

void hb_core_child_init(hb_server *first) { ai_child_init(hb_root, &first->s); }
int hb_core_enabled(hb_server *s) { return cfg_enabled(s->vec[0]); }
size_t hb_core_max_buffer(hb_server *s) { return (size_t) cfg_maxbuf(s->vec[0]); }

hb_req *hb_core_req_begin(hb_server *s, const char *method, const char *uri, const char *args,
                          const char *host, const char *client_ip, const char *local_ip,
                          unsigned local_port, int https, const char *docroot,
                          const hb_header *h, int nh, int header_only)
{
    apr_pool_t *p;
    hb_req *q;
    request_rec *r;
    int i;
    char *hn;
    if (apr_pool_create(&p, NULL) != APR_SUCCESS) return NULL;
    q = apr_pcalloc(p, sizeof(*q));
    r = &q->r;
    q->srv = s;
    if (docroot) s->docroot = apr_pstrdup(hb_root, docroot);   /* same root for every request of a server */
    r->pool = p;
    r->server = &s->s;
    r->connection = &q->c;
    q->c.pool = p;
    q->c.base_server = &s->s;
    q->c.client_ip = apr_pstrdup(p, client_ip);
    q->c.local_ip = apr_pstrdup(p, local_ip);
    apr_sockaddr_info_get(&q->c.local_addr, q->c.local_ip, APR_UNSPEC, (apr_port_t) local_port, 0, p);
    apr_sockaddr_info_get(&q->c.client_addr, q->c.client_ip, APR_UNSPEC, 0, 0, p);
    q->c.bucket_alloc = apr_bucket_alloc_create(p);
    r->useragent_ip = q->c.client_ip;
    r->useragent_addr = q->c.client_addr;
    r->headers_in = apr_table_make(p, 16);
    r->headers_out = apr_table_make(p, 8);
    r->err_headers_out = apr_table_make(p, 4);
    r->subprocess_env = apr_table_make(p, 4);
    r->notes = apr_table_make(p, 8);
    for (i = 0; i < nh; i++) apr_table_add(r->headers_in, h[i].name, h[i].value);
    if (https) apr_table_setn(r->notes, "hb-https", "1");
    r->method = apr_pstrdup(p, method);
    r->method_number = !strcmp(method, "GET") || !strcmp(method, "HEAD") ? M_GET
                     : !strcmp(method, "POST") ? M_POST : !strcmp(method, "PUT") ? M_PUT
                     : !strcmp(method, "DELETE") ? M_DELETE : !strcmp(method, "OPTIONS") ? M_OPTIONS : M_INVALID;
    r->header_only = header_only;
    r->uri = apr_pstrdup(p, uri);
    r->args = args && *args ? apr_pstrdup(p, args) : NULL;
    r->unparsed_uri = r->args ? apr_pstrcat(p, r->uri, "?", r->args, NULL) : r->uri;
    r->parsed_uri.path = r->uri;
    r->parsed_uri.query = r->args;
    hn = apr_pstrdup(p, host ? host : (s->s.server_hostname ? s->s.server_hostname : "localhost"));
    if (*hn != '[' && strchr(hn, ':')) *strchr(hn, ':') = 0;
    r->hostname = hn;
    r->status = HTTP_OK;
    r->request_time = apr_time_now();
    q->fguide.r = q->flearn.r = q->sink_out.r = q->sink_null.r = r;
    q->fguide.c = q->flearn.c = q->sink_out.c = q->sink_null.c = &q->c;
    q->fguide.next = &q->sink_out;
    q->flearn.next = &q->sink_null;

    ai_post_read(r);
    ai_translate(r);
    return q;
}

void hb_core_req_end(hb_req *q) { if (q) apr_pool_destroy(q->r.pool); }

const char *hb_core_req_args(hb_req *q, int *changed)
{
    const char *orig = apr_table_get(q->r.notes, "ai-orig-uri");
    (void) orig;
    *changed = 1;
    return q->r.args;
}

int hb_core_req_endpoint(hb_req *q) { return apr_table_get(q->r.notes, "ai-endpoint") != NULL; }
int hb_core_req_mode(hb_req *q) { return get_mode(&q->r, NULL); }
int hb_core_req_has_header(hb_req *q, const char *name) { return apr_table_get(q->r.headers_in, name) != NULL; }
const char *hb_core_req_client_ip(hb_req *q) { return q->r.useragent_ip; }
const char *hb_core_secret(void) { return act_secret_g; }

int hb_core_handle(hb_req *q, const char *body, size_t len)
{
    int rc;
    q->body = body; q->blen = len; q->bpos = 0;
    rc = ai_handler(&q->r);
    if (rc == DECLINED) return HTTP_NOT_FOUND;
    if (rc != OK && rc != DONE) return rc;
    return 0;
}

void hb_core_resp_set(hb_req *q, int status, const char *ctype, const char *cenc)
{
    q->r.status = status;
    q->r.content_type = ctype ? apr_pstrdup(q->r.pool, ctype) : NULL;
    if (cenc) apr_table_set(q->r.headers_out, "Content-Encoding", cenc);
    else apr_table_unset(q->r.headers_out, "Content-Encoding");
}

int hb_core_insert_filters(hb_req *q, int is_error)
{
    q->filters = 0;
    if (is_error) ai_insert_error_filter(&q->r);
    else ai_insert_filter(&q->r);
    return q->filters;
}

void hb_core_filter_feed(hb_req *q, int which, const char *data, size_t len)
{
    apr_bucket_brigade *bb;
    if (q->removed & which) return;
    bb = apr_brigade_create(q->r.pool, q->c.bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, apr_bucket_transient_create(data, len, q->c.bucket_alloc));
    if (which == HB_FILTER_GUIDE) ai_guide_filter(&q->fguide, bb);
    else ai_learn_filter(&q->flearn, bb);
    apr_brigade_destroy(bb);
}

void hb_core_filter_finish(hb_req *q, int which)
{
    apr_bucket_brigade *bb;
    if (q->removed & which) return;
    bb = apr_brigade_create(q->r.pool, q->c.bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(bb, apr_bucket_eos_create(q->c.bucket_alloc));
    if (which == HB_FILTER_GUIDE) { q->out.len = 0; ai_guide_filter(&q->fguide, bb); }
    else ai_learn_filter(&q->flearn, bb);
    apr_brigade_destroy(bb);
}

typedef struct { hb_req *q; int n, cap; } hb_hcollect;

static int hb_collect(void *data, const char *k, const char *v)
{
    hb_hcollect *hc = data;
    if (!strcasecmp(k, "Content-Length") || !strcasecmp(k, "Content-Type")) return 1;
    if (hc->n == hc->cap) {
        int nc = hc->cap ? hc->cap * 2 : 8;
        hb_header *nh = apr_palloc(hc->q->r.pool, nc * sizeof(*nh));
        if (hc->n) memcpy(nh, hc->q->hdrs, hc->n * sizeof(*nh));
        hc->q->hdrs = nh; hc->cap = nc;
    }
    hc->q->hdrs[hc->n].name = k; hc->q->hdrs[hc->n].value = v; hc->n++;
    return 1;
}

int hb_core_out(hb_req *q, const char **ctype, hb_header **h, int *nh, const char **body, size_t *len, int extra_only)
{
    hb_hcollect hc = { q, 0, 0 };
    q->hdrs = NULL;
    if (!extra_only) apr_table_do(hb_collect, &hc, q->r.headers_out, NULL);
    apr_table_do(hb_collect, &hc, q->r.err_headers_out, NULL);
    *h = q->hdrs; *nh = hc.n;
    *ctype = q->r.content_type;
    *body = q->out.s ? q->out.s : "";
    *len = q->out.len;
    return q->r.status;
}
