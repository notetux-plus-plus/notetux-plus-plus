/* Scintilla source code edit control
 * UniqueString.h — C translation of scintilla/src/UniqueString.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   using UniqueString = std::unique_ptr<const char[]>
 *     →  const char *  (caller owns via malloc/free; UniqueStringFree releases it)
 *
 *   class UniqueStringSet  →  UniqueStringSet struct + USS_* functions
 *   std::vector<UniqueString> strings  →  const char **items; int count; int cap;
 */

#ifndef UNIQUESTRING_C_H
#define UNIQUESTRING_C_H

/* Is text NULL or empty? */
static inline int IsNullOrEmpty(const char *text) {
    return !text || *text == '\0';
}

/* Equivalent to strdup — returns a malloc'd copy the caller must free. */
const char *UniqueStringCopy(const char *text);

/* ── UniqueStringSet ──────────────────────────────────────────────────
 *
 * Intern table: Save() always returns the same pointer for equal strings,
 * so callers can compare font names by address rather than by content.
 * The set owns all stored strings; USS_destroy() or USS_Clear() frees them.
 */
typedef struct {
    const char **items;
    int          count;
    int          cap;
} UniqueStringSet;

void        USS_init   (UniqueStringSet *self);
void        USS_destroy(UniqueStringSet *self);
void        USS_Clear  (UniqueStringSet *self);
const char *USS_Save   (UniqueStringSet *self, const char *text);

#endif /* UNIQUESTRING_C_H */
