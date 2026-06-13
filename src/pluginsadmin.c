#include "pluginsadmin.h"
#include "plugin.h"
#include <string.h>
#include <stdlib.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#define CATALOGUE_URL \
    "https://raw.githubusercontent.com/notetux-plus-plus/nppPluginList/main/v1/notetux_plugin_list.json"

/* sha256 value used as a placeholder in the JSON for unreleased plugins */
#define SHA256_ZEROS \
    "0000000000000000000000000000000000000000000000000000000000000000"

#define MAX_CATALOGUE_BYTES (1u * 1024u * 1024u)   /*  1 MB — catalogue is JSON text  */
#define MAX_PLUGIN_BYTES   (50u * 1024u * 1024u)   /* 50 MB — generous cap for .so    */

/* ------------------------------------------------------------------ */
/* Data                                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    char *name;          /* identifier matching .so directory */
    char *display_name;
    char *version;
    char *author;
    char *description;
    char *homepage;
    char *download_url;
    char *sha256;
} CatalogEntry;

/* Persistent dialog state — all NULL until first pluginsadmin_show() */
static GPtrArray  *s_catalog     = NULL;
static GHashTable *s_installed   = NULL; /* name → TRUE (gboolean boxed) */
static GtkWidget  *s_dialog      = NULL;
static GtkListStore *s_store     = NULL;
static GtkWidget  *s_spinner     = NULL;
static GtkWidget  *s_status_lbl  = NULL;
static GtkWidget  *s_install_btn = NULL;
static GtkWidget  *s_uninst_btn  = NULL;
static GtkWidget  *s_refresh_btn = NULL;
static GtkTreeView *s_treeview   = NULL;

/* ------------------------------------------------------------------ */
/* Installed plugins scan                                              */
/* ------------------------------------------------------------------ */

static char *plugins_dir(void)
{
    return g_build_filename(g_get_home_dir(), ".config", "notetux", "plugins", NULL);
}

/* Returns GHashTable mapping plugin name string → non-null sentinel */
static GHashTable *scan_installed(void)
{
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    char *dir = plugins_dir();
    GDir *d = g_dir_open(dir, 0, NULL);
    if (d) {
        const char *name;
        while ((name = g_dir_read_name(d))) {
            gchar *so = g_strdup_printf("%s/%s/%s.so", dir, name, name);
            if (g_file_test(so, G_FILE_TEST_EXISTS))
                g_hash_table_insert(ht, g_strdup(name), GINT_TO_POINTER(1));
            g_free(so);
        }
        g_dir_close(d);
    }
    g_free(dir);
    return ht;
}

/* ------------------------------------------------------------------ */
/* CatalogEntry                                                        */
/* ------------------------------------------------------------------ */

static void entry_free(gpointer p)
{
    CatalogEntry *e = p;
    g_free(e->name);        g_free(e->display_name); g_free(e->version);
    g_free(e->author);      g_free(e->description);  g_free(e->homepage);
    g_free(e->download_url); g_free(e->sha256);
    g_free(e);
}

/* ------------------------------------------------------------------ */
/* JSON parsing (json-glib)                                            */
/* ------------------------------------------------------------------ */

static const char *jstr(JsonObject *obj, const char *key)
{
    if (!json_object_has_member(obj, key)) return "";
    JsonNode *n = json_object_get_member(obj, key);
    const char *s = JSON_NODE_HOLDS_VALUE(n) ? json_node_get_string(n) : NULL;
    return s ? s : "";
}

