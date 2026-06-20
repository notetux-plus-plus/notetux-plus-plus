#include "lsp.h"
#include "lsp_json.h"
#include "editor.h"
#include "sci_c.h"
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static sptr_t sci_msg(GtkWidget *sci, unsigned int m, uptr_t w, sptr_t l)
{
    return scintilla_send_message(SCINTILLA(sci), m, w, l);
}

/* ════════════════════════════════════════════════════════════════════════
 * Configuration — ~/.config/notetux/lsp_servers.xml
 *
 *   <LSPServers>
 *     <Server lang="c"      cmd="clangd" args="--background-index"/>
 *     <Server lang="cpp"    cmd="clangd" args="--background-index"/>
 *     <Server lang="python" cmd="pylsp"/>
 *     <Server lang="rust"   cmd="rust-analyzer"/>
 *   </LSPServers>
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX_SERVERS 32
#define DIAG_ERROR_INDICATOR   10   /* red squiggle  */
#define DIAG_WARNING_INDICATOR 11   /* yellow squiggle */

typedef struct {
    char lang[64];
    char cmd[256];
    char args[512];
} ServerCfg;

/* ── Per-server state ───────────────────────────────────────────────── */

typedef enum {
    SRV_IDLE,         /* not started */
    SRV_STARTING,     /* subprocess spawned, waiting for initialize response */
    SRV_READY,        /* initialized, accepting requests */
    SRV_FAILED        /* spawn or protocol error */
} SrvState;

typedef struct {
    char           *filepath;   /* owned */
    int             version;
} OpenDoc;

typedef struct {
    /* Pending completion request (only one outstanding at a time) */
    int              comp_id;
    LspCompletionCb  comp_cb;
    void            *comp_userdata;
    /* Pending definition request */
    int              def_id;
    GtkWidget       *def_sci;
    /* Pending hover request */
    int              hover_id;
    GtkWidget       *hover_sci;
    /* Pending rename request */
    int              rename_id;
    GtkWidget       *rename_sci;
    char             rename_new[256];
} PendingReqs;

typedef struct LspServer LspServer;
struct LspServer {
    ServerCfg    cfg;
    SrvState     state;
    GSubprocess *proc;
    GOutputStream *stdin_pipe;
    GInputStream  *stdout_pipe;
    GThread      *reader;
    GMutex        write_mutex;
    int           next_id;
    PendingReqs   pending;
    /* Open documents tracked by this server */
    GPtrArray    *open_docs;   /* of OpenDoc* */
    /* Buffered messages queued while SRV_STARTING */
    GPtrArray    *queued_msgs; /* of char* (full JSON body, g_free'd) */
};

/* ── Diagnostic cache ───────────────────────────────────────────────── */

typedef struct {
    char *filepath;
    int   error_count;
    int   warning_count;
    /* Per-line ranges to paint.  We keep the raw LSP Diagnostic objects
     * so we can repaint after a theme change.  For now we only store
     * the indicator spans. */
    GPtrArray *spans; /* of DiagSpan* */
} DiagCache;

typedef struct {
    int  line;
    int  col_start;
    int  col_end;
    int  severity;   /* 1=error, 2=warning, 3=info, 4=hint */
} DiagSpan;

/* ── Module globals ─────────────────────────────────────────────────── */

static GtkWidget   *s_window      = NULL;
static ServerCfg    s_cfg[MAX_SERVERS];
static int          s_cfg_count   = 0;
static LspServer   *s_servers[MAX_SERVERS];
static int          s_srv_count   = 0;
static GHashTable  *s_diag_cache  = NULL; /* filepath → DiagCache* */
static GMutex       s_diag_mutex;

/* ════════════════════════════════════════════════════════════════════════
 * Config loading
 * ════════════════════════════════════════════════════════════════════════ */

typedef struct { gboolean in_server; char lang[64]; char cmd[256]; char args[512]; } CfgParse;

static void cfg_start(GMarkupParseContext *ctx G_GNUC_UNUSED,
                       const char *elem, const char **names, const char **vals,
                       void *ud, GError **err G_GNUC_UNUSED)
{
    CfgParse *p = ud;
    if (strcmp(elem, "Server") == 0) {
        p->lang[0] = p->cmd[0] = p->args[0] = '\0';
        for (int i = 0; names[i]; i++) {
            if      (strcmp(names[i], "lang") == 0) g_strlcpy(p->lang, vals[i], sizeof(p->lang));
            else if (strcmp(names[i], "cmd")  == 0) g_strlcpy(p->cmd,  vals[i], sizeof(p->cmd));
            else if (strcmp(names[i], "args") == 0) g_strlcpy(p->args, vals[i], sizeof(p->args));
        }
        p->in_server = TRUE;
    }
}

