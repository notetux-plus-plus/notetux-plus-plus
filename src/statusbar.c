#include "statusbar.h"
#include "encoding.h"
#include "sci_c.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Widgets and state                                                   */
/* ------------------------------------------------------------------ */

static GtkWidget *s_lbl_pos;
static GtkWidget *s_lbl_enc;
static GtkWidget *s_lbl_eol;
static GtkWidget *s_lbl_lang;
static GtkWidget *s_lbl_ovr;
static GtkWidget *s_lbl_indent;

static GtkWidget *s_sci    = NULL;
static StatusbarEncodingCb s_enc_cb  = NULL;
static StatusbarLanguageCb s_lang_cb = NULL;

void statusbar_set_encoding_cb(StatusbarEncodingCb cb) { s_enc_cb  = cb; }
void statusbar_set_language_cb(StatusbarLanguageCb cb) { s_lang_cb = cb; }

/* ------------------------------------------------------------------ */
/* Internal Scintilla helper                                           */
/* ------------------------------------------------------------------ */

static sptr_t smsg(unsigned int m, uptr_t w, sptr_t l)
{
    if (!s_sci) return 0;
    return scintilla_send_message(SCINTILLA(s_sci), m, w, l);
}

/* ------------------------------------------------------------------ */
/* Layout helpers                                                      */
/* ------------------------------------------------------------------ */

static GtkWidget *vsep(void)
{
    GtkWidget *s = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(s, 4);
    gtk_widget_set_margin_end(s, 4);
    return s;
}

static GtkWidget *rlabel(const char *text)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_widget_set_margin_start(l, 4);
    gtk_widget_set_margin_end(l, 4);
    return l;
}

static void on_eb_realize(GtkWidget *w, gpointer d)
{
    (void)d;
    GdkWindow *win = gtk_widget_get_window(w);
    if (!win) return;
    GdkCursor *cur = gdk_cursor_new_from_name(gdk_display_get_default(), "pointer");
    if (cur) { gdk_window_set_cursor(win, cur); g_object_unref(cur); }
}

/* Return the triggering GdkEvent from a multi-press gesture. */
static const GdkEvent *gesture_current_event(GtkGestureMultiPress *g)
{
    GdkEventSequence *seq =
        gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(g));
    return gtk_gesture_get_last_event(GTK_GESTURE(g), seq);
}

static GtkWidget *clickable_wrap(GtkWidget *label, GCallback handler)
{
    GtkWidget *eb = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eb), label);
    g_signal_connect(eb, "realize", G_CALLBACK(on_eb_realize), NULL);

    /* GtkGestureMultiPress works on both X11 and Wayland, unlike
     * the old button-press-event + GtkEventBox combination. */
    GtkGesture *gesture = gtk_gesture_multi_press_new(eb);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0); /* all buttons */
    g_signal_connect(gesture, "pressed", handler, NULL);
    /* Keep the gesture alive as long as the EventBox lives. */
    g_object_set_data_full(G_OBJECT(eb), "gesture", gesture, g_object_unref);

    return eb;
}

/* ------------------------------------------------------------------ */
/* EOL popup                                                           */
/* ------------------------------------------------------------------ */

static void on_eol_item(GtkMenuItem *item, gpointer data)
{
    (void)item;
    int mode = GPOINTER_TO_INT(data);
    smsg(SCI_SETEOLMODE, (uptr_t)mode, 0);
    smsg(SCI_CONVERTEOLS, (uptr_t)mode, 0);
    if (s_sci) statusbar_update_from_sci(s_sci);
}

static void on_eol_click(GtkGestureMultiPress *g, gint n, gdouble x, gdouble y, gpointer d)
{
    (void)n; (void)x; (void)y; (void)d;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g)) != 3) return;
    int cur = (int)smsg(SCI_GETEOLMODE, 0, 0);
    static const struct { const char *label; int mode; } items[] = {
        { "Windows (CRLF)", SC_EOL_CRLF },
        { "Unix (LF)",      SC_EOL_LF   },
        { "Old Mac (CR)",   SC_EOL_CR   },
    };
    GtkWidget *menu = gtk_menu_new();
    for (int i = 0; i < 3; i++) {
        GtkWidget *mi = gtk_check_menu_item_new_with_label(items[i].label);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), cur == items[i].mode);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(mi, "activate", G_CALLBACK(on_eol_item),
                         GINT_TO_POINTER(items[i].mode));
    }
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), gesture_current_event(g));
}

/* ------------------------------------------------------------------ */
/* INS / OVR popup                                                     */
/* ------------------------------------------------------------------ */

static void on_ovr_item(GtkMenuItem *item, gpointer data)
{
    (void)item;
    smsg(SCI_SETOVERTYPE, (uptr_t)GPOINTER_TO_INT(data), 0);
    if (s_sci) statusbar_update_from_sci(s_sci);
}