static GPtrArray *parse_catalogue(const char *text, gsize len)
{
    GError *err = NULL;
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, text, (gssize)len, &err)) {
        g_warning("pluginsadmin: JSON parse error: %s", err->message);
        g_error_free(err); g_object_unref(parser);
        return NULL;
    }
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root)) { g_object_unref(parser); return NULL; }

    JsonArray *ja = json_node_get_array(root);
    guint n = json_array_get_length(ja);
    GPtrArray *arr = g_ptr_array_new_with_free_func(entry_free);

    for (guint i = 0; i < n; i++) {
        JsonNode *node = json_array_get_element(ja, i);
        if (!JSON_NODE_HOLDS_OBJECT(node)) continue;
        JsonObject *obj = json_node_get_object(node);

        CatalogEntry *e = g_new0(CatalogEntry, 1);
        e->name         = g_strdup(jstr(obj, "name"));
        e->display_name = g_strdup(*jstr(obj, "displayName")
                                       ? jstr(obj, "displayName") : e->name);
        e->version      = g_strdup(jstr(obj, "version"));
        e->author       = g_strdup(jstr(obj, "author"));
        e->description  = g_strdup(jstr(obj, "description"));
        e->homepage     = g_strdup(jstr(obj, "homepage"));

        if (json_object_has_member(obj, "repository")) {
            JsonObject *repo = json_object_get_object_member(obj, "repository");
            e->download_url = g_strdup(jstr(repo, "download"));
            e->sha256       = g_strdup(jstr(repo, "sha256"));
        } else {
            e->download_url = g_strdup("");
            e->sha256       = g_strdup("");
        }

        if (*e->name)
            g_ptr_array_add(arr, e);
        else
            entry_free(e);
    }
    g_object_unref(parser);
    return arr;
}

/* ------------------------------------------------------------------ */
/* List store                                                          */
/* ------------------------------------------------------------------ */

enum { COL_IDX=0, COL_NAME, COL_VER, COL_AUTHOR, COL_DESC, COL_STATUS, COL_BG, N_COLS };

#define INSTALLED_BG "#d4edda"  /* light green for installed rows */

static const char *entry_status(const CatalogEntry *e)
{
    if (strcmp(e->sha256, SHA256_ZEROS) == 0 || !*e->download_url)
        return "Coming soon";
    if (s_installed && g_hash_table_contains(s_installed, e->name))
        return "Installed";
    return "Available";
}

static void populate_store(void)
{
    gtk_list_store_clear(s_store);
    if (!s_catalog) return;
    for (guint i = 0; i < s_catalog->len; i++) {
        CatalogEntry *e = s_catalog->pdata[i];
        const char *status = entry_status(e);
        GtkTreeIter it;
        gtk_list_store_append(s_store, &it);
        gtk_list_store_set(s_store, &it,
            COL_IDX,    (int)i,
            COL_NAME,   e->display_name,
            COL_VER,    e->version,
            COL_AUTHOR, e->author,
            COL_DESC,   e->description,
            COL_STATUS, status,
            COL_BG,     strcmp(status, "Installed") == 0 ? INSTALLED_BG : NULL,
            -1);
    }
}

/* ------------------------------------------------------------------ */
/* Selection → button sensitivity                                      */
/* ------------------------------------------------------------------ */

static void sync_buttons(void)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(s_treeview);
    GtkTreeIter it;
    GtkTreeModel *m;
    if (!gtk_tree_selection_get_selected(sel, &m, &it)) {
        gtk_widget_set_sensitive(s_install_btn, FALSE);
        gtk_widget_set_sensitive(s_uninst_btn,  FALSE);
        return;
    }
    char *status;
    gtk_tree_model_get(m, &it, COL_STATUS, &status, -1);
    gtk_widget_set_sensitive(s_install_btn, strcmp(status, "Available") == 0);
    gtk_widget_set_sensitive(s_uninst_btn,  strcmp(status, "Installed") == 0);
    g_free(status);
}

static void on_selection_changed(GtkTreeSelection *sel, gpointer ud)
{
    (void)sel; (void)ud;
    sync_buttons();
}

/* ------------------------------------------------------------------ */
/* Catalogue fetch — runs in a GThread                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    GPtrArray *catalog;
    char      *error_msg;
} FetchResult;

static gboolean on_fetch_done(gpointer data)
{
    FetchResult *fr = data;

    /* Dialog may have been destroyed while we were fetching */
    if (!s_dialog) {
        if (fr->catalog) g_ptr_array_unref(fr->catalog);
        g_free(fr->error_msg);
        g_free(fr);
        return G_SOURCE_REMOVE;
    }

    gtk_spinner_stop(GTK_SPINNER(s_spinner));
    gtk_widget_hide(s_spinner);
    gtk_widget_set_sensitive(s_refresh_btn, TRUE);

    if (fr->catalog) {
        if (s_catalog)   g_ptr_array_unref(s_catalog);
        if (s_installed) g_hash_table_unref(s_installed);
        s_catalog   = fr->catalog;
        s_installed = scan_installed();
        populate_store();
        gtk_label_set_text(GTK_LABEL(s_status_lbl),
            s_catalog->len ? "" : "No plugins found in catalogue.");
    } else {
        gtk_label_set_text(GTK_LABEL(s_status_lbl),
            fr->error_msg ? fr->error_msg : "Failed to load catalogue.");
    }

    g_free(fr->error_msg);
    g_free(fr);
    return G_SOURCE_REMOVE;
}