static void cfg_end(GMarkupParseContext *ctx G_GNUC_UNUSED,
                     const char *elem, void *ud, GError **err G_GNUC_UNUSED)
{
    CfgParse *p = ud;
    if (strcmp(elem, "Server") == 0 && p->in_server && p->lang[0] && p->cmd[0]) {
        if (s_cfg_count < MAX_SERVERS) {
            g_strlcpy(s_cfg[s_cfg_count].lang, p->lang, sizeof(s_cfg[0].lang));
            g_strlcpy(s_cfg[s_cfg_count].cmd,  p->cmd,  sizeof(s_cfg[0].cmd));
            g_strlcpy(s_cfg[s_cfg_count].args, p->args, sizeof(s_cfg[0].args));
            s_cfg_count++;
        }
        p->in_server = FALSE;
    }
}

static void load_config(void)
{
    const char *home = g_get_home_dir();
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/notetux/lsp_servers.xml", home);

    gchar *contents = NULL;
    gsize  len      = 0;
    if (!g_file_get_contents(path, &contents, &len, NULL))
        return; /* no config = no servers, that's fine */

    GMarkupParser parser = { cfg_start, cfg_end, NULL, NULL, NULL };
    CfgParse      state  = { FALSE, "", "", "" };
    GMarkupParseContext *ctx = g_markup_parse_context_new(&parser, 0, &state, NULL);
    g_markup_parse_context_parse(ctx, contents, (gssize)len, NULL);
    g_markup_parse_context_free(ctx);
    g_free(contents);
}

/* ════════════════════════════════════════════════════════════════════════
 * JSON-RPC transport
 * ════════════════════════════════════════════════════════════════════════ */

/* Write a JSON-RPC message to the server's stdin (called on main thread,
 * protected by write_mutex). */
static void srv_send(LspServer *srv, const char *body)
{
    if (!srv->stdin_pipe) return;
    gsize bodylen = strlen(body);
    char header[64];
    gsize hlen = (gsize)snprintf(header, sizeof(header),
                                  "Content-Length: %zu\r\n\r\n", bodylen);
    g_mutex_lock(&srv->write_mutex);
    g_output_stream_write_all(srv->stdin_pipe, header, hlen,  NULL, NULL, NULL);
    g_output_stream_write_all(srv->stdin_pipe, body,   bodylen, NULL, NULL, NULL);
    g_mutex_unlock(&srv->write_mutex);
}

/* Send a JSON-RPC request (has id).  Returns the id used. */
static int srv_request(LspServer *srv, const char *method, const char *params_json)
{
    int id = ++srv->next_id;
    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_kstr(&b, "jsonrpc", "2.0");
    jb_kint(&b, "id", id);
    jb_kstr(&b, "method", method);
    if (params_json && *params_json) {
        jb_key(&b, "params");
        g_string_append(b.s, params_json);
        b.need_comma = TRUE;
    }
    jb_end_obj(&b);
    srv_send(srv, jb_get(&b));
    jb_free(&b);
    return id;
}

/* Send a JSON-RPC notification (no id). */
static void srv_notify(LspServer *srv, const char *method, const char *params_json)
{
    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_kstr(&b, "jsonrpc", "2.0");
    jb_kstr(&b, "method", method);
    if (params_json && *params_json) {
        jb_key(&b, "params");
        g_string_append(b.s, params_json);
        b.need_comma = TRUE;
    }
    jb_end_obj(&b);
    if (srv->state == SRV_STARTING) {
        /* Queue until initialized */
        g_ptr_array_add(srv->queued_msgs, g_strdup(jb_get(&b)));
    } else {
        srv_send(srv, jb_get(&b));
    }
    jb_free(&b);
}

/* ════════════════════════════════════════════════════════════════════════
 * LSP message builders
 * ════════════════════════════════════════════════════════════════════════ */

static char *build_initialize_params(void)
{
    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_kint(&b, "processId", (int)getpid());
    jb_key(&b, "clientInfo");
    jb_begin_obj(&b);
    jb_kstr(&b, "name", "notetux++");
    jb_kstr(&b, "version", "1.0");
    jb_end_obj(&b);
    jb_key(&b, "capabilities");
    jb_begin_obj(&b);
    /* textDocument capabilities */
    jb_key(&b, "textDocument");
    jb_begin_obj(&b);
    /* publishDiagnostics */
    jb_key(&b, "publishDiagnostics");
    jb_begin_obj(&b);
    jb_kbool(&b, "relatedInformation", FALSE);
    jb_end_obj(&b);
    /* completion */
    jb_key(&b, "completion");
    jb_begin_obj(&b);
    jb_key(&b, "completionItem");
    jb_begin_obj(&b);
    jb_kbool(&b, "snippetSupport", FALSE);
    jb_end_obj(&b);
    jb_end_obj(&b);
    /* hover */
    jb_key(&b, "hover");
    jb_begin_obj(&b);
    jb_key(&b, "contentFormat");
    jb_begin_arr(&b);
    jb_str(&b, "plaintext");
    jb_end_arr(&b);
    jb_end_obj(&b);
    /* definition */
    jb_key(&b, "definition");
    jb_begin_obj(&b);
    jb_kbool(&b, "linkSupport", FALSE);
    jb_end_obj(&b);
    /* rename */
    jb_key(&b, "rename");
    jb_begin_obj(&b);
    jb_kbool(&b, "prepareSupport", FALSE);
    jb_end_obj(&b);
    jb_end_obj(&b); /* textDocument */
    jb_end_obj(&b); /* capabilities */
    jb_end_obj(&b);
    return g_strdup(jb_get(&b));
    /* caller frees */
    jb_free(&b);
}