static void on_ovr_click(GtkGestureMultiPress *g, gint n, gdouble x, gdouble y, gpointer d)
{
    (void)n; (void)x; (void)y; (void)d;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g)) != 3) return;
    int cur = (int)smsg(SCI_GETOVERTYPE, 0, 0);
    static const struct { const char *label; int val; } items[] = {
        { "Insert (INS)",   0 },
        { "Overtype (OVR)", 1 },
    };
    GtkWidget *menu = gtk_menu_new();
    for (int i = 0; i < 2; i++) {
        GtkWidget *mi = gtk_check_menu_item_new_with_label(items[i].label);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), cur == items[i].val);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(mi, "activate", G_CALLBACK(on_ovr_item),
                         GINT_TO_POINTER(items[i].val));
    }
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), gesture_current_event(g));
}

/* ------------------------------------------------------------------ */
/* Indent popup                                                        */
/* ------------------------------------------------------------------ */

typedef struct { int use_tabs; int width; } IndentChoice;
static const IndentChoice kIndentChoices[] = {
    {0,2},{0,4},{0,8},   /* spaces */
    {1,2},{1,4},{1,8},   /* tabs   */
};

static void on_indent_item(GtkMenuItem *item, gpointer data)
{
    (void)item;
    const IndentChoice *c = (const IndentChoice *)data;
    smsg(SCI_SETUSETABS,  (uptr_t)c->use_tabs, 0);
    smsg(SCI_SETTABWIDTH, (uptr_t)c->width,     0);
    if (s_sci) statusbar_update_from_sci(s_sci);
}

static void on_indent_click(GtkGestureMultiPress *g, gint n, gdouble x, gdouble y, gpointer d)
{
    (void)n; (void)x; (void)y; (void)d;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g)) != 3) return;
    int cur_tabs  = (int)smsg(SCI_GETUSETABS,  0, 0);
    int cur_width = (int)smsg(SCI_GETTABWIDTH, 0, 0);

    GtkWidget *menu = gtk_menu_new();
    static const int widths[] = {2, 4, 8};

    GtkWidget *sp_hdr = gtk_menu_item_new_with_label("Indent using Spaces");
    gtk_widget_set_sensitive(sp_hdr, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), sp_hdr);
    for (int i = 0; i < 3; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "    %d spaces", widths[i]);
        GtkWidget *mi = gtk_check_menu_item_new_with_label(buf);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi),
                                       !cur_tabs && cur_width == widths[i]);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(mi, "activate", G_CALLBACK(on_indent_item),
                         (gpointer)&kIndentChoices[i]);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *tb_hdr = gtk_menu_item_new_with_label("Indent using Tabs");
    gtk_widget_set_sensitive(tb_hdr, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), tb_hdr);
    for (int i = 0; i < 3; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "    %d wide", widths[i]);
        GtkWidget *mi = gtk_check_menu_item_new_with_label(buf);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi),
                                       cur_tabs && cur_width == widths[i]);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(mi, "activate", G_CALLBACK(on_indent_item),
                         (gpointer)&kIndentChoices[3 + i]);
    }

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), gesture_current_event(g));
}

/* ------------------------------------------------------------------ */
/* Encoding popup                                                      */
/* ------------------------------------------------------------------ */

static void on_enc_item(GtkMenuItem *item, gpointer data)
{
    (void)item;
    if (s_enc_cb) s_enc_cb((const char *)data);
}

static void on_enc_click(GtkGestureMultiPress *g, gint n, gdouble x, gdouble y, gpointer d)
{
    (void)n; (void)x; (void)y; (void)d;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g)) != 3) return;
    const char *cur = gtk_label_get_text(GTK_LABEL(s_lbl_enc));
    GtkWidget *menu = gtk_menu_new();
    for (int i = 0; i < npp_encoding_count; i++) {
        GtkWidget *mi = gtk_check_menu_item_new_with_label(npp_encodings[i].display);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi),
                                       strcmp(npp_encodings[i].display, cur) == 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(mi, "activate", G_CALLBACK(on_enc_item),
                         (gpointer)npp_encodings[i].display);
    }
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), gesture_current_event(g));
}

/* ------------------------------------------------------------------ */
/* Language popup                                                      */
/* ------------------------------------------------------------------ */

typedef struct { const char *key; const char *label; int sep; } LangEntry;
static const LangEntry kCommonLangs[] = {
    { "",           "Normal Text", 0 },
    { NULL, NULL, 1 },
    { "c",          "C",           0 },
    { "cpp",        "C++",         0 },
    { "cs",         "C#",          0 },
    { "java",       "Java",        0 },
    { "python",     "Python",      0 },
    { "javascript", "JavaScript",  0 },
    { "typescript", "TypeScript",  0 },
    { "go",         "Go",          0 },
    { "rust",       "Rust",        0 },
    { "swift",      "Swift",       0 },
    { NULL, NULL, 1 },
    { "php",        "PHP",         0 },
    { "html",       "HTML",        0 },
    { "xml",        "XML",         0 },
    { "css",        "CSS",         0 },
    { "json",       "JSON",        0 },
    { "sql",        "SQL",         0 },
    { "markdown",   "Markdown",    0 },
    { "yaml",       "YAML",        0 },
    { "toml",       "TOML",        0 },
    { NULL, NULL, 1 },
    { "bash",       "Bash",        0 },
    { "lua",        "Lua",         0 },
    { "ruby",       "Ruby",        0 },
    { "perl",       "Perl",        0 },
    { "powershell", "PowerShell",  0 },
    { NULL, NULL, 0 },   /* sentinel */
};