static gpointer fetch_thread(gpointer data)
{
    (void)data;
    FetchResult *fr = g_new0(FetchResult, 1);

    SoupSession *sess = soup_session_new();
    SoupMessage *msg  = soup_message_new("GET", CATALOGUE_URL);
    GError *err = NULL;
    GBytes *body = soup_session_send_and_read(sess, msg, NULL, &err);
    guint status = soup_message_get_status(msg);
    g_object_unref(msg);
    g_object_unref(sess);

    if (err) {
        fr->error_msg = g_strdup(err->message);
        g_error_free(err);
        if (body) g_bytes_unref(body);
    } else if (status != SOUP_STATUS_OK) {
        fr->error_msg = g_strdup_printf("HTTP %u: could not load catalogue.", status);
        if (body) g_bytes_unref(body);
    } else {
        gsize len;
        const char *text = g_bytes_get_data(body, &len);
        if (len > MAX_CATALOGUE_BYTES) {
            fr->error_msg = g_strdup("Catalogue response too large (> 1 MB).");
            g_bytes_unref(body);
        } else {
            fr->catalog = parse_catalogue(text, len);
            g_bytes_unref(body);
            if (!fr->catalog)
                fr->error_msg = g_strdup("Failed to parse plugin catalogue.");
        }
    }

    g_idle_add(on_fetch_done, fr);
    return NULL;
}

static void start_fetch(void)
{
    if (!s_dialog) return;
    gtk_list_store_clear(s_store);
    gtk_label_set_text(GTK_LABEL(s_status_lbl), "Loading catalogue…");
    gtk_widget_show(s_spinner);
    gtk_spinner_start(GTK_SPINNER(s_spinner));
    gtk_widget_set_sensitive(s_refresh_btn, FALSE);
    gtk_widget_set_sensitive(s_install_btn, FALSE);
    gtk_widget_set_sensitive(s_uninst_btn,  FALSE);
    g_thread_new("pluginlist-fetch", fetch_thread, NULL);
}

static void on_refresh(GtkButton *b, gpointer ud) { (void)b; (void)ud; start_fetch(); }

/* ------------------------------------------------------------------ */
/* Install from catalogue — runs in a GThread                          */
/* ------------------------------------------------------------------ */

typedef struct {
    guint  entry_idx;
    char  *name;
    char  *download_url;
    char  *sha256;
    /* result fields, filled by thread */
    gboolean success;
    char    *error_msg;
} InstallJob;

static gboolean on_install_done(gpointer data)
{
    InstallJob *job = data;

    if (!s_dialog) goto cleanup;

    gtk_spinner_stop(GTK_SPINNER(s_spinner));
    gtk_widget_hide(s_spinner);
    gtk_widget_set_sensitive(s_refresh_btn, TRUE);
    gtk_widget_set_sensitive(s_install_btn, FALSE); /* re-enabled by sync_buttons */

    if (job->success) {
        gtk_label_set_text(GTK_LABEL(s_status_lbl), "");

        /* Update status in the store */
        GtkTreeModel *m = GTK_TREE_MODEL(s_store);
        GtkTreeIter it;
        if (gtk_tree_model_get_iter_first(m, &it)) {
            do {
                int idx;
                gtk_tree_model_get(m, &it, COL_IDX, &idx, -1);
                if ((guint)idx == job->entry_idx) {
                    gtk_list_store_set(s_store, &it,
                        COL_STATUS, "Installed",
                        COL_BG,     INSTALLED_BG,
                        -1);
                    break;
                }
            } while (gtk_tree_model_iter_next(m, &it));
        }
        if (s_installed)
            g_hash_table_insert(s_installed, g_strdup(job->name), GINT_TO_POINTER(1));

        GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(s_dialog),
            GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
            "Plugin installed. Restart Notetux++ to load it.");
        g_signal_connect(msg, "response", G_CALLBACK(gtk_widget_destroy), NULL);
        gtk_widget_show(msg);
    } else {
        gtk_label_set_text(GTK_LABEL(s_status_lbl),
            job->error_msg ? job->error_msg : "Install failed.");
    }
    sync_buttons();