static char *build_text_document_item(const char *filepath, const char *lang_id,
                                       int version, const char *text)
{
    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    char uri[2048];
    snprintf(uri, sizeof(uri), "file://%s", filepath);
    jb_kstr(&b, "uri", uri);
    jb_kstr(&b, "languageId", lang_id);
    jb_kint(&b, "version", version);
    jb_kstr(&b, "text", text);
    jb_end_obj(&b);
    char *r = g_strdup(jb_get(&b));
    jb_free(&b);
    return r;
}

static char *build_position_params(const char *filepath, int line, int col)
{
    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_key(&b, "textDocument");
    jb_begin_obj(&b);
    char uri[2048];
    snprintf(uri, sizeof(uri), "file://%s", filepath);
    jb_kstr(&b, "uri", uri);
    jb_end_obj(&b);
    jb_key(&b, "position");
    jb_begin_obj(&b);
    jb_kint(&b, "line", line);
    jb_kint(&b, "character", col);
    jb_end_obj(&b);
    jb_end_obj(&b);
    char *r = g_strdup(jb_get(&b));
    jb_free(&b);
    return r;
}

/* ════════════════════════════════════════════════════════════════════════
 * Diagnostic painting (main thread)
 * ════════════════════════════════════════════════════════════════════════ */

/* Find the Scintilla widget currently showing filepath */
static GtkWidget *sci_for_path(const char *filepath)
{
    int n = editor_page_count();
    for (int i = 0; i < n; i++) {
        NppDoc *d = editor_doc_at(i);
        if (d && d->filepath && strcmp(d->filepath, filepath) == 0)
            return d->sci;
    }
    return NULL;
}

static void paint_diagnostics(const char *filepath, GPtrArray *spans)
{
    GtkWidget *sci = sci_for_path(filepath);
    if (!sci) return;

    /* Clear previous indicators */
    glong len = sci_msg(sci, SCI_GETLENGTH, 0, 0);

    sci_msg(sci, SCI_SETINDICATORCURRENT, DIAG_ERROR_INDICATOR, 0);
    sci_msg(sci, SCI_INDICATORCLEARRANGE, 0, len);
    sci_msg(sci, SCI_SETINDICATORCURRENT, DIAG_WARNING_INDICATOR, 0);
    sci_msg(sci, SCI_INDICATORCLEARRANGE, 0, len);

    if (!spans) return;
    for (guint i = 0; i < spans->len; i++) {
        DiagSpan *sp = g_ptr_array_index(spans, i);
        glong line_start = sci_msg(sci, SCI_POSITIONFROMLINE, sp->line, 0);
        glong col_s = line_start + sp->col_start;
        glong col_e = line_start + sp->col_end;
        if (col_s >= len) continue;
        if (col_e > len)  col_e = len;
        if (col_s == col_e) col_e = col_s + 1;

        int ind = (sp->severity <= 1) ? DIAG_ERROR_INDICATOR : DIAG_WARNING_INDICATOR;
        sci_msg(sci, SCI_SETINDICATORCURRENT, ind, 0);
        sci_msg(sci, SCI_INDICATORFILLRANGE, col_s, col_e - col_s);
    }
}

/* Called on GTK main thread via g_idle_add */
typedef struct { char *filepath; GPtrArray *spans; int errors; int warnings; } DiagIdleData;

static gboolean diag_idle_cb(gpointer ud)
{
    DiagIdleData *d = ud;

    g_mutex_lock(&s_diag_mutex);
    DiagCache *cache = g_hash_table_lookup(s_diag_cache, d->filepath);
    if (!cache) {
        cache = g_new0(DiagCache, 1);
        cache->filepath = g_strdup(d->filepath);
        g_hash_table_insert(s_diag_cache, cache->filepath, cache);
    }
    if (cache->spans) g_ptr_array_unref(cache->spans);
    cache->spans         = d->spans ? g_ptr_array_ref(d->spans) : NULL;
    cache->error_count   = d->errors;
    cache->warning_count = d->warnings;
    g_mutex_unlock(&s_diag_mutex);

    paint_diagnostics(d->filepath, d->spans);
    if (d->spans) g_ptr_array_unref(d->spans);
    g_free(d->filepath);
    g_free(d);
    return G_SOURCE_REMOVE;
}

/* ════════════════════════════════════════════════════════════════════════
 * Completion idle callback
 * ════════════════════════════════════════════════════════════════════════ */

typedef struct {
    LspServer      *srv;
    LspCompletionCb cb;
    void           *ud;
    char          **items;
    int             count;
} CompIdleData;

static gboolean comp_idle_cb(gpointer ud)
{
    CompIdleData *d = ud;
    if (d->cb)
        d->cb((const char **)d->items, d->count, d->ud);
    for (int i = 0; i < d->count; i++) g_free(d->items[i]);
    g_free(d->items);
    g_free(d);
    return G_SOURCE_REMOVE;
}

/* ════════════════════════════════════════════════════════════════════════
 * Definition / hover idle callbacks
 * ════════════════════════════════════════════════════════════════════════ */

