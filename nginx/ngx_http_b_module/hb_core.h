/*
 * hb_core.h - the boundary between nginx and the shared http-b core.
 *
 * The core is mod_http_b.c itself, compiled unchanged against a small
 * Apache-API layer (hb_core.c). nginx code only sees the plain C types here,
 * so nginx and Apache headers never meet in one translation unit.
 */
#ifndef HB_CORE_H
#define HB_CORE_H
#include <stddef.h>

typedef struct hb_server hb_server;
typedef struct hb_req hb_req;
typedef struct { const char *name, *value; } hb_header;

#define HB_LOG_ERR 1
#define HB_LOG_WARN 2
#define HB_LOG_INFO 3
#define HB_LOG_DEBUG 4
typedef void (*hb_log_fn)(int level, const char *msg);

#define HB_FILTER_GUIDE 1
#define HB_FILTER_LEARN 2

int         hb_core_global_init(hb_log_fn log, const char *server_root, unsigned uid, unsigned gid);
void        hb_core_set_user(unsigned uid, unsigned gid);
hb_server  *hb_core_server_create(void);
const char *hb_core_directive(hb_server *s, const char *apache_name, int argc, const char **argv);
hb_server  *hb_core_server_merge(hb_server *parent, hb_server *child);
void        hb_core_server_identity(hb_server *s, const char *hostname, unsigned port, hb_server *next);
const char *hb_core_post_config(hb_server *first);
void        hb_core_child_init(hb_server *first);
int         hb_core_enabled(hb_server *s);
size_t      hb_core_max_buffer(hb_server *s);

/* request lifecycle */
hb_req     *hb_core_req_begin(hb_server *s, const char *method, const char *uri, const char *args,
                              const char *host, const char *client_ip, const char *local_ip,
                              unsigned local_port, int https, const char *docroot,
                              const hb_header *h, int nh, int header_only);
void        hb_core_req_end(hb_req *q);
const char *hb_core_req_args(hb_req *q, int *changed);
int         hb_core_req_endpoint(hb_req *q);
int         hb_core_req_mode(hb_req *q);
int         hb_core_req_has_header(hb_req *q, const char *name);
/* the address access rules must see: the agent's, also for requests the
 * core makes back to this server on the agent's behalf */
const char *hb_core_req_client_ip(hb_req *q);
/* secret that marks the core's own requests to this server */
const char *hb_core_secret(void);

/* endpoints: returns the HTTP status to send (0 = response written) */
int         hb_core_handle(hb_req *q, const char *body, size_t len);

/* response side */
void        hb_core_resp_set(hb_req *q, int status, const char *ctype, const char *cenc);
int         hb_core_insert_filters(hb_req *q, int is_error);
void        hb_core_filter_feed(hb_req *q, int which, const char *data, size_t len);
void        hb_core_filter_finish(hb_req *q, int which);

/* what the core produced: status, content type, headers, body */
int         hb_core_out(hb_req *q, const char **ctype, hb_header **h, int *nh,
                        const char **body, size_t *len, int extra_only);
#endif