cleanup:
    g_free(job->name);
    g_free(job->download_url);
    g_free(job->sha256);
    g_free(job->error_msg);
    g_free(job);
    return G_SOURCE_REMOVE;
}

static gpointer install_thread(gpointer data)
{
    InstallJob *job = data;

    /* Download */
    SoupSession *sess = soup_session_new();
    SoupMessage *msg  = soup_message_new("GET", job->download_url);
    GError *err = NULL;
    GBytes *body = soup_session_send_and_read(sess, msg, NULL, &err);
    guint status = soup_message_get_status(msg);
    g_object_unref(msg);
    g_object_unref(sess);

    if (err || status != SOUP_STATUS_OK) {
        job->error_msg = err
            ? g_strdup(err->message)
            : g_strdup_printf("HTTP %u: download failed.", status);
        if (err)  g_error_free(err);
        if (body) g_bytes_unref(body);
        job->success = FALSE;
        g_idle_add(on_install_done, job);
        return NULL;
    }

    gsize len;
    const guint8 *bytes = g_bytes_get_data(body, &len);

    if (len > MAX_PLUGIN_BYTES) {
        g_bytes_unref(body);
        job->error_msg = g_strdup("Plugin file exceeds 50 MB size limit.");
        job->success = FALSE;
        g_idle_add(on_install_done, job);
        return NULL;
    }

    /* Verify SHA256 (skip when the hash is all zeros — dev build) */
    if (strcmp(job->sha256, SHA256_ZEROS) != 0) {
        GChecksum *ck = g_checksum_new(G_CHECKSUM_SHA256);
        g_checksum_update(ck, bytes, len);
        gboolean ok = (strcmp(g_checksum_get_string(ck), job->sha256) == 0);
        g_checksum_free(ck);
        if (!ok) {
            g_bytes_unref(body);
            job->error_msg = g_strdup("SHA256 mismatch — download may be corrupt or tampered with.");
            job->success = FALSE;
            g_idle_add(on_install_done, job);
            return NULL;
        }
    }

    /* Write to ~/.config/notetux/plugins/<Name>/<Name>.so */
    char *so_name  = g_path_get_basename(job->download_url);
    char *dest_dir = g_build_filename(g_get_home_dir(), ".config", "notetux",
                                      "plugins", job->name, NULL);
    g_mkdir_with_parents(dest_dir, 0755);
    char *dest = g_build_filename(dest_dir, so_name, NULL);

    GError *werr = NULL;
    g_file_set_contents(dest, (const char *)bytes, (gssize)len, &werr);
    g_bytes_unref(body);
    g_free(so_name); g_free(dest); g_free(dest_dir);

    if (werr) {
        job->error_msg = g_strdup(werr->message);
        g_error_free(werr);
        job->success = FALSE;
    } else {
        job->success = TRUE;
    }
    g_idle_add(on_install_done, job);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Install from file (local .so)                                       */
/* ------------------------------------------------------------------ */

static void on_install_from_file(GtkButton *b, gpointer ud)
{
    (void)b; (void)ud;
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Select Plugin (.so)",
        GTK_WINDOW(s_dialog), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Install", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *ff = gtk_file_filter_new();
    gtk_file_filter_set_name(ff, "Shared Libraries (*.so)");
    gtk_file_filter_add_pattern(ff, "*.so");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), ff);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *src  = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        char *base = g_path_get_basename(src);
        char *name = g_strdup(base);
        char *dot  = strrchr(name, '.');
        if (dot) *dot = '\0';

        char *dest_dir = g_build_filename(g_get_home_dir(), ".config", "notetux",
                                          "plugins", name, NULL);
        g_mkdir_with_parents(dest_dir, 0755);
        char *dest = g_build_filename(dest_dir, base, NULL);

        GFile *src_f  = g_file_new_for_path(src);
        GFile *dest_f = g_file_new_for_path(dest);
        GError *err   = NULL;
        g_file_copy(src_f, dest_f, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);
        g_object_unref(src_f); g_object_unref(dest_f);

        GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(s_dialog),
            GTK_DIALOG_MODAL,
            err ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
            err ? "Install failed: %s" : "Plugin installed.\nRestart Notetux++ to load it.",
            err ? err->message : "");
        g_signal_connect(info, "response", G_CALLBACK(gtk_widget_destroy), NULL);
        gtk_widget_show(info);

        if (!err && s_installed) {
            g_hash_table_insert(s_installed, g_strdup(name), GINT_TO_POINTER(1));
            if (s_catalog) populate_store();
        }
        if (err) g_error_free(err);
        g_free(dest); g_free(dest_dir); g_free(name); g_free(base); g_free(src);
    }
    gtk_widget_destroy(dlg);
}

