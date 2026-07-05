/* Scintilla source code edit control
 * DBCS.c — C translation of scintilla/src/DBCS.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   bool → int  (0/1)
 *   noexcept → removed
 *   std::array<DBCSPair,0x8000> → DBCSPair[0x8000] typedef FoldMap
 *   std::vector<CodePageFoldMap> cpToFoldMap → static heap array
 *   emplace_back → realloc + placement-style struct copy
 *   std::find_if + lambda → linear scan
 */

#include <stdlib.h>
#include <string.h>
#include "DBCS.h"

/* ── DBCSIsLeadByte ──────────────────────────────────────────────────── */

int DBCSIsLeadByte(int codePage, char ch) {
    const unsigned char uch = (unsigned char)ch;
    switch (codePage) {
    case DBCS_cp932:
        return ((uch >= 0x81) && (uch <= 0x9F)) ||
               ((uch >= 0xE0) && (uch <= 0xFC));
    case DBCS_cp936:
        return (uch >= 0x81) && (uch <= 0xFE);
    case DBCS_cp949:
        return (uch >= 0x81) && (uch <= 0xFE);
    case DBCS_cp950:
        return (uch >= 0x81) && (uch <= 0xFE);
    case DBCS_cp1361:
        return ((uch >= 0x84) && (uch <= 0xD3)) ||
               ((uch >= 0xD8) && (uch <= 0xDE)) ||
               ((uch >= 0xE0) && (uch <= 0xF9));
    default:
        break;
    }
    return 0;
}

/* ── DBCSIsTrailByte ─────────────────────────────────────────────────── */

int DBCSIsTrailByte(int codePage, char ch) {
    const unsigned char trail = (unsigned char)ch;
    switch (codePage) {
    case DBCS_cp932:
        return (trail != 0x7F) &&
               ((trail >= 0x40) && (trail <= 0xFC));
    case DBCS_cp936:
        return (trail != 0x7F) &&
               ((trail >= 0x40) && (trail <= 0xFE));
    case DBCS_cp949:
        return ((trail >= 0x41) && (trail <= 0x5A)) ||
               ((trail >= 0x61) && (trail <= 0x7A)) ||
               ((trail >= 0x81) && (trail <= 0xFE));
    case DBCS_cp950:
        return ((trail >= 0x40) && (trail <= 0x7E)) ||
               ((trail >= 0xA1) && (trail <= 0xFE));
    case DBCS_cp1361:
        return ((trail >= 0x31) && (trail <= 0x7E)) ||
               ((trail >= 0x81) && (trail <= 0xFE));
    default:
        break;
    }
    return 0;
}

/* ── IsDBCSValidSingleByte ───────────────────────────────────────────── */

int IsDBCSValidSingleByte(int codePage, int ch) {
    switch (codePage) {
    case DBCS_cp932:
        return ch == 0x80
            || (ch >= 0xA0 && ch <= 0xDF)
            || (ch >= 0xFD);
    case DBCS_cp936:
        return ch == 0x80;
    default:
        return 0;
    }
}

/* ── FoldMap registry ────────────────────────────────────────────────── */

typedef struct {
    int     codePage;
    FoldMap foldMap;          /* 64 KB of DBCSPair — embedded, not a pointer */
} CodePageFoldMap;

static CodePageFoldMap *s_cpToFoldMap = NULL;
static int              s_cpCount     = 0;
static int              s_cpCap       = 0;

FoldMap *DBCSCreateFoldMap(int codePage) {
    if (s_cpCount == s_cpCap) {
        int newCap = s_cpCap ? s_cpCap * 2 : 4;
        CodePageFoldMap *buf = realloc(s_cpToFoldMap,
                                       (size_t)newCap * sizeof(CodePageFoldMap));
        if (!buf) return NULL;
        s_cpToFoldMap = buf;
        s_cpCap       = newCap;
    }
    CodePageFoldMap *entry = &s_cpToFoldMap[s_cpCount++];
    entry->codePage = codePage;
    memset(entry->foldMap, 0, sizeof(entry->foldMap));
    return &entry->foldMap;
}

const FoldMap *DBCSGetFoldMap(int codePage) {
    for (int i = 0; i < s_cpCount; i++) {
        if (s_cpToFoldMap[i].codePage == codePage)
            return (const FoldMap *)&s_cpToFoldMap[i].foldMap;
    }
    return NULL;
}
