#include "lsp_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════════
 * Recursive-descent JSON parser
 * ════════════════════════════════════════════════════════════════════════ */

typedef struct { const char *p; const char *end; } Ctx;

static void skip_ws(Ctx *c)
{
    while (c->p < c->end &&
           (*c->p == ' ' || *c->p == '\t' || *c->p == '\r' || *c->p == '\n'))
        c->p++;
}

static JsonVal *err_val(void)
{
    JsonVal *v = g_new0(JsonVal, 1);
    v->type = JSON_ERROR;
    return v;
}

static JsonVal *parse_value(Ctx *c);

/* Parse a JSON string (cursor must be on the opening '"') */
static char *parse_string_raw(Ctx *c)
{
    if (c->p >= c->end || *c->p != '"') return NULL;
    c->p++; /* skip " */
    GString *gs = g_string_new(NULL);
    while (c->p < c->end && *c->p != '"') {
        if (*c->p == '\\') {
            c->p++;
            if (c->p >= c->end) break;
            switch (*c->p) {
                case '"':  g_string_append_c(gs, '"');  break;
                case '\\': g_string_append_c(gs, '\\'); break;
                case '/':  g_string_append_c(gs, '/');  break;
                case 'n':  g_string_append_c(gs, '\n'); break;
                case 'r':  g_string_append_c(gs, '\r'); break;
                case 't':  g_string_append_c(gs, '\t'); break;
                case 'b':  g_string_append_c(gs, '\b'); break;
                case 'f':  g_string_append_c(gs, '\f'); break;
                case 'u': {
                    /* \uXXXX */
                    if (c->end - c->p < 5) break;
                    char hex[5] = { c->p[1], c->p[2], c->p[3], c->p[4], 0 };
                    gunichar uc = (gunichar)strtol(hex, NULL, 16);
                    c->p += 4;
                    gchar buf[8];
                    gint len = g_unichar_to_utf8(uc, buf);
                    g_string_append_len(gs, buf, len);
                    break;
                }
                default: g_string_append_c(gs, *c->p); break;
            }
        } else {
            g_string_append_c(gs, *c->p);
        }
        c->p++;
    }
    if (c->p < c->end) c->p++; /* skip closing " */
    return g_string_free(gs, FALSE);
}

static JsonVal *parse_string(Ctx *c)
{
    char *s = parse_string_raw(c);
    if (!s) return err_val();
    JsonVal *v = g_new0(JsonVal, 1);
    v->type = JSON_STRING;
    v->s    = s;
    return v;
}

static JsonVal *parse_number(Ctx *c)
{
    char *end;
    double d = strtod(c->p, &end);
    JsonVal *v = g_new0(JsonVal, 1);
    v->type = JSON_NUMBER;
    v->n    = d;
    c->p    = end;
    return v;
}

static JsonVal *parse_array(Ctx *c)
{
    c->p++; /* skip [ */
    JsonVal *v = g_new0(JsonVal, 1);
    v->type = JSON_ARRAY;
    v->arr  = g_ptr_array_new_with_free_func((GDestroyNotify)json_free);
    skip_ws(c);
    if (c->p < c->end && *c->p == ']') { c->p++; return v; }
    while (c->p < c->end) {
        skip_ws(c);
        JsonVal *elem = parse_value(c);
        g_ptr_array_add(v->arr, elem);
        skip_ws(c);
        if (c->p >= c->end) break;
        if (*c->p == ',') { c->p++; continue; }
        if (*c->p == ']') { c->p++; break; }
        break;
    }
    return v;
}

static JsonVal *parse_object(Ctx *c)
{
    c->p++; /* skip { */
    JsonVal *v = g_new0(JsonVal, 1);
    v->type = JSON_OBJECT;
    v->obj  = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     g_free, (GDestroyNotify)json_free);
    skip_ws(c);
    if (c->p < c->end && *c->p == '}') { c->p++; return v; }
    while (c->p < c->end) {
        skip_ws(c);
        if (*c->p != '"') break;
        char *key = parse_string_raw(c);
        if (!key) break;
        skip_ws(c);
        if (c->p >= c->end || *c->p != ':') { g_free(key); break; }
        c->p++; /* skip : */
        skip_ws(c);
        JsonVal *val = parse_value(c);
        g_hash_table_insert(v->obj, key, val);
        skip_ws(c);
        if (c->p >= c->end) break;
        if (*c->p == ',') { c->p++; continue; }
        if (*c->p == '}') { c->p++; break; }
        break;
    }
    return v;
}

static JsonVal *parse_value(Ctx *c)
{
    skip_ws(c);
    if (c->p >= c->end) return err_val();
    char ch = *c->p;
    if (ch == '"') return parse_string(c);
    if (ch == '{') return parse_object(c);
    if (ch == '[') return parse_array(c);
    if (ch == 't' && c->end - c->p >= 4 && memcmp(c->p, "true", 4) == 0) {
        c->p += 4;
        JsonVal *v = g_new0(JsonVal, 1); v->type = JSON_BOOL; v->b = TRUE; return v;
    }
    if (ch == 'f' && c->end - c->p >= 5 && memcmp(c->p, "false", 5) == 0) {
        c->p += 5;
        JsonVal *v = g_new0(JsonVal, 1); v->type = JSON_BOOL; v->b = FALSE; return v;
    }
    if (ch == 'n' && c->end - c->p >= 4 && memcmp(c->p, "null", 4) == 0) {
        c->p += 4;
        JsonVal *v = g_new0(JsonVal, 1); v->type = JSON_NULL; return v;
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) return parse_number(c);
    return err_val();
}