/* ------------------------------------------------------------------ */
/* Install from catalogue button handler                               */
/* ------------------------------------------------------------------ */

static void on_install_clicked(GtkButton *b, gpointer ud)
{
    (void)b; (void)ud;
    if (!s_catalog) return;

    GtkTreeSelection *sel = gtk_tree_view_get_selection(s_treeview);
    GtkTreeIter it;
    GtkTreeModel *m;
    if (!gtk_tree_selection_get_selected(sel, &m, &it)) return;

    int idx;
    gtk_tree_model_get(m, &it, COL_IDX, &idx, -1);
    if (idx < 0 || (guint)idx >= s_catalog->len) return;
    CatalogEntry *e = s_catalog->pdata[idx];
    if (!e->download_url || !*e->download_url) return;

    InstallJob *job = g_new0(InstallJob, 1);
    job->entry_idx    = (guint)idx;
    job->name         = g_strdup(e->name);
    job->download_url = g_strdup(e->download_url);
    job->sha256       = g_strdup(e->sha256);

    gtk_widget_set_sensitive(s_install_btn, FALSE);
    gtk_widget_set_sensitive(s_refresh_btn, FALSE);
    gtk_label_set_text(GTK_LABEL(s_status_lbl), "Downloading…");
    gtk_widget_show(s_spinner);
    gtk_spinner_start(GTK_SPINNER(s_spinner));

    g_thread_new("plugin-install", install_thread, job);
}

/* ------------------------------------------------------------------ */
/* Uninstall button handler                                            */
/* ------------------------------------------------------------------ */

static void on_uninstall_clicked(GtkButton *b, gpointer ud)
{
    (void)b; (void)ud;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(s_treeview);
    GtkTreeIter it;
    GtkTreeModel *m;
    if (!gtk_tree_selection_get_selected(sel, &m, &it)) return;

    int idx;
    gtk_tree_model_get(m, &it, COL_IDX, &idx, -1);
    if (!s_catalog || idx < 0 || (guint)idx >= s_catalog->len) return;
    CatalogEntry *e = s_catalog->pdata[idx];

    char *dir  = g_build_filename(g_get_home_dir(), ".config", "notetux",
                                  "plugins", e->name, NULL);
    char *so   = g_strdup_printf("%s/%s.so", dir, e->name);
    GFile *so_f  = g_file_new_for_path(so);
    GFile *dir_f = g_file_new_for_path(dir);
    GError *err  = NULL;
    g_file_delete(so_f, NULL, &err);
    if (!err) g_file_delete(dir_f, NULL, NULL); /* ignore empty-dir errors */
    g_object_unref(so_f); g_object_unref(dir_f);
    g_free(so); g_free(dir);

    if (err) {
        gtk_label_set_text(GTK_LABEL(s_status_lbl), err->message);
        g_error_free(err);
        return;
    }

    if (s_installed) g_hash_table_remove(s_installed, e->name);
    gtk_list_store_set(s_store, &it,
        COL_STATUS, "Available",
        COL_BG,     NULL,
        -1);
    sync_buttons();

    GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(s_dialog),
        GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "Plugin removed. Restart Notetux++ to apply.");
    g_signal_connect(info, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show(info);
}

/* ------------------------------------------------------------------ */
/* Dialog — persistent singleton                                        */
/* ------------------------------------------------------------------ */

static void on_dialog_destroy(GtkWidget *w, gpointer ud)
{
    /* Clear all widget refs so async callbacks can guard safely */
    (void)w; (void)ud;
    s_dialog = s_spinner = s_status_lbl = NULL;
    s_install_btn = s_uninst_btn = s_refresh_btn = NULL;
    s_store    = NULL;
    s_treeview = NULL;
}