typedef struct { char *filepath; int line; int col; } GotoIdleData;

static gboolean goto_idle_cb(gpointer ud)
{
    GotoIdleData *d = ud;
    editor_open_and_goto(d->filepath, d->line);
    /* TODO: also set column */
    g_free(d->filepath);
    g_free(d);
    return G_SOURCE_REMOVE;
}

typedef struct { char *text; GtkWidget *sci; } HoverIdleData;

static gboolean hover_idle_cb(gpointer ud)
{
    HoverIdleData *d = ud;
    /* Show as a transient tooltip-style popover anchored to the caret */
    if (d->sci && GTK_IS_WIDGET(d->sci) && d->text && *d->text) {
        GtkWidget *pop = gtk_popover_new(d->sci);
        GtkWidget *lbl = gtk_label_new(d->text);
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 80);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_end(lbl, 8);
        gtk_widget_set_margin_top(lbl, 6);
        gtk_widget_set_margin_bottom(lbl, 6);
        gtk_container_add(GTK_CONTAINER(pop), lbl);
        gtk_widget_show_all(pop);
        gtk_popover_popup(GTK_POPOVER(pop));
    }
    g_free(d->text);
    g_free(d);
    return G_SOURCE_REMOVE;
}

/* ════════════════════════════════════════════════════════════════════════
 * Reader thread — parses incoming LSP messages
 * ════════════════════════════════════════════════════════════════════════ */

static gboolean read_exactly(GInputStream *in, char *buf, gsize n)
{
    gsize done = 0;
    while (done < n) {
        gsize got = 0;
        if (!g_input_stream_read_all(in, buf + done, n - done, &got, NULL, NULL) || got == 0)
            return FALSE;
        done += got;
    }
    return TRUE;
}

/* Read one LSP header line (up to \r\n), return line without the CRLF.
 * Returns NULL on EOF/error. */
static char *read_header_line(GInputStream *in)
{
    GString *line = g_string_new(NULL);
    char c;
    while (TRUE) {
        gsize got = 0;
        if (!g_input_stream_read_all(in, &c, 1, &got, NULL, NULL) || got == 0) {
            g_string_free(line, TRUE);
            return NULL;
        }
        if (c == '\r') {
            /* expect \n */
            g_input_stream_read_all(in, &c, 1, &got, NULL, NULL);
            break;
        }
        g_string_append_c(line, c);
    }
    return g_string_free(line, FALSE);
}

static void handle_message(LspServer *srv, const char *body);

static gpointer reader_thread(gpointer data)
{
    LspServer    *srv = data;
    GInputStream *in  = srv->stdout_pipe;

    while (TRUE) {
        /* Read headers until blank line */
        gsize content_length = 0;
        while (TRUE) {
            char *line = read_header_line(in);
            if (!line) goto done;
            if (line[0] == '\0') { g_free(line); break; }
            if (strncmp(line, "Content-Length:", 15) == 0)
                content_length = (gsize)atol(line + 15);
            g_free(line);
        }
        if (content_length == 0) continue;

        char *body = g_malloc(content_length + 1);
        if (!read_exactly(in, body, content_length)) { g_free(body); goto done; }
        body[content_length] = '\0';

        handle_message(srv, body);
        g_free(body);
    }
done:
    return NULL;
}

/* ════════════════════════════════════════════════════════════════════════
 * Message dispatch
 * ════════════════════════════════════════════════════════════════════════ */

static void handle_publish_diagnostics(LspServer *srv G_GNUC_UNUSED, JsonVal *params)
{
    JsonVal *uri_v  = json_get(params, "uri");
    JsonVal *diags  = json_get(params, "diagnostics");
    if (!uri_v || !diags) return;

    const char *uri = json_str(uri_v);
    if (!uri) return;
    /* Convert file:///path → /path */
    const char *filepath = uri;
    if (strncmp(uri, "file://", 7) == 0) filepath = uri + 7;

    GPtrArray *spans = g_ptr_array_new_with_free_func(g_free);
    int errors = 0, warnings = 0;

    int n = json_arr_len(diags);
    for (int i = 0; i < n; i++) {
        JsonVal *d    = json_idx(diags, i);
        JsonVal *range = json_get(d, "range");
        if (!range) continue;
        JsonVal *start = json_get(range, "start");
        JsonVal *end   = json_get(range, "end");
        if (!start || !end) continue;

        int sev = json_int(json_get(d, "severity"));
        if (sev == 0) sev = 1; /* default to error */

        DiagSpan *sp   = g_new0(DiagSpan, 1);
        sp->line       = json_int(json_get(start, "line"));
        sp->col_start  = json_int(json_get(start, "character"));
        sp->col_end    = json_int(json_get(end,   "character"));
        sp->severity   = sev;
        g_ptr_array_add(spans, sp);

        if (sev == 1) errors++;
        else if (sev == 2) warnings++;
    }

    DiagIdleData *idle = g_new0(DiagIdleData, 1);
    idle->filepath = g_strdup(filepath);
    idle->spans    = spans;
    idle->errors   = errors;
    idle->warnings = warnings;
    g_idle_add(diag_idle_cb, idle);
}

