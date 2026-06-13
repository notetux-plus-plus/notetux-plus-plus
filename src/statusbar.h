#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <gtk/gtk.h>

typedef void (*StatusbarEncodingCb)(const char *enc_name);
typedef void (*StatusbarLanguageCb)(const char *lang_key);

GtkWidget *statusbar_init(void);
void       statusbar_update_from_sci(GtkWidget *sci);
void       statusbar_set_language(const char *lang);
void       statusbar_set_encoding(const char *enc);
void       statusbar_set_overtype(gboolean ovr);

/* Register callbacks invoked when the user changes encoding or language
 * via the status bar context menus. Called once after statusbar_init(). */
void       statusbar_set_encoding_cb(StatusbarEncodingCb cb);
void       statusbar_set_language_cb(StatusbarLanguageCb cb);

#endif /* STATUSBAR_H */