JsonVal *json_parse(const char *src)
{
    if (!src) return NULL;
    Ctx c = { src, src + strlen(src) };
    JsonVal *v = parse_value(&c);
    if (!v || v->type == JSON_ERROR) { json_free(v); return NULL; }
    return v;
}

void json_free(JsonVal *v)
{
    if (!v) return;
    switch (v->type) {
        case JSON_STRING: g_free(v->s); break;
        case JSON_ARRAY:  g_ptr_array_unref(v->arr); break;
        case JSON_OBJECT: g_hash_table_unref(v->obj); break;
        default: break;
    }
    g_free(v);
}

JsonVal *json_get(JsonVal *obj, const char *key)
{
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return g_hash_table_lookup(obj->obj, key);
}

JsonVal *json_idx(JsonVal *arr, int i)
{
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    if (i < 0 || (guint)i >= arr->arr->len) return NULL;
    return g_ptr_array_index(arr->arr, (guint)i);
}

int json_arr_len(JsonVal *arr)
{
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return (int)arr->arr->len;
}

const char *json_str(JsonVal *v)
{
    return (v && v->type == JSON_STRING) ? v->s : NULL;
}

double json_num(JsonVal *v)
{
    return (v && v->type == JSON_NUMBER) ? v->n : 0.0;
}

int json_int(JsonVal *v)
{
    return (v && v->type == JSON_NUMBER) ? (int)v->n : 0;
}

gboolean json_bool(JsonVal *v)
{
    return (v && v->type == JSON_BOOL) ? v->b : FALSE;
}

/* ════════════════════════════════════════════════════════════════════════
 * JSON builder
 * ════════════════════════════════════════════════════════════════════════ */

void jb_init(JsonBuf *b)
{
    b->s          = g_string_new(NULL);
    b->need_comma = FALSE;
}

static void jb_sep(JsonBuf *b)
{
    if (b->need_comma) g_string_append_c(b->s, ',');
    b->need_comma = FALSE;
}

void jb_begin_obj(JsonBuf *b) { jb_sep(b); g_string_append_c(b->s, '{'); b->need_comma = FALSE; }
void jb_end_obj  (JsonBuf *b) { g_string_append_c(b->s, '}'); b->need_comma = TRUE; }
void jb_begin_arr(JsonBuf *b) { jb_sep(b); g_string_append_c(b->s, '['); b->need_comma = FALSE; }
void jb_end_arr  (JsonBuf *b) { g_string_append_c(b->s, ']'); b->need_comma = TRUE; }

/* Append a JSON-escaped string value (no surrounding quotes) */
static void append_escaped(GString *s, const char *v)
{
    for (; *v; v++) {
        unsigned char c = (unsigned char)*v;
        switch (c) {
            case '"':  g_string_append(s, "\\\""); break;
            case '\\': g_string_append(s, "\\\\"); break;
            case '\n': g_string_append(s, "\\n");  break;
            case '\r': g_string_append(s, "\\r");  break;
            case '\t': g_string_append(s, "\\t");  break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    g_string_append(s, esc);
                } else {
                    g_string_append_c(s, (char)c);
                }
                break;
        }
    }
}

void jb_key(JsonBuf *b, const char *k)
{
    jb_sep(b);
    g_string_append_c(b->s, '"');
    append_escaped(b->s, k);
    g_string_append(b->s, "\":");
    b->need_comma = FALSE;
}

void jb_str(JsonBuf *b, const char *v)
{
    jb_sep(b);
    g_string_append_c(b->s, '"');
    if (v) append_escaped(b->s, v);
    g_string_append_c(b->s, '"');
    b->need_comma = TRUE;
}

void jb_int(JsonBuf *b, int v)
{
    jb_sep(b);
    g_string_append_printf(b->s, "%d", v);
    b->need_comma = TRUE;
}

void jb_dbl(JsonBuf *b, double v)
{
    jb_sep(b);
    g_string_append_printf(b->s, "%g", v);
    b->need_comma = TRUE;
}

void jb_bool(JsonBuf *b, gboolean v)
{
    jb_sep(b);
    g_string_append(b->s, v ? "true" : "false");
    b->need_comma = TRUE;
}

void jb_null(JsonBuf *b)
{
    jb_sep(b);
    g_string_append(b->s, "null");
    b->need_comma = TRUE;
}

void jb_kstr(JsonBuf *b, const char *k, const char *v) { jb_key(b, k); jb_str(b, v); }
void jb_kint(JsonBuf *b, const char *k, int v)          { jb_key(b, k); jb_int(b, v); }
void jb_kbool(JsonBuf *b, const char *k, gboolean v)    { jb_key(b, k); jb_bool(b, v); }

const char *jb_get(JsonBuf *b) { return b->s->str; }

void jb_free(JsonBuf *b) { g_string_free(b->s, TRUE); b->s = NULL; }
