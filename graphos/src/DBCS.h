/* Scintilla source code edit control
 * DBCS.h — C translation of scintilla/src/DBCS.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   namespace Scintilla::Internal    →  removed
 *   constexpr int cp*                →  enum DBCS_CodePage { ... }
 *   constexpr bool IsDBCSCodePage()  →  static inline int IsDBCSCodePage()
 *   constexpr uint16_t DBCSIndex()   →  static inline uint16_t DBCSIndex()
 *   std::array<DBCSPair,0x8000>      →  DBCSPair foldMap[0x8000]  (typedef FoldMap)
 *   std::vector<CodePageFoldMap>     →  static heap array in DBCS.c
 */

#ifndef DBCS_C_H
#define DBCS_C_H

#include <stdint.h>

/* ── Code-page constants ─────────────────────────────────────────────── */
enum {
    DBCS_cp932  =  932,
    DBCS_cp936  =  936,
    DBCS_cp949  =  949,
    DBCS_cp950  =  950,
    DBCS_cp1361 = 1361
};

static inline int IsDBCSCodePage(int codePage) {
    return codePage == DBCS_cp932
        || codePage == DBCS_cp936
        || codePage == DBCS_cp949
        || codePage == DBCS_cp950
        || codePage == DBCS_cp1361;
}

/* ── DBCSPair + FoldMap ──────────────────────────────────────────────── */
typedef struct { char chars[2]; } DBCSPair;

/* Fixed-size fold table: index via DBCSIndex(ch1, ch2) → DBCSPair */
typedef DBCSPair FoldMap[0x8000];

/* Calculate index from a DBCS byte pair (ch1 must have top bit set). */
static inline uint16_t DBCSIndex(char ch1, char ch2) {
    const unsigned char uch1 = (unsigned char)ch1 & 0x7F;
    const unsigned char uch2 = (unsigned char)ch2;
    return (uint16_t)((uch1 << 8) | uch2);
}

/* ── Range-check predicates ──────────────────────────────────────────── */
int DBCSIsLeadByte        (int codePage, char ch);
int DBCSIsTrailByte       (int codePage, char ch);
int IsDBCSValidSingleByte (int codePage, int  ch);

/* ── FoldMap registry ────────────────────────────────────────────────── */
FoldMap       *DBCSCreateFoldMap(int codePage);
const FoldMap *DBCSGetFoldMap   (int codePage);

#endif /* DBCS_C_H */