static void flush_queued(LspServer *srv)
{
    for (guint i = 0; i < srv->queued_msgs->len; i++) {
        char *msg = g_ptr_array_index(srv->queued_msgs, i);
        srv_send(srv, msg);
        g_free(msg);
    }
    g_ptr_array_set_size(srv->queued_msgs, 0);
}

static void handle_initialize_response(LspServer *srv)
{
    /* Send initialized notification */
    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_end_obj(&b);
    srv_send(srv, "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}");
    jb_free(&b);

    srv->state = SRV_READY;
    flush_queued(srv);
}

static void handle_completion_response(LspServer *srv, JsonVal *result)
{
    if (!srv->pending.comp_cb) return;

    /* result is either CompletionList or CompletionItem[] */
    JsonVal *items_v = json_get(result, "items");
    if (!items_v) items_v = result; /* array form */

    int n = json_arr_len(items_v);
    char **items = n > 0 ? g_new0(char *, n) : NULL;
    int count = 0;
    for (int i = 0; i < n && count < n; i++) {
        JsonVal *item  = json_idx(items_v, i);
        JsonVal *label = json_get(item, "label");
        const char *s  = json_str(label);
        if (s) items[count++] = g_strdup(s);
    }

    CompIdleData *idle = g_new0(CompIdleData, 1);
    idle->srv   = srv;
    idle->cb    = srv->pending.comp_cb;
    idle->ud    = srv->pending.comp_userdata;
    idle->items = items;
    idle->count = count;
    srv->pending.comp_cb = NULL;
    g_idle_add(comp_idle_cb, idle);
}

static void handle_definition_response(LspServer *srv G_GNUC_UNUSED, JsonVal *result)
{
    /* result: Location | Location[] | LocationLink[] */
    JsonVal *loc = (result && result->type == JSON_ARRAY) ? json_idx(result, 0) : result;
    if (!loc) return;

    JsonVal *uri_v  = json_get(loc, "uri");
    JsonVal *range  = json_get(loc, "range");
    if (!uri_v || !range) return;
    JsonVal *start  = json_get(range, "start");
    if (!start) return;

    const char *uri = json_str(uri_v);
    if (!uri) return;
    const char *fp = (strncmp(uri, "file://", 7) == 0) ? uri + 7 : uri;

    GotoIdleData *idle = g_new0(GotoIdleData, 1);
    idle->filepath = g_strdup(fp);
    idle->line     = json_int(json_get(start, "line"));
    idle->col      = json_int(json_get(start, "character"));
    g_idle_add(goto_idle_cb, idle);
}

static void handle_hover_response(LspServer *srv, JsonVal *result)
{
    if (!srv->pending.hover_sci) return;
    GtkWidget *sci = srv->pending.hover_sci;
    srv->pending.hover_sci = NULL;

    JsonVal *contents = json_get(result, "contents");
    if (!contents) return;

    char *text = NULL;
    if (contents->type == JSON_STRING) {
        text = g_strdup(contents->s);
    } else if (contents->type == JSON_OBJECT) {
        JsonVal *val = json_get(contents, "value");
        if (val) text = g_strdup(json_str(val));
    } else if (contents->type == JSON_ARRAY) {
        /* Take first element */
        JsonVal *first = json_idx(contents, 0);
        if (first) {
            if (first->type == JSON_STRING) text = g_strdup(first->s);
            else { JsonVal *v = json_get(first, "value"); if (v) text = g_strdup(json_str(v)); }
        }
    }
    if (!text || !*text) { g_free(text); return; }

    HoverIdleData *idle = g_new0(HoverIdleData, 1);
    idle->text = text;
    idle->sci  = sci;
    g_idle_add(hover_idle_cb, idle);
}

static void handle_message(LspServer *srv, const char *body)
{
    JsonVal *root = json_parse(body);
    if (!root) return;

    JsonVal *id_v     = json_get(root, "id");
    JsonVal *method_v = json_get(root, "method");
    JsonVal *result_v = json_get(root, "result");
    JsonVal *params_v = json_get(root, "params");

    if (method_v) {
        /* Notification or server-initiated request */
        const char *method = json_str(method_v);
        if (method && strcmp(method, "textDocument/publishDiagnostics") == 0)
            handle_publish_diagnostics(srv, params_v);
    } else if (id_v && result_v) {
        /* Response to one of our requests */
        int id = json_int(id_v);
        if (srv->state == SRV_STARTING) {
            handle_initialize_response(srv);
        } else if (id == srv->pending.comp_id) {
            handle_completion_response(srv, result_v);
        } else if (id == srv->pending.def_id) {
            handle_definition_response(srv, result_v);
            srv->pending.def_id = 0;
        } else if (id == srv->pending.hover_id) {
            handle_hover_response(srv, result_v);
            srv->pending.hover_id = 0;
        }
    }

    json_free(root);
}

/* ════════════════════════════════════════════════════════════════════════
 * Server lifecycle
 * ════════════════════════════════════════════════════════════════════════ */

