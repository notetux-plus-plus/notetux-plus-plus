/* Scintilla source code edit control
 * CaseFolder.h — C translation of scintilla/src/CaseFolder.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   class CaseFolder (abstract base, pure-virtual Fold)
 *     →  CaseFolder_vtbl  (C vtable pattern)
 *        typedef struct CaseFolder { const CaseFolder_vtbl *vtbl; } CaseFolder;
 *
 *   class CaseFolderTable : public CaseFolder
 *     →  CaseFolderTable  (embeds CaseFolder as first member for safe upcast)
 *        char mapping[256];
 *
 *   class CaseFolderUnicode : public CaseFolderTable
 *     →  CaseFolderUnicode  (embeds CaseFolderTable as first member)
 *        ICaseConverter *converter;
 *
 *   delete copy/move constructors  →  no copy helpers in C (just don't copy)
 *   virtual ~CaseFolder()          →  callers use CaseFolder_vtbl.destroy (or
 *                                     just free the concrete object directly)
 *   std::size(mapping)             →  256  (mapping is char[256])
 */

#ifndef CASEFOLDER_C_H
#define CASEFOLDER_C_H

#include <stddef.h>
#include "CaseConvert.h"   /* ICaseConverter */

/* ── CaseFolder (abstract base) ───────────────────────────────────────
 *
 * The vtable carries Fold + destroy so callers can treat any subtype
 * through a CaseFolder * without knowing the concrete type.
 */
typedef struct CaseFolder CaseFolder;

typedef struct {
    size_t (*Fold)(CaseFolder *self,
                   char *folded, size_t sizeFolded,
                   const char *mixed, size_t lenMixed);
    void   (*destroy)(CaseFolder *self);
} CaseFolder_vtbl;

struct CaseFolder {
    const CaseFolder_vtbl *vtbl;   /* must be first */
};

/* Convenience wrappers so callers don't have to dereference the vtable. */
static inline size_t CaseFolder_Fold(CaseFolder *self,
                                      char *folded, size_t sizeFolded,
                                      const char *mixed, size_t lenMixed) {
    return self->vtbl->Fold(self, folded, sizeFolded, mixed, lenMixed);
}
static inline void CaseFolder_destroy(CaseFolder *self) {
    self->vtbl->destroy(self);
}

/* ── CaseFolderTable ──────────────────────────────────────────────────
 *
 * An ASCII lookup table: mapping[i] gives the folded form of byte i.
 * Initialised to standard ASCII lower-case by CaseFolderTable_init().
 */
typedef struct {
    CaseFolder base;       /* vtbl pointer — must be first for safe upcast */
    char       mapping[256];
} CaseFolderTable;

void   CaseFolderTable_init          (CaseFolderTable *self);
size_t CaseFolderTable_Fold          (CaseFolder *self,
                                       char *folded, size_t sizeFolded,
                                       const char *mixed, size_t lenMixed);
void   CaseFolderTable_SetTranslation(CaseFolderTable *self,
                                       char ch, char chTranslation);
void   CaseFolderTable_StandardASCII (CaseFolderTable *self);

/* ── CaseFolderUnicode ────────────────────────────────────────────────
 *
 * Extends CaseFolderTable: single-byte inputs are handled by the
 * mapping table; multi-byte UTF-8 inputs are forwarded to the
 * Unicode case-fold converter.
 */
typedef struct {
    CaseFolderTable base;      /* vtbl + mapping — must be first */
    ICaseConverter *converter;
} CaseFolderUnicode;

void   CaseFolderUnicode_init(CaseFolderUnicode *self);
size_t CaseFolderUnicode_Fold(CaseFolder *self,
                               char *folded, size_t sizeFolded,
                               const char *mixed, size_t lenMixed);

#endif /* CASEFOLDER_C_H */
