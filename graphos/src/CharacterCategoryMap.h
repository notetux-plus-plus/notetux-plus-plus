/* Scintilla source code edit control
 * CharacterCategoryMap.h — C translation of scintilla/src/CharacterCategoryMap.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   namespace Scintilla::Internal    →  removed; names kept as-is
 *   enum CharacterCategory           →  plain C enum (no class)
 *   std::vector<unsigned char> dense →  unsigned char *dense; int denseLen;
 *   CategoryFor (inline method)      →  CCM_CategoryFor (inline function)
 *   CharacterCategoryMap()           →  CCM_init / CCM_destroy
 */

#ifndef CHARACTERCATEGORYMAP_C_H
#define CHARACTERCATEGORYMAP_C_H

#include <stddef.h>
#include <stdlib.h>

/* ── Unicode general category codes ──────────────────────────────────── */
typedef enum {
    ccLu, ccLl, ccLt, ccLm, ccLo,
    ccMn, ccMc, ccMe,
    ccNd, ccNl, ccNo,
    ccPc, ccPd, ccPs, ccPe, ccPi, ccPf, ccPo,
    ccSm, ccSc, ccSk, ccSo,
    ccZs, ccZl, ccZp,
    ccCc, ccCf, ccCs, ccCo, ccCn
} CharacterCategory;

/* ── Free functions ──────────────────────────────────────────────────── */

/* Binary search through catRanges[]; handles all Unicode code points. */
CharacterCategory CategoriseCharacter(int character);

/* UAX #31 identifier predicates */
int IsIdStart    (int character);
int IsIdContinue (int character);
int IsXidStart   (int character);
int IsXidContinue(int character);

/* ── CharacterCategoryMap ─────────────────────────────────────────────
 *
 * A dense array cache: for common code points (0..denseLen-1) category
 * lookup is O(1); rarer code points fall back to CategoriseCharacter().
 *
 * Initialised with Optimize(256) by CCM_init(), which pre-caches the
 * BMP basic Latin range.  Call CCM_Optimize() to widen the cache.
 */
typedef struct {
    unsigned char *dense;
    int            denseLen;
} CharacterCategoryMap;

void              CCM_init    (CharacterCategoryMap *self);
void              CCM_destroy (CharacterCategoryMap *self);
int               CCM_Size    (const CharacterCategoryMap *self);
void              CCM_Optimize(CharacterCategoryMap *self, int countCharacters);

static inline CharacterCategory CCM_CategoryFor(const CharacterCategoryMap *self,
                                                  int character) {
    if (character >= 0 && character < self->denseLen)
        return (CharacterCategory)self->dense[character];
    return CategoriseCharacter(character);
}

#endif /* CHARACTERCATEGORYMAP_C_H */