static void setup_indicators(GtkWidget *sci)
{
    /* Error: red squiggle */
    sci_msg(sci, SCI_INDICSETSTYLE,  DIAG_ERROR_INDICATOR, INDIC_SQUIGGLE);
    sci_msg(sci, SCI_INDICSETFORE,   DIAG_ERROR_INDICATOR, 0x0000cc); /* BGR red */
    sci_msg(sci, SCI_INDICSETALPHA,  DIAG_ERROR_INDICATOR, 200);
    /* Warning: orange squiggle */
    sci_msg(sci, SCI_INDICSETSTYLE,  DIAG_WARNING_INDICATOR, INDIC_SQUIGGLE);
    sci_msg(sci, SCI_INDICSETFORE,   DIAG_WARNING_INDICATOR, 0x0088ff); /* BGR orange */
    sci_msg(sci, SCI_INDICSETALPHA,  DIAG_WARNING_INDICATOR, 180);
}

static LspServer *find_server_for_lang(const char *lang_id)
{
    for (int i = 0; i < s_srv_count; i++) {
        if (strcmp(s_servers[i]->cfg.lang, lang_id) == 0)
            return s_servers[i];
    }
    return NULL;
}

static ServerCfg *find_cfg_for_lang(const char *lang_id)
{
    for (int i = 0; i < s_cfg_count; i++) {
        if (strcmp(s_cfg[i].lang, lang_id) == 0)
            return &s_cfg[i];
    }
    return NULL;
}

static LspServer *start_server(ServerCfg *cfg)
{
    /* Build argv from cmd + args */
    gchar  *argv_str = g_strdup_printf("%s %s", cfg->cmd, cfg->args);
    gchar **argv     = NULL;
    GError *err      = NULL;
    if (!g_shell_parse_argv(argv_str, NULL, &argv, &err)) {
        g_warning("LSP: failed to parse command '%s': %s", argv_str, err->message);
        g_error_free(err);
        g_free(argv_str);
        return NULL;
    }
    g_free(argv_str);

    GSubprocessLauncher *lnch = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    GSubprocess *proc = g_subprocess_launcher_spawnv(
        lnch, (const gchar * const *)argv, &err);
    g_strfreev(argv);
    g_object_unref(lnch);

    if (!proc) {
        g_message("LSP: could not start '%s': %s", cfg->cmd, err->message);
        g_error_free(err);
        return NULL;
    }

    LspServer *srv        = g_new0(LspServer, 1);
    srv->cfg              = *cfg;
    srv->state            = SRV_STARTING;
    srv->proc             = proc;
    srv->stdin_pipe       = g_subprocess_get_stdin_pipe(proc);
    srv->stdout_pipe      = g_subprocess_get_stdout_pipe(proc);
    srv->open_docs        = g_ptr_array_new_with_free_func(g_free);
    srv->queued_msgs      = g_ptr_array_new_with_free_func(g_free);
    g_mutex_init(&srv->write_mutex);

    /* Start reader thread */
    srv->reader = g_thread_new("lsp-reader", reader_thread, srv);

    /* Send initialize */
    char *init_params = build_initialize_params();
    srv->next_id = 0;
    srv_request(srv, "initialize", init_params);
    g_free(init_params);

    if (s_srv_count < MAX_SERVERS)
        s_servers[s_srv_count++] = srv;

    return srv;
}

static LspServer *get_or_start_server(const char *lang_id)
{
    LspServer *srv = find_server_for_lang(lang_id);
    if (srv) return srv;
    ServerCfg *cfg = find_cfg_for_lang(lang_id);
    if (!cfg) return NULL;
    return start_server(cfg);
}

/* ════════════════════════════════════════════════════════════════════════
 * Open document tracking
 * ════════════════════════════════════════════════════════════════════════ */

static OpenDoc *find_open_doc(LspServer *srv, const char *filepath)
{
    for (guint i = 0; i < srv->open_docs->len; i++) {
        OpenDoc *d = g_ptr_array_index(srv->open_docs, i);
        if (strcmp(d->filepath, filepath) == 0) return d;
    }
    return NULL;
}

/* ════════════════════════════════════════════════════════════════════════
 * Public API
 * ════════════════════════════════════════════════════════════════════════ */

void lsp_init(GtkWidget *window)
{
    s_window    = window;
    s_cfg_count = 0;
    s_srv_count = 0;
    g_mutex_init(&s_diag_mutex);
    s_diag_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          NULL, g_free);
    load_config();
}

void lsp_shutdown(void)
{
    for (int i = 0; i < s_srv_count; i++) {
        LspServer *srv = s_servers[i];
        if (srv->state == SRV_READY)
            srv_send(srv, "{\"jsonrpc\":\"2.0\",\"id\":99999,\"method\":\"shutdown\",\"params\":null}");
        if (srv->stdin_pipe)
            g_output_stream_close(srv->stdin_pipe, NULL, NULL);
        if (srv->reader)
            g_thread_join(srv->reader);
        g_subprocess_force_exit(srv->proc);
        g_object_unref(srv->proc);
        g_ptr_array_unref(srv->open_docs);
        g_ptr_array_unref(srv->queued_msgs);
        g_mutex_clear(&srv->write_mutex);
        g_free(srv);
        s_servers[i] = NULL;
    }
    s_srv_count = 0;
    if (s_diag_cache) { g_hash_table_unref(s_diag_cache); s_diag_cache = NULL; }
}