static void build_dialog(GtkWindow *parent)
{
    s_dialog = gtk_dialog_new_with_buttons("Plugins Admin", parent,
        GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(s_dialog), 720, 460);

    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(s_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(box), 6);
    gtk_box_set_spacing(GTK_BOX(box), 4);

    /* ── tree ──────────────────────────────────────────────────── */
    s_store = gtk_list_store_new(N_COLS,
        G_TYPE_INT,    /* COL_IDX    */
        G_TYPE_STRING, /* COL_NAME   */
        G_TYPE_STRING, /* COL_VER    */
        G_TYPE_STRING, /* COL_AUTHOR */
        G_TYPE_STRING, /* COL_DESC   */
        G_TYPE_STRING, /* COL_STATUS */
        G_TYPE_STRING  /* COL_BG     */
    );

    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(s_store));
    g_object_unref(s_store);  /* view holds the ref */
    s_treeview = GTK_TREE_VIEW(tv);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv), TRUE);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(tv), FALSE);

    struct { const char *label; int col; gboolean expand; } cols[] = {
        { "Name",        COL_NAME,   FALSE },
        { "Version",     COL_VER,    FALSE },
        { "Author",      COL_AUTHOR, FALSE },
        { "Description", COL_DESC,   TRUE  },
        { "Status",      COL_STATUS, FALSE },
    };
    for (int i = 0; i < 5; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
            cols[i].label, r, "text", cols[i].col, NULL);
        gtk_tree_view_column_add_attribute(c, r, "cell-background", COL_BG);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_column_set_expand(c, cols[i].expand);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tv), c);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tv);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    /* ── button bar ────────────────────────────────────────────── */
    GtkWidget *btn_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(btn_bar), 2);

    s_install_btn       = gtk_button_new_with_label("Install");
    s_uninst_btn        = gtk_button_new_with_label("Uninstall");
    GtkWidget *file_btn = gtk_button_new_with_label("Install from file…");
    s_refresh_btn       = gtk_button_new_with_label("Refresh");
    s_spinner           = gtk_spinner_new();
    s_status_lbl        = gtk_label_new("");
    gtk_label_set_ellipsize(GTK_LABEL(s_status_lbl), PANGO_ELLIPSIZE_END);

    gtk_widget_set_sensitive(s_install_btn, FALSE);
    gtk_widget_set_sensitive(s_uninst_btn,  FALSE);

    gtk_box_pack_start(GTK_BOX(btn_bar), s_install_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_bar), s_uninst_btn,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_bar), file_btn,      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_bar), s_refresh_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_bar), s_spinner,     FALSE, FALSE, 4);
    gtk_box_pack_end  (GTK_BOX(btn_bar), s_status_lbl,  TRUE,  TRUE,  4);
    gtk_box_pack_start(GTK_BOX(box), btn_bar, FALSE, FALSE, 0);

    /* ── signals ───────────────────────────────────────────────── */
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
    g_signal_connect(sel,            "changed", G_CALLBACK(on_selection_changed), NULL);
    g_signal_connect(s_install_btn,  "clicked", G_CALLBACK(on_install_clicked),   NULL);
    g_signal_connect(s_uninst_btn,   "clicked", G_CALLBACK(on_uninstall_clicked), NULL);
    g_signal_connect(file_btn,       "clicked", G_CALLBACK(on_install_from_file), NULL);
    g_signal_connect(s_refresh_btn,  "clicked", G_CALLBACK(on_refresh),           NULL);

    /* Close hides rather than destroys so the singleton lives on */
    g_signal_connect(s_dialog, "response",     G_CALLBACK(gtk_widget_hide), NULL);
    g_signal_connect(s_dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    /* But if the parent window is destroyed, we do get destroyed */
    g_signal_connect(s_dialog, "destroy", G_CALLBACK(on_dialog_destroy), NULL);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void pluginsadmin_show(GtkWindow *parent)
{
    if (!s_dialog)
        build_dialog(parent);

    gtk_widget_show_all(s_dialog);
    gtk_widget_hide(s_spinner);   /* spinner only shows during loading */
    gtk_window_present(GTK_WINDOW(s_dialog));

    /* Always refresh the catalogue when the dialog is opened */
    start_fetch();
}
