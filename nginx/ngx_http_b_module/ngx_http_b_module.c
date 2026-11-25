/*
 * ngx_http_b_module - http-b for nginx.
 *
 * Every feature of mod_http_b runs here: the nginx side below handles
 * configuration, request phases, filters and threads; the protocol itself is
 * the shared core in mod_http_b.c, driven through hb_core.h.
 *
 * Directives: every Apache directive AIAgentXyz is available as http_b_xyz
 * with the same arguments, in the http and server blocks.
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#if (NGX_THREADS)
#include <ngx_thread_pool.h>
#endif
#include "hb_core.h"

typedef struct {
    hb_server   *hb;
    ngx_http_core_srv_conf_t *cscf;
} ngx_http_b_srv_conf_t;

typedef struct {
    ngx_array_t  servers;      /* ngx_http_b_srv_conf_t * */
    ngx_uint_t   any_enabled;
#if (NGX_THREADS)
    ngx_thread_pool_t *tp;
#endif
} ngx_http_b_main_conf_t;

typedef struct {
    hb_req      *q;
    ngx_uint_t   endpoint;
    ngx_uint_t   filters;      /* HB_FILTER_* active on this response */
    ngx_uint_t   done;
    u_char      *body; size_t blen; ngx_int_t rc;
    ngx_http_request_t *r;
} ngx_http_b_ctx_t;

