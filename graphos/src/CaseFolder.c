/* Scintilla source code edit control
 * CaseFolder.c — C translation of scintilla/src/CaseFolder.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   class CaseFolder          →  CaseFolder + CaseFolder_vtbl  (C vtable)
 *   class CaseFolderTable     →  CaseFolderTable (embeds CaseFolder as first member)
 *   class CaseFolderUnicode   →  CaseFolderUnicode (embeds CaseFolderTable)
 *
 *   (unsigned char)ch         →  (unsigned char)ch  (same)
 *   std::size(mapping)        →  256
 *   MakeLowerCase(i)          →  CharacterType.h  MakeLowerCase(i)
 *   ConverterFor(CaseConversion::fold) → ConverterFor(CASE_FOLD)
 *   converter->CaseConvertString(…)   → converter->CaseConvertString(converter, …)
 *
 *   No heap allocation needed: both concrete types are meant to be used as
 *   stack/embedded objects (the C++ code also uses them that way).
 *   CaseFolderTable_destroy and CaseFolderUnicode_destroy are no-ops.
 */

#include "CaseFolder.h"
#include "CharacterType.h"

/* ── static vtables ──────────────────────────────────────────────────── */

static void CFT_destroy(CaseFolder *self) { (void)self; /* no heap to free */ }
static void CFU_destroy(CaseFolder *self) { (void)self; }

static const CaseFolder_vtbl s_cft_vtbl = {
    CaseFolderTable_Fold,
    CFT_destroy,
};

static const CaseFolder_vtbl s_cfu_vtbl = {
    CaseFolderUnicode_Fold,
    CFU_destroy,
};

/* ── CaseFolderTable ─────────────────────────────────────────────────── */

void CaseFolderTable_init(CaseFolderTable *self) {
    self->base.vtbl = &s_cft_vtbl;
    CaseFolderTable_StandardASCII(self);
}

size_t CaseFolderTable_Fold(CaseFolder *base,
                              char *folded, size_t sizeFolded,
                              const char *mixed, size_t lenMixed) {
    CaseFolderTable *self = (CaseFolderTable *)base;
    if (lenMixed > sizeFolded)
        return 0;
    for (size_t i = 0; i < lenMixed; i++)
        folded[i] = self->mapping[(unsigned char)mixed[i]];
    return lenMixed;
}

void CaseFolderTable_SetTranslation(CaseFolderTable *self,
                                     char ch, char chTranslation) {
    self->mapping[(unsigned char)ch] = chTranslation;
}

void CaseFolderTable_StandardASCII(CaseFolderTable *self) {
    for (int i = 0; i < 256; i++)
        self->mapping[i] = (char)Sci_MakeLowerCase(i);
}

/* ── CaseFolderUnicode ───────────────────────────────────────────────── */

void CaseFolderUnicode_init(CaseFolderUnicode *self) {
    CaseFolderTable_init(&self->base);
    self->base.base.vtbl = &s_cfu_vtbl;   /* override table vtbl with unicode vtbl */
    self->converter = ConverterFor(CASE_FOLD);
}

size_t CaseFolderUnicode_Fold(CaseFolder *base,
                               char *folded, size_t sizeFolded,
                               const char *mixed, size_t lenMixed) {
    CaseFolderUnicode *self = (CaseFolderUnicode *)base;
    if (lenMixed == 1 && sizeFolded > 0) {
        folded[0] = self->base.mapping[(unsigned char)mixed[0]];
        return 1;
    }
    return self->converter->CaseConvertString(self->converter,
                                               folded, sizeFolded,
                                               mixed, lenMixed);
}