void lsp_doc_open(GtkWidget *sci, const char *filepath, const char *lang_id)
{
    if (!filepath || !lang_id) return;

    /* Set up indicators on this sci */
    setup_indicators(sci);

    LspServer *srv = get_or_start_server(lang_id);
    if (!srv) return;

    if (find_open_doc(srv, filepath)) return; /* already open */

    /* Get document text */
    glong text_len = sci_msg(sci, SCI_GETLENGTH, 0, 0);
    char *text     = g_malloc(text_len + 1);
    sci_msg(sci, SCI_GETTEXT, text_len + 1, (sptr_t)text);

    OpenDoc *d    = g_new0(OpenDoc, 1);
    d->filepath   = g_strdup(filepath);
    d->version    = 1;
    g_ptr_array_add(srv->open_docs, d);

    char *item = build_text_document_item(filepath, lang_id, d->version, text);
    g_free(text);

    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_key(&b, "textDocument");
    g_string_append(b.s, item);
    b.need_comma = TRUE;
    jb_end_obj(&b);
    g_free(item);

    srv_notify(srv, "textDocument/didOpen", jb_get(&b));
    jb_free(&b);

    /* Replay cached diagnostics if any */
    g_mutex_lock(&s_diag_mutex);
    DiagCache *cache = g_hash_table_lookup(s_diag_cache, filepath);
    GPtrArray *spans = cache ? (cache->spans ? g_ptr_array_ref(cache->spans) : NULL) : NULL;
    g_mutex_unlock(&s_diag_mutex);
    if (spans) { paint_diagnostics(filepath, spans); g_ptr_array_unref(spans); }
}

void lsp_doc_change(GtkWidget *sci, const char *filepath)
{
    if (!filepath) return;
    /* Find which server tracks this file */
    LspServer *srv = NULL;
    OpenDoc   *doc = NULL;
    for (int i = 0; i < s_srv_count && !srv; i++) {
        doc = find_open_doc(s_servers[i], filepath);
        if (doc) srv = s_servers[i];
    }
    if (!srv || !doc) return;

    doc->version++;
    glong text_len = sci_msg(sci, SCI_GETLENGTH, 0, 0);
    char *text     = g_malloc(text_len + 1);
    sci_msg(sci, SCI_GETTEXT, text_len + 1, (sptr_t)text);

    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_key(&b, "textDocument");
    jb_begin_obj(&b);
    char uri[2048]; snprintf(uri, sizeof(uri), "file://%s", filepath);
    jb_kstr(&b, "uri", uri);
    jb_kint(&b, "version", doc->version);
    jb_end_obj(&b);
    jb_key(&b, "contentChanges");
    jb_begin_arr(&b);
    jb_begin_obj(&b);
    jb_kstr(&b, "text", text);
    jb_end_obj(&b);
    jb_end_arr(&b);
    jb_end_obj(&b);
    g_free(text);

    srv_notify(srv, "textDocument/didChange", jb_get(&b));
    jb_free(&b);
}

void lsp_doc_save(const char *filepath)
{
    if (!filepath) return;
    for (int i = 0; i < s_srv_count; i++) {
        if (find_open_doc(s_servers[i], filepath)) {
            JsonBuf b; jb_init(&b);
            jb_begin_obj(&b);
            jb_key(&b, "textDocument");
            jb_begin_obj(&b);
            char uri[2048]; snprintf(uri, sizeof(uri), "file://%s", filepath);
            jb_kstr(&b, "uri", uri);
            jb_end_obj(&b);
            jb_end_obj(&b);
            srv_notify(s_servers[i], "textDocument/didSave", jb_get(&b));
            jb_free(&b);
            break;
        }
    }
}

void lsp_doc_close(const char *filepath)
{
    if (!filepath) return;
    for (int i = 0; i < s_srv_count; i++) {
        LspServer *srv = s_servers[i];
        OpenDoc   *doc = find_open_doc(srv, filepath);
        if (!doc) continue;

        JsonBuf b; jb_init(&b);
        jb_begin_obj(&b);
        jb_key(&b, "textDocument");
        jb_begin_obj(&b);
        char uri[2048]; snprintf(uri, sizeof(uri), "file://%s", filepath);
        jb_kstr(&b, "uri", uri);
        jb_end_obj(&b);
        jb_end_obj(&b);
        srv_notify(srv, "textDocument/didClose", jb_get(&b));
        jb_free(&b);

        /* Remove from open_docs */
        g_ptr_array_remove(srv->open_docs, doc);
        break;
    }
    /* Clear diagnostic cache */
    g_mutex_lock(&s_diag_mutex);
    g_hash_table_remove(s_diag_cache, filepath);
    g_mutex_unlock(&s_diag_mutex);
}

gboolean lsp_available_for(const char *lang_id)
{
    if (!lang_id) return FALSE;
    return find_cfg_for_lang(lang_id) != NULL;
}