static void on_lang_item(GtkMenuItem *item, gpointer data)
{
    (void)item;
    if (s_lang_cb) s_lang_cb((const char *)data);
}

static void on_lang_click(GtkGestureMultiPress *g, gint n, gdouble x, gdouble y, gpointer d)
{
    (void)n; (void)x; (void)y; (void)d;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g)) != 3) return;
    const char *cur = gtk_label_get_text(GTK_LABEL(s_lbl_lang));
    GtkWidget *menu = gtk_menu_new();
    for (const LangEntry *l = kCommonLangs; l->key || l->sep; l++) {
        if (l->sep) {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                                  gtk_separator_menu_item_new());
            continue;
        }
        gboolean active = (l->key[0] == '\0')
            ? (strcmp(cur, "Normal Text") == 0)
            : (g_ascii_strcasecmp(l->label, cur) == 0);
        GtkWidget *mi = gtk_check_menu_item_new_with_label(l->label);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), active);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect(mi, "activate", G_CALLBACK(on_lang_item), (gpointer)l->key);
    }
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), gesture_current_event(g));
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

GtkWidget *statusbar_init(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(box, 2);
    gtk_widget_set_margin_bottom(box, 2);

    s_lbl_pos = rlabel("Ln 1, Col 1");
    gtk_box_pack_start(GTK_BOX(box), s_lbl_pos, FALSE, FALSE, 0);

    /* right-aligned clickable labels (innermost first = rightmost) */
    s_lbl_lang   = rlabel("Normal Text");
    s_lbl_eol    = rlabel("LF");
    s_lbl_ovr    = rlabel("INS");
    s_lbl_enc    = rlabel("UTF-8");
    s_lbl_indent = rlabel("Spaces: 4");

    gtk_box_pack_end(GTK_BOX(box),
        clickable_wrap(s_lbl_lang,   G_CALLBACK(on_lang_click)),   FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), vsep(), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box),
        clickable_wrap(s_lbl_eol,    G_CALLBACK(on_eol_click)),    FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), vsep(), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box),
        clickable_wrap(s_lbl_ovr,    G_CALLBACK(on_ovr_click)),    FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), vsep(), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box),
        clickable_wrap(s_lbl_enc,    G_CALLBACK(on_enc_click)),    FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), vsep(), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box),
        clickable_wrap(s_lbl_indent, G_CALLBACK(on_indent_click)), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), vsep(), FALSE, FALSE, 0);

    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(frame),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(frame), box, FALSE, FALSE, 0);
    return frame;
}

void statusbar_update_from_sci(GtkWidget *sci)
{
    if (!sci || !s_lbl_pos) return;
    s_sci = sci;

    sptr_t pos  = scintilla_send_message(SCINTILLA(sci), SCI_GETCURRENTPOS, 0, 0);
    int    line = (int)scintilla_send_message(SCINTILLA(sci), SCI_LINEFROMPOSITION, (uptr_t)pos, 0);
    int    col  = (int)scintilla_send_message(SCINTILLA(sci), SCI_GETCOLUMN, (uptr_t)pos, 0);
    char   buf[64];
    snprintf(buf, sizeof(buf), "Ln %d, Col %d", line + 1, col + 1);
    gtk_label_set_text(GTK_LABEL(s_lbl_pos), buf);

    int eol = (int)scintilla_send_message(SCINTILLA(sci), SCI_GETEOLMODE, 0, 0);
    gtk_label_set_text(GTK_LABEL(s_lbl_eol),
        eol == SC_EOL_CRLF ? "CRLF" : eol == SC_EOL_CR ? "CR" : "LF");

    int ovr = (int)scintilla_send_message(SCINTILLA(sci), SCI_GETOVERTYPE, 0, 0);
    gtk_label_set_text(GTK_LABEL(s_lbl_ovr), ovr ? "OVR" : "INS");

    int use_tabs = (int)scintilla_send_message(SCINTILLA(sci), SCI_GETUSETABS, 0, 0);
    int tab_w    = (int)scintilla_send_message(SCINTILLA(sci), SCI_GETTABWIDTH, 0, 0);
    if (tab_w < 1) tab_w = 4;
    snprintf(buf, sizeof(buf), use_tabs ? "Tabs: %d" : "Spaces: %d", tab_w);
    gtk_label_set_text(GTK_LABEL(s_lbl_indent), buf);
}

void statusbar_set_language(const char *lang)
{
    if (s_lbl_lang)
        gtk_label_set_text(GTK_LABEL(s_lbl_lang), lang ? lang : "Normal Text");
}

void statusbar_set_encoding(const char *enc)
{
    if (s_lbl_enc)
        gtk_label_set_text(GTK_LABEL(s_lbl_enc), enc ? enc : "UTF-8");
}

void statusbar_set_overtype(gboolean ovr)
{
    if (s_lbl_ovr)
        gtk_label_set_text(GTK_LABEL(s_lbl_ovr), ovr ? "OVR" : "INS");
}
