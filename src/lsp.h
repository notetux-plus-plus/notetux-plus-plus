#ifndef LSP_H
#define LSP_H

#include <gtk/gtk.h>

/* Initialise LSP subsystem (call once after window is created). */
void lsp_init(GtkWidget *window);

/* Shut down all running language servers gracefully. */
void lsp_shutdown(void);

/* ── Document lifecycle (call from editor.c) ────────────────────────── */

/* Notify LSP that a document was opened.  lang_id is the Lexilla language
 * key as used by lexer.c (e.g. "c", "cpp", "python", "rust"). */
void lsp_doc_open  (GtkWidget *sci, const char *filepath, const char *lang_id);
void lsp_doc_change(GtkWidget *sci, const char *filepath);
void lsp_doc_close (const char *filepath);
void lsp_doc_save  (const char *filepath);

/* Returns TRUE if a language server is configured (and running / starting)
 * for the given lang_id. */
gboolean lsp_available_for(const char *lang_id);

/* ── LSP actions (bound to menu items / keyboard shortcuts) ─────────── */

/* Jump to definition of the symbol at the current caret position. */
void lsp_goto_definition(GtkWidget *sci, const char *filepath);

/* Show hover documentation for the symbol at the current caret. */
void lsp_hover(GtkWidget *sci, const char *filepath);

/* Rename symbol at caret — prompts for new name. */
void lsp_rename(GtkWidget *sci, const char *filepath);

/* ── Completion (called from autocomplete.c) ────────────────────────── */

typedef void (*LspCompletionCb)(const char **items, int count, void *userdata);

/* Request completion items asynchronously.  The callback is invoked on the
 * GTK main thread.  items[] is valid only for the duration of the callback. */
void lsp_request_completion(GtkWidget *sci, const char *filepath,
                             int line, int col,
                             LspCompletionCb cb, void *userdata);

/* ── Diagnostics ────────────────────────────────────────────────────── */

/* Return the number of diagnostics (errors + warnings) for the given file,
 * or -1 if LSP is not tracking that file. */
int lsp_diagnostic_count(const char *filepath);

#endif /* LSP_H */