void lsp_goto_definition(GtkWidget *sci, const char *filepath)
{
    if (!filepath) return;
    LspServer *srv = NULL;
    for (int i = 0; i < s_srv_count; i++) {
        if (find_open_doc(s_servers[i], filepath)) { srv = s_servers[i]; break; }
    }
    if (!srv || srv->state != SRV_READY) return;

    glong pos  = sci_msg(sci, SCI_GETCURRENTPOS, 0, 0);
    int   line = (int)sci_msg(sci, SCI_LINEFROMPOSITION, pos, 0);
    glong ls   = sci_msg(sci, SCI_POSITIONFROMLINE, line, 0);
    int   col  = (int)(pos - ls);

    char *params = build_position_params(filepath, line, col);
    int id = srv_request(srv, "textDocument/definition", params);
    g_free(params);
    srv->pending.def_id  = id;
    srv->pending.def_sci = sci;
}

void lsp_hover(GtkWidget *sci, const char *filepath)
{
    if (!filepath) return;
    LspServer *srv = NULL;
    for (int i = 0; i < s_srv_count; i++) {
        if (find_open_doc(s_servers[i], filepath)) { srv = s_servers[i]; break; }
    }
    if (!srv || srv->state != SRV_READY) return;

    glong pos  = sci_msg(sci, SCI_GETCURRENTPOS, 0, 0);
    int   line = (int)sci_msg(sci, SCI_LINEFROMPOSITION, pos, 0);
    glong ls   = sci_msg(sci, SCI_POSITIONFROMLINE, line, 0);
    int   col  = (int)(pos - ls);

    char *params = build_position_params(filepath, line, col);
    int id = srv_request(srv, "textDocument/hover", params);
    g_free(params);
    srv->pending.hover_id  = id;
    srv->pending.hover_sci = sci;
}

void lsp_rename(GtkWidget *sci, const char *filepath)
{
    if (!filepath) return;
    LspServer *srv = NULL;
    for (int i = 0; i < s_srv_count; i++) {
        if (find_open_doc(s_servers[i], filepath)) { srv = s_servers[i]; break; }
    }
    if (!srv || srv->state != SRV_READY) return;

    /* Prompt for new name */
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        "Rename Symbol", GTK_WINDOW(s_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Rename", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))),
                       entry, FALSE, FALSE, 8);
    gtk_widget_show_all(dlg);

    glong pos  = sci_msg(sci, SCI_GETCURRENTPOS, 0, 0);
    int   line = (int)sci_msg(sci, SCI_LINEFROMPOSITION, pos, 0);
    glong ls   = sci_msg(sci, SCI_POSITIONFROMLINE, line, 0);
    int   col  = (int)(pos - ls);

    g_signal_connect(dlg, "response", G_CALLBACK(gtk_widget_hide), NULL);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (new_name && *new_name) {
            JsonBuf b; jb_init(&b);
            jb_begin_obj(&b);
            jb_key(&b, "textDocument");
            jb_begin_obj(&b);
            char uri[2048]; snprintf(uri, sizeof(uri), "file://%s", filepath);
            jb_kstr(&b, "uri", uri);
            jb_end_obj(&b);
            jb_key(&b, "position");
            jb_begin_obj(&b);
            jb_kint(&b, "line", line);
            jb_kint(&b, "character", col);
            jb_end_obj(&b);
            jb_kstr(&b, "newName", new_name);
            jb_end_obj(&b);
            srv_request(srv, "textDocument/rename", jb_get(&b));
            jb_free(&b);
        }
    }
    gtk_widget_destroy(dlg);
}

void lsp_request_completion(GtkWidget *sci, const char *filepath,
                             int line, int col,
                             LspCompletionCb cb, void *userdata)
{
    if (!filepath) return;
    LspServer *srv = NULL;
    for (int i = 0; i < s_srv_count; i++) {
        if (find_open_doc(s_servers[i], filepath)) { srv = s_servers[i]; break; }
    }
    if (!srv || srv->state != SRV_READY) return;

    JsonBuf b; jb_init(&b);
    jb_begin_obj(&b);
    jb_key(&b, "textDocument");
    jb_begin_obj(&b);
    char uri[2048]; snprintf(uri, sizeof(uri), "file://%s", filepath);
    jb_kstr(&b, "uri", uri);
    jb_end_obj(&b);
    jb_key(&b, "position");
    jb_begin_obj(&b);
    jb_kint(&b, "line", line);
    jb_kint(&b, "character", col);
    jb_end_obj(&b);
    jb_end_obj(&b);

    int id = srv_request(srv, "textDocument/completion", jb_get(&b));
    jb_free(&b);
    (void)sci;

    srv->pending.comp_id       = id;
    srv->pending.comp_cb       = cb;
    srv->pending.comp_userdata = userdata;
}

int lsp_diagnostic_count(const char *filepath)
{
    if (!filepath) return -1;
    g_mutex_lock(&s_diag_mutex);
    DiagCache *cache = g_hash_table_lookup(s_diag_cache, filepath);
    int total = cache ? cache->error_count + cache->warning_count : -1;
    g_mutex_unlock(&s_diag_mutex);
    return total;
}
