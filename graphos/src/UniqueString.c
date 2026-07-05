/* Scintilla source code edit control
 * UniqueString.c — C translation of scintilla/src/UniqueString.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   UniqueString (unique_ptr<const char[]>)  →  const char * (malloc/free)
 *   std::string_view sv(text); sv.copy(…)    →  strlen + memcpy
 *   std::vector<UniqueString> strings        →  const char **items + count/cap
 *   strings.clear()                          →  free each entry, reset count
 *   strings.push_back(UniqueStringCopy(…))   →  realloc + store pointer
 */

#include <stdlib.h>
#include <string.h>
#include "UniqueString.h"

/* ── UniqueStringCopy ────────────────────────────────────────────────── */

const char *UniqueStringCopy(const char *text) {
    if (!text)
        return NULL;
    const size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

/* ── UniqueStringSet ─────────────────────────────────────────────────── */

void USS_init(UniqueStringSet *self) {
    self->items = NULL;
    self->count = 0;
    self->cap   = 0;
}

void USS_Clear(UniqueStringSet *self) {
    for (int i = 0; i < self->count; i++)
        free((void *)self->items[i]);
    self->count = 0;
}

void USS_destroy(UniqueStringSet *self) {
    USS_Clear(self);
    free(self->items);
    self->items = NULL;
    self->cap   = 0;
}

const char *USS_Save(UniqueStringSet *self, const char *text) {
    if (!text)
        return NULL;

    /* Return existing interned pointer if present */
    for (int i = 0; i < self->count; i++) {
        if (strcmp(self->items[i], text) == 0)
            return self->items[i];
    }

    /* Grow the pointer array if needed */
    if (self->count == self->cap) {
        int newCap = self->cap ? self->cap * 2 : 8;
        const char **buf = realloc(self->items,
                                   (size_t)newCap * sizeof(const char *));
        if (!buf) return NULL;
        self->items = buf;
        self->cap   = newCap;
    }

    const char *copy = UniqueStringCopy(text);
    if (!copy) return NULL;
    self->items[self->count++] = copy;
    return copy;
}
