#ifndef LSP_JSON_H
#define LSP_JSON_H

#include <glib.h>

/* ── Minimal JSON value tree ─────────────────────────────────────────── */

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_ERROR
} JsonType;

typedef struct JsonVal JsonVal;
struct JsonVal {
    JsonType type;
    union {
        gboolean    b;
        double      n;
        char       *s;       /* owned, UTF-8 */
        GPtrArray  *arr;     /* of JsonVal* */
        GHashTable *obj;     /* char* (owned) → JsonVal* */
    };
};

/* Parse a JSON string.  Returns NULL on error. */
JsonVal    *json_parse(const char *src);
void        json_free(JsonVal *v);

/* Navigate: return child or NULL */
JsonVal    *json_get(JsonVal *obj, const char *key);
JsonVal    *json_idx(JsonVal *arr, int i);
int         json_arr_len(JsonVal *arr);

/* Typed accessors — return default if wrong type */
const char *json_str(JsonVal *v);          /* NULL  if not STRING */
double      json_num(JsonVal *v);          /* 0.0   if not NUMBER */
int         json_int(JsonVal *v);          /* 0     if not NUMBER */
gboolean    json_bool(JsonVal *v);         /* FALSE if not BOOL   */

/* ── JSON builder (string-based) ─────────────────────────────────────── */

typedef struct {
    GString *s;
    gboolean need_comma; /* internal */
} JsonBuf;

void  jb_init(JsonBuf *b);
void  jb_begin_obj(JsonBuf *b);
void  jb_end_obj(JsonBuf *b);
void  jb_begin_arr(JsonBuf *b);
void  jb_end_arr(JsonBuf *b);
void  jb_key(JsonBuf *b, const char *k);
void  jb_str(JsonBuf *b, const char *v);
void  jb_int(JsonBuf *b, int v);
void  jb_dbl(JsonBuf *b, double v);
void  jb_bool(JsonBuf *b, gboolean v);
void  jb_null(JsonBuf *b);
/* Shorthand: key + value */
void  jb_kstr(JsonBuf *b, const char *k, const char *v);
void  jb_kint(JsonBuf *b, const char *k, int v);
void  jb_kbool(JsonBuf *b, const char *k, gboolean v);
/* Returns the built string (b->s is still valid until jb_free) */
const char *jb_get(JsonBuf *b);
void  jb_free(JsonBuf *b);

#endif /* LSP_JSON_H */