static ngx_int_t ngx_http_b_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_b_init_module(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_b_init_process(ngx_cycle_t *cycle);
static void *ngx_http_b_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_b_create_srv_conf(ngx_conf_t *cf);
static char *ngx_http_b_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_http_b_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt   ngx_http_next_body_filter;

static ngx_command_t ngx_http_b_commands[] = {
    { ngx_string("http_b"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgent" },
    { ngx_string("http_b_default_mode"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentDefaultMode" },
    { ngx_string("http_b_user_agent"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentUserAgent" },
    { ngx_string("http_b_builtin_user_agents"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentBuiltinUserAgents" },
    { ngx_string("http_b_site_name"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentSiteName" },
    { ngx_string("http_b_site_description"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentSiteDescription" },
    { ngx_string("http_b_contact"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentContact" },
    { ngx_string("http_b_prefix"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentPrefix" },
    { ngx_string("http_b_describe"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentDescribe" },
    { ngx_string("http_b_exclude"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentExclude" },
    { ngx_string("http_b_faq"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentFAQ" },
    { ngx_string("http_b_max_pages"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentMaxPages" },
    { ngx_string("http_b_max_buffer"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentMaxBuffer" },
    { ngx_string("http_b_max_text"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentMaxText" },
    { ngx_string("http_b_check_access"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentCheckAccess" },
    { ngx_string("http_b_llms_txt"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLlmsTxt" },
    { ngx_string("http_b_advertise"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAdvertise" },
    { ngx_string("http_b_ai_service"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAIService" },
    { ngx_string("http_b_ai_key"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAIKey" },
    { ngx_string("http_b_ai_model"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAIModel" },
    { ngx_string("http_b_ai_endpoint"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAIEndpoint" },
    { ngx_string("http_b_ai_timeout"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAITimeout" },
    { ngx_string("http_b_ai_max_tokens"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAIMaxTokens" },
    { ngx_string("http_b_ai_rate_limit"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAIRateLimit" },
    { ngx_string("http_b_ai_context"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAIContext" },
    { ngx_string("http_b_activation"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentActivation" },
    { ngx_string("http_b_learn_period"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLearnPeriod" },
    { ngx_string("http_b_learn_min_pages"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLearnMinPages" },
    { ngx_string("http_b_learn_max_pages"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLearnMaxPages" },
    { ngx_string("http_b_learn_traffic"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLearnTraffic" },
    { ngx_string("http_b_learn_query_strings"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLearnQueryStrings" },
    { ngx_string("http_b_knowledge_dir"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentKnowledgeDir" },
    { ngx_string("http_b_log_questions"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLogQuestions" },
    { ngx_string("http_b_learning"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentLearning" },
    { ngx_string("http_b_code_scan"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentCodeScan" },
    { ngx_string("http_b_source_dir"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentSourceDir" },
    { ngx_string("http_b_code_scan_interval"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentCodeScanInterval" },
    { ngx_string("http_b_actions"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentActions" },
    { ngx_string("http_b_act_rate_limit"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentActRateLimit" },
    { ngx_string("http_b_ads"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAds" },
    { ngx_string("http_b_ad"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAd" },
    { ngx_string("http_b_ads_file"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAdsFile" },
    { ngx_string("http_b_ads_max"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAdsMax" },
    { ngx_string("http_b_ads_required"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAdsRequired" },
    { ngx_string("http_b_ads_terms"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentAdsTerms" },
    { ngx_string("http_b_list_protected"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_b_directive, NGX_HTTP_SRV_CONF_OFFSET, 0, (void *) "AIAgentListProtected" },
    ngx_null_command
};

static ngx_http_module_t ngx_http_b_module_ctx = {
    NULL, ngx_http_b_init,
    ngx_http_b_create_main_conf, NULL,
    ngx_http_b_create_srv_conf, ngx_http_b_merge_srv_conf,
    NULL, NULL
};

ngx_module_t ngx_http_b_module = {
    NGX_MODULE_V1, &ngx_http_b_module_ctx, ngx_http_b_commands, NGX_HTTP_MODULE,
    NULL, ngx_http_b_init_module, ngx_http_b_init_process, NULL, NULL, NULL, NULL,
    NGX_MODULE_V1_PADDING
};

/* ---------------------------------------------------------------- */

static char *hb_cstr(ngx_pool_t *p, ngx_str_t *s)
{
    char *c = ngx_pnalloc(p, s->len + 1);
    if (c == NULL) return NULL;
    ngx_memcpy(c, s->data, s->len); c[s->len] = 0;
    return c;
}

static void hb_ngx_log(int level, const char *msg)
{
    ngx_uint_t l = level == HB_LOG_ERR ? NGX_LOG_ERR : level == HB_LOG_WARN ? NGX_LOG_WARN
                 : level == HB_LOG_INFO ? NGX_LOG_INFO : NGX_LOG_DEBUG;
    ngx_log_error(l, ngx_cycle->log, 0, "http-b: %s", msg);
}

/* The module context is wiped by internal redirects (index files,
 * error_page, try_files); keep it reachable through a pool cleanup. */
static void hb_ctx_cleanup(void *data)
{
    ngx_http_b_ctx_t *ctx = data;
    if (ctx->q) { hb_core_req_end(ctx->q); ctx->q = NULL; }
}

static ngx_http_b_ctx_t *hb_get_ctx(ngx_http_request_t *r)
{
    ngx_http_b_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_b_module);
    ngx_pool_cleanup_t *cln;
    if (ctx) return ctx;
    for (cln = r->pool->cleanup; cln; cln = cln->next) {
        if (cln->handler == hb_ctx_cleanup) {
            ctx = cln->data;
            ngx_http_set_ctx(r, ctx, ngx_http_b_module);
            return ctx;
        }
    }
    return NULL;
}

static ngx_int_t hb_add_header(ngx_http_request_t *r, const char *k, const char *v)
{
    ngx_table_elt_t *h;
    size_t kl = ngx_strlen(k), vl = ngx_strlen(v);
    h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) return NGX_ERROR;
    h->hash = 1; h->next = NULL;
    h->key.data = ngx_pnalloc(r->pool, kl); h->value.data = ngx_pnalloc(r->pool, vl);
    if (h->key.data == NULL || h->value.data == NULL) return NGX_ERROR;
    ngx_memcpy(h->key.data, k, kl); h->key.len = kl;
    ngx_memcpy(h->value.data, v, vl); h->value.len = vl;
    if (kl == 13 && ngx_strncasecmp((u_char *) k, (u_char *) "Cache-Control", 13) == 0) {
        r->headers_out.cache_control = h;
    }
    return NGX_OK;
}

/* hide a request header from nginx and from upstreams */
static void hb_drop_header(ngx_http_request_t *r, ngx_table_elt_t *h)
{
    static u_char dropped[] = "X-Http-B-Dropped";
    if (h == r->headers_in.if_modified_since) r->headers_in.if_modified_since = NULL;
    if (h == r->headers_in.if_none_match) r->headers_in.if_none_match = NULL;
    if (h == r->headers_in.if_unmodified_since) r->headers_in.if_unmodified_since = NULL;
    if (h == r->headers_in.if_match) r->headers_in.if_match = NULL;
    if (h == r->headers_in.if_range) r->headers_in.if_range = NULL;
    if (h == r->headers_in.range) r->headers_in.range = NULL;
#if (NGX_HTTP_GZIP || NGX_HTTP_HEADERS)
    if (h == r->headers_in.accept_encoding) r->headers_in.accept_encoding = NULL;
#endif
    if (h->key.len == 15 && ngx_strncasecmp(h->key.data, (u_char *) "Accept-Encoding", 15) == 0) {
        ngx_str_set(&h->value, "identity");
        return;
    }
    h->key.data = dropped; h->key.len = sizeof(dropped) - 1;
    h->lowcase_key = (u_char *) "x-http-b-dropped";
    h->hash = ngx_hash_key(h->lowcase_key, h->key.len);
    ngx_str_set(&h->value, "1");
}

/* Requests the core makes to this server on an agent's behalf carry the
 * agent's address; like the realip module, make nginx (allow/deny, logs,
 * upstreams) see that address for the rest of the request. */
typedef struct {
    ngx_connection_t *c;
    struct sockaddr *sockaddr; socklen_t socklen; ngx_str_t addr_text;
} hb_addr_save_t;

static void hb_addr_restore(void *data)
{
    hb_addr_save_t *s = data;
    s->c->sockaddr = s->sockaddr; s->c->socklen = s->socklen; s->c->addr_text = s->addr_text;
}

static ngx_int_t hb_set_client_addr(ngx_http_request_t *r, const char *ip)
{
    ngx_connection_t *c = r->connection;
    ngx_addr_t addr;
    ngx_str_t s;
    ngx_pool_cleanup_t *cln;
    hb_addr_save_t *sv;
    u_char *text;
    s.data = (u_char *) ip; s.len = ngx_strlen(ip);
    if (s.len == c->addr_text.len && ngx_strncmp(s.data, c->addr_text.data, s.len) == 0) return NGX_OK;
    if (ngx_parse_addr(c->pool, &addr, s.data, s.len) != NGX_OK) return NGX_DECLINED;
    cln = ngx_pool_cleanup_add(r->pool, sizeof(hb_addr_save_t));
    if (cln == NULL) return NGX_ERROR;
    sv = cln->data;
    sv->c = c; sv->sockaddr = c->sockaddr; sv->socklen = c->socklen; sv->addr_text = c->addr_text;
    cln->handler = hb_addr_restore;
    text = ngx_pnalloc(c->pool, s.len);
    if (text == NULL) return NGX_ERROR;
    ngx_memcpy(text, s.data, s.len);
    ngx_inet_set_port(addr.sockaddr, ngx_inet_get_port(c->sockaddr));
    c->sockaddr = addr.sockaddr; c->socklen = addr.socklen;
    c->addr_text.data = text; c->addr_text.len = s.len;
    return NGX_OK;
}

static int hb_is_loopback(ngx_connection_t *c)
{
    return (c->addr_text.len > 4 && ngx_strncmp(c->addr_text.data, "127.", 4) == 0)
        || (c->addr_text.len == 3 && ngx_strncmp(c->addr_text.data, "::1", 3) == 0);
}

/* ---------------------------------------------------------------- */
/* request phase                                                     */
/* ---------------------------------------------------------------- */

static ngx_int_t hb_content_handler(ngx_http_request_t *r);

static ngx_int_t ngx_http_b_rewrite_handler(ngx_http_request_t *r)
{
    ngx_http_b_srv_conf_t *bs;
    ngx_http_b_ctx_t *ctx;
    ngx_pool_cleanup_t *cln;
    ngx_list_part_t *part;
    ngx_table_elt_t *h;
    ngx_uint_t i, n;
    hb_header *hh;
    u_char addr[NGX_SOCKADDRLEN], *end;
    ngx_str_t local = { 0, addr }, path;
    size_t root = 0;
    char *method, *uri, *args, *host, *client, *lip, *docroot = NULL, *via = NULL;
    const char *nargs;
    int changed;
    in_port_t port;

    if (r != r->main) return NGX_DECLINED;
    if (hb_get_ctx(r)) return NGX_DECLINED;   /* already decided before a redirect */
    bs = ngx_http_get_module_srv_conf(r, ngx_http_b_module);
    if (bs->hb == NULL || !hb_core_enabled(bs->hb)) return NGX_DECLINED;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_b_ctx_t));
    if (ctx == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    ctx->r = r;
    ngx_http_set_ctx(r, ctx, ngx_http_b_module);
    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    cln->handler = hb_ctx_cleanup; cln->data = ctx;

    /* request headers */
    n = 0;
    for (part = &r->headers_in.headers.part; part; part = part->next) n += part->nelts;
    hh = ngx_palloc(r->pool, (n + 1) * sizeof(hb_header));
    if (hh == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    n = 0;
    for (part = &r->headers_in.headers.part; part; part = part->next) {
        h = part->elts;
        for (i = 0; i < part->nelts; i++) {
            if (h[i].hash == 0) continue;
            if (h[i].key.len == 15 && ngx_strncasecmp(h[i].key.data, (u_char *) "X-Http-B-Client", 15) == 0) {
                via = hb_cstr(r->pool, &h[i].value);
                hb_drop_header(r, &h[i]);
                continue;
            }
            hh[n].name = hb_cstr(r->pool, &h[i].key);
            hh[n].value = hb_cstr(r->pool, &h[i].value);
            n++;
        }
    }

    if (ngx_connection_local_sockaddr(r->connection, &local, 0) != NGX_OK) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    local.len = ngx_sock_ntop(r->connection->local_sockaddr, r->connection->local_socklen, addr,
                              NGX_SOCKADDRLEN - 1, 0);
    addr[local.len] = 0;
    port = ngx_inet_get_port(r->connection->local_sockaddr);

    end = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (end && root) {
        docroot = ngx_pnalloc(r->pool, root + 1);
        if (docroot) {
            ngx_memcpy(docroot, path.data, root); docroot[root] = 0;
            if (root > 1 && docroot[root - 1] == '/') docroot[root - 1] = 0;
        }
    }

    method = hb_cstr(r->pool, &r->method_name);
    uri = hb_cstr(r->pool, &r->uri);
    args = hb_cstr(r->pool, &r->args);
    host = r->headers_in.host ? hb_cstr(r->pool, &r->headers_in.host->value) : hb_cstr(r->pool, &r->headers_in.server);
    client = hb_cstr(r->pool, &r->connection->addr_text);
    lip = (char *) addr;
    if (via) {   /* "<secret> <agent address>", only from this server itself */
        const char *secret = hb_core_secret();
        size_t sl = ngx_strlen(secret);
        if (sl && hb_is_loopback(r->connection) && ngx_strncmp(via, secret, sl) == 0 && via[sl] == ' ') {
            client = via + sl + 1;
            if (hb_set_client_addr(r, client) == NGX_ERROR) return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    if (!method || !uri || !args || !host || !client) return NGX_HTTP_INTERNAL_SERVER_ERROR;

    ctx->q = hb_core_req_begin(bs->hb, method, uri, args, host, client, lip, port,
#if (NGX_HTTP_SSL)
                               r->connection->ssl != NULL,
#else
                               0,
#endif
                               docroot, hh, (int) n, r->method == NGX_HTTP_HEAD);
    if (ctx->q == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    if (hb_core_req_client_ip(ctx->q) && hb_set_client_addr(r, hb_core_req_client_ip(ctx->q)) == NGX_ERROR)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    nargs = hb_core_req_args(ctx->q, &changed);
    if (nargs == NULL) nargs = "";
    if (ngx_strlen(nargs) != r->args.len || ngx_strncmp(nargs, r->args.data, r->args.len) != 0) {
        r->args.len = ngx_strlen(nargs);
        r->args.data = ngx_pnalloc(r->pool, r->args.len + 1);
        if (r->args.data == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
        ngx_memcpy(r->args.data, nargs, r->args.len);
        r->valid_unparsed_uri = 0;   /* upstreams rebuild the URI without ai= */
    }

    /* headers the core removed (conditionals, Accept-Encoding, X-AI-Agent-Act) */
    for (part = &r->headers_in.headers.part; part; part = part->next) {
        h = part->elts;
        for (i = 0; i < part->nelts; i++) {
            char *k;
            if (h[i].hash == 0) continue;
            k = hb_cstr(r->pool, &h[i].key);
            if (k && !hb_core_req_has_header(ctx->q, k)) hb_drop_header(r, &h[i]);
        }
    }
    if (hb_core_req_mode(ctx->q) == 2 || hb_core_req_mode(ctx->q) == 3) {
#if (NGX_HTTP_GZIP)
        r->gzip_tested = 1; r->gzip_ok = 0;
#endif
    }

    if (hb_core_req_endpoint(ctx->q)) {
        ctx->endpoint = 1;
        r->content_handler = hb_content_handler;
    }
    return NGX_DECLINED;
}

/* ---------------------------------------------------------------- */
/* endpoints: run in a thread, because they may call AI services and */
/* make HTTP requests back to this server                           */
/* ---------------------------------------------------------------- */

static ngx_int_t hb_send_result(ngx_http_request_t *r, ngx_http_b_ctx_t *ctx)
{
    const char *ctype, *body;
    hb_header *h; int nh, k, status;
    size_t len;
    ngx_buf_t *b; ngx_chain_t out; ngx_int_t rc;

    if (ctx->rc) return ctx->rc;
    status = hb_core_out(ctx->q, &ctype, &h, &nh, &body, &len, 0);
    r->headers_out.status = status ? (ngx_uint_t) status : NGX_HTTP_OK;
    if (ctype) {
        r->headers_out.content_type.len = ngx_strlen(ctype);
        r->headers_out.content_type.data = ngx_pnalloc(r->pool, r->headers_out.content_type.len);
        if (r->headers_out.content_type.data == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
        ngx_memcpy(r->headers_out.content_type.data, ctype, r->headers_out.content_type.len);
        r->headers_out.content_type_len = r->headers_out.content_type.len;
    }
    for (k = 0; k < nh; k++) {
        if (hb_add_header(r, h[k].name, h[k].value) != NGX_OK) return NGX_ERROR;
    }
    r->headers_out.content_length_n = len;
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only || len == 0) {
        if (len == 0 && rc == NGX_OK && !r->header_only) return ngx_http_send_special(r, NGX_HTTP_LAST);
        return rc;
    }
    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) return NGX_ERROR;
    b->last = ngx_cpymem(b->pos, body, len);
    b->last_buf = 1; b->last_in_chain = 1;
    out.buf = b; out.next = NULL;
    return ngx_http_output_filter(r, &out);
}

#if (NGX_THREADS)
static void hb_thread_run(void *data, ngx_log_t *log)
{
    ngx_http_b_ctx_t *ctx = data;
    ctx->rc = hb_core_handle(ctx->q, (const char *) ctx->body, ctx->blen);
}

static void hb_thread_done(ngx_event_t *ev)
{
    ngx_http_b_ctx_t *ctx = ev->data;
    ngx_http_request_t *r = ctx->r;
    ngx_connection_t *c = r->connection;
    r->main->blocked--;
    r->aio = 0;
    ngx_http_finalize_request(r, hb_send_result(r, ctx));
    ngx_http_run_posted_requests(c);
}
#endif

static void hb_body_ready(ngx_http_request_t *r)
{
    ngx_http_b_ctx_t *ctx = hb_get_ctx(r);
    ngx_chain_t *cl;
    size_t len = 0;
    u_char *p;

    if (ctx == NULL || ctx->q == NULL) { ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR); return; }
    if (r->request_body && r->request_body->bufs) {
        for (cl = r->request_body->bufs; cl; cl = cl->next)
            len += cl->buf->in_file ? (size_t) (cl->buf->file_last - cl->buf->file_pos)
                                    : (size_t) (cl->buf->last - cl->buf->pos);
        p = ctx->body = ngx_pnalloc(r->pool, len + 1);
        if (p == NULL) { ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR); return; }
        for (cl = r->request_body->bufs; cl; cl = cl->next) {
            if (cl->buf->in_file) {
                ssize_t n = ngx_read_file(cl->buf->file, p, (size_t) (cl->buf->file_last - cl->buf->file_pos),
                                          cl->buf->file_pos);
                if (n < 0) { ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR); return; }
                p += n;
            } else p = ngx_cpymem(p, cl->buf->pos, cl->buf->last - cl->buf->pos);
        }
        ctx->blen = p - ctx->body;
    }
#if (NGX_THREADS)
    {
        ngx_http_b_main_conf_t *mc = ngx_http_get_module_main_conf(r, ngx_http_b_module);
        if (mc->tp) {
            ngx_thread_task_t *task = ngx_thread_task_alloc(r->pool, 0);
            if (task == NULL) { ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR); return; }
            task->ctx = ctx;
            task->handler = hb_thread_run;
            task->event.data = ctx;
            task->event.handler = hb_thread_done;
            if (ngx_thread_task_post(mc->tp, task) != NGX_OK) {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR); return;
            }
            r->main->blocked++;
            r->aio = 1;
            return;
        }
    }
#endif
    ctx->rc = hb_core_handle(ctx->q, (const char *) ctx->body, ctx->blen);
    ngx_http_finalize_request(r, hb_send_result(r, ctx));
}

static ngx_int_t hb_content_handler(ngx_http_request_t *r)
{
    ngx_int_t rc;
    r->request_body_in_single_buf = 1;
    rc = ngx_http_read_client_request_body(r, hb_body_ready);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) return rc;
    return NGX_DONE;
}

/* ---------------------------------------------------------------- */
/* response filters                                                  */
/* ---------------------------------------------------------------- */

/* the core "sets" these headers (Apache apr_table_set): drop what the page had */
static void hb_unset_header(ngx_http_request_t *r, const char *name)
{
    ngx_list_part_t *part;
    ngx_table_elt_t *h;
    ngx_uint_t i;
    size_t n = ngx_strlen(name);
    for (part = &r->headers_out.headers.part; part; part = part->next) {
        h = part->elts;
        for (i = 0; i < part->nelts; i++)
            if (h[i].hash && h[i].key.len == n && ngx_strncasecmp(h[i].key.data, (u_char *) name, n) == 0)
                h[i].hash = 0;
    }
    if (n == 13 && ngx_strncasecmp((u_char *) name, (u_char *) "Cache-Control", 13) == 0)
        r->headers_out.cache_control = NULL;
}

static ngx_int_t hb_apply_core_headers(ngx_http_request_t *r, ngx_http_b_ctx_t *ctx, int extra_only)
{
    const char *ctype, *body; hb_header *h; int nh, k; size_t len;
    hb_core_out(ctx->q, &ctype, &h, &nh, &body, &len, extra_only);
    for (k = 0; k < nh; k++) {
        if (!extra_only && ngx_strcasecmp((u_char *) h[k].name, (u_char *) "Content-Encoding") == 0) continue;
        if (!extra_only && (ngx_strcasecmp((u_char *) h[k].name, (u_char *) "Cache-Control") == 0
                            || ngx_strcasecmp((u_char *) h[k].name, (u_char *) "Content-Language") == 0
                            || ngx_strcasecmp((u_char *) h[k].name, (u_char *) "X-AI-Mode") == 0))
            hb_unset_header(r, h[k].name);
        if (hb_add_header(r, h[k].name, h[k].value) != NGX_OK) return NGX_ERROR;
    }
    return NGX_OK;
}

static ngx_int_t ngx_http_b_header_filter(ngx_http_request_t *r)
{
    ngx_http_b_ctx_t *ctx;
    char *ct = NULL, *enc = NULL;
    ngx_int_t status;

    if (r != r->main) return ngx_http_next_header_filter(r);
    ctx = hb_get_ctx(r);
    if (ctx == NULL || ctx->q == NULL || ctx->endpoint || ctx->done) return ngx_http_next_header_filter(r);
    ctx->done = 1;

    status = r->headers_out.status ? (ngx_int_t) r->headers_out.status : 200;
    if (r->headers_out.content_type.len) {
        ngx_str_t full = r->headers_out.content_type;
        if (r->headers_out.charset.len) {
            full.len = r->headers_out.content_type.len + r->headers_out.charset.len + 10;
            full.data = ngx_pnalloc(r->pool, full.len);
            if (full.data == NULL) return NGX_ERROR;
            full.len = ngx_sprintf(full.data, "%V; charset=%V", &r->headers_out.content_type,
                                   &r->headers_out.charset) - full.data;
        }
        ct = hb_cstr(r->pool, &full);
    }
    if (r->headers_out.content_encoding && r->headers_out.content_encoding->hash)
        enc = hb_cstr(r->pool, &r->headers_out.content_encoding->value);
    hb_core_resp_set(ctx->q, (int) status, ct, enc);
    ctx->filters = hb_core_insert_filters(ctx->q, status >= 400);

    if (ctx->filters & HB_FILTER_GUIDE) {
        if (r->header_only) {          /* HEAD: nothing to transform */
            ctx->filters = 0;
            if (hb_apply_core_headers(r, ctx, 1) != NGX_OK) return NGX_ERROR;
            return ngx_http_next_header_filter(r);
        }
        r->filter_need_in_memory = 1;
        return NGX_OK;                 /* headers go out with the transformed body */
    }
    if (hb_apply_core_headers(r, ctx, 1) != NGX_OK) return NGX_ERROR;
    if (ctx->filters & HB_FILTER_LEARN) r->filter_need_in_memory = 1;
    return ngx_http_next_header_filter(r);
}

static ngx_int_t hb_feed_chain(ngx_http_request_t *r, ngx_http_b_ctx_t *ctx, int which, ngx_chain_t *in, int consume,
                               ngx_uint_t *last)
{
    ngx_chain_t *cl;
    for (cl = in; cl; cl = cl->next) {
        ngx_buf_t *b = cl->buf;
        if (ngx_buf_in_memory(b) && b->last > b->pos) {
            hb_core_filter_feed(ctx->q, which, (const char *) b->pos, b->last - b->pos);
        } else if (b->in_file && b->file_last > b->file_pos) {
            size_t n = (size_t) (b->file_last - b->file_pos);
            u_char *tmp = ngx_pnalloc(r->pool, n);
            ssize_t got;
            if (tmp == NULL) return NGX_ERROR;
            got = ngx_read_file(b->file, tmp, n, b->file_pos);
            if (got > 0) hb_core_filter_feed(ctx->q, which, (const char *) tmp, (size_t) got);
        }
        if (consume) { b->pos = b->last; if (b->in_file) b->file_pos = b->file_last; }
        if (b->last_buf) *last = 1;
    }
    return NGX_OK;
}

static ngx_int_t ngx_http_b_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_b_ctx_t *ctx;
    ngx_uint_t last = 0;

    if (r != r->main || in == NULL) return ngx_http_next_body_filter(r, in);
    ctx = hb_get_ctx(r);
    if (ctx == NULL || ctx->q == NULL || ctx->endpoint || !ctx->filters) return ngx_http_next_body_filter(r, in);

    if (ctx->filters & HB_FILTER_GUIDE) {
        const char *ctype, *body; hb_header *h; int nh; size_t len;
        ngx_buf_t *b; ngx_chain_t out; ngx_int_t rc;
        int status;
        if (hb_feed_chain(r, ctx, HB_FILTER_GUIDE, in, 1, &last) != NGX_OK) return NGX_ERROR;
        if (!last) return NGX_OK;
        hb_core_filter_finish(ctx->q, HB_FILTER_GUIDE);
        ctx->filters = 0;
        status = hb_core_out(ctx->q, &ctype, &h, &nh, &body, &len, 0);
        if (status) r->headers_out.status = status;
        if (ctype) {
            r->headers_out.content_type.len = ngx_strlen(ctype);
            r->headers_out.content_type.data = (u_char *) ctype;
            r->headers_out.content_type_len = r->headers_out.content_type.len;
            r->headers_out.content_type_lowcase = NULL;
            r->headers_out.charset.len = 0;
        }
        ngx_http_clear_content_length(r);
        ngx_http_clear_accept_ranges(r);
        ngx_http_clear_etag(r);
        if (r->headers_out.content_encoding) { r->headers_out.content_encoding->hash = 0; r->headers_out.content_encoding = NULL; }
        r->headers_out.content_length_n = len;
        if (hb_apply_core_headers(r, ctx, 0) != NGX_OK) return NGX_ERROR;
        rc = ngx_http_next_header_filter(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) return rc;
        b = ngx_create_temp_buf(r->pool, len ? len : 1);
        if (b == NULL) return NGX_ERROR;
        b->last = ngx_cpymem(b->pos, body, len);
        b->last_buf = 1; b->last_in_chain = 1;
        out.buf = b; out.next = NULL;
        return ngx_http_next_body_filter(r, &out);
    }
    if (ctx->filters & HB_FILTER_LEARN) {
        if (hb_feed_chain(r, ctx, HB_FILTER_LEARN, in, 0, &last) != NGX_OK) return NGX_ERROR;
        if (last) { hb_core_filter_finish(ctx->q, HB_FILTER_LEARN); ctx->filters = 0; }
    }
    return ngx_http_next_body_filter(r, in);
}

/* ---------------------------------------------------------------- */
/* configuration                                                     */
/* ---------------------------------------------------------------- */

static char *ngx_http_b_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_b_srv_conf_t *bs = conf;
    ngx_str_t *v = cf->args->elts;
    const char *argv[16], *err;
    ngx_uint_t i;
    if (cf->args->nelts - 1 > 16) return "has too many arguments";
    for (i = 1; i < cf->args->nelts; i++) {
        argv[i - 1] = hb_cstr(cf->pool, &v[i]);
        if (argv[i - 1] == NULL) return NGX_CONF_ERROR;
    }
    err = hb_core_directive(bs->hb, (const char *) cmd->post, (int) cf->args->nelts - 1, argv);
    if (err) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "\"%V\" (%s): %s", &cmd->name, (char *) cmd->post, err);
        return NGX_CONF_ERROR;
    }
    return NGX_CONF_OK;
}

static void *ngx_http_b_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_b_main_conf_t *mc = ngx_pcalloc(cf->pool, sizeof(*mc));
    char *prefix;
    if (mc == NULL) return NULL;
    if (ngx_array_init(&mc->servers, cf->pool, 4, sizeof(ngx_http_b_srv_conf_t *)) != NGX_OK) return NULL;
    prefix = hb_cstr(cf->pool, &cf->cycle->prefix);
    if (prefix && ngx_strlen(prefix) > 1 && prefix[ngx_strlen(prefix) - 1] == '/') prefix[ngx_strlen(prefix) - 1] = 0;
    if (hb_core_global_init(hb_ngx_log, prefix, 0, 0) != 0)
        return NULL;
    return mc;
}

static void *ngx_http_b_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_b_srv_conf_t *bs = ngx_pcalloc(cf->pool, sizeof(*bs));
    if (bs == NULL) return NULL;
    bs->hb = hb_core_server_create();
    return bs;
}

static char *ngx_http_b_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_b_srv_conf_t *prev = parent, *bs = child, **slot;
    ngx_http_b_main_conf_t *mc = ngx_http_conf_get_module_main_conf(cf, ngx_http_b_module);
    bs->hb = hb_core_server_merge(prev->hb, bs->hb);
    bs->cscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_core_module);
    slot = ngx_array_push(&mc->servers);
    if (slot == NULL) return NGX_CONF_ERROR;
    *slot = bs;
    if (hb_core_enabled(bs->hb)) mc->any_enabled = 1;
    return NGX_CONF_OK;
}

static ngx_http_b_main_conf_t *hb_main_conf(ngx_cycle_t *cycle)
{
    ngx_http_conf_ctx_t *hc = (ngx_http_conf_ctx_t *) cycle->conf_ctx[ngx_http_module.index];
    if (hc == NULL) return NULL;
    return hc->main_conf[ngx_http_b_module.ctx_index];
}

static ngx_int_t ngx_http_b_init_module(ngx_cycle_t *cycle)
{
    ngx_http_b_main_conf_t *mc = hb_main_conf(cycle);
    ngx_http_b_srv_conf_t **sv;
    ngx_uint_t i;
    const char *err;
    ngx_core_conf_t *ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);
    if (mc == NULL || !mc->any_enabled || mc->servers.nelts == 0) return NGX_OK;
    /* knowledge directories are created by the master and handed to the worker user */
    if (ccf && ccf->user != (ngx_uid_t) NGX_CONF_UNSET_UINT) hb_core_set_user(ccf->user, ccf->group);
    sv = mc->servers.elts;
    for (i = 0; i < mc->servers.nelts; i++) {
        char *name = NULL;
        if (sv[i]->cscf && sv[i]->cscf->server_name.len) name = hb_cstr(cycle->pool, &sv[i]->cscf->server_name);
        hb_core_server_identity(sv[i]->hb, name, 0, i + 1 < mc->servers.nelts ? sv[i + 1]->hb : NULL);
    }
    err = hb_core_post_config(sv[0]->hb);
    if (err) { ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "%s", err); return NGX_ERROR; }
    return NGX_OK;
}

static ngx_int_t ngx_http_b_init_process(ngx_cycle_t *cycle)
{
    ngx_http_b_main_conf_t *mc = hb_main_conf(cycle);
    ngx_http_b_srv_conf_t **sv;
    if (mc == NULL || !mc->any_enabled || mc->servers.nelts == 0) return NGX_OK;
    sv = mc->servers.elts;
    hb_core_child_init(sv[0]->hb);
#if (NGX_THREADS)
    mc->tp = ngx_thread_pool_get(cycle, &(ngx_str_t) ngx_string("default"));
#endif
    return NGX_OK;
}

static ngx_int_t ngx_http_b_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    ngx_http_handler_pt *h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
    if (h == NULL) return NGX_ERROR;
    *h = ngx_http_b_rewrite_handler;
#if (NGX_THREADS)
    if (ngx_thread_pool_add(cf, NULL) == NULL) return NGX_ERROR;
#endif
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_b_header_filter;
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_b_body_filter;
    return NGX_OK;
}
