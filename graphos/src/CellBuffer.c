/* Scintilla source code edit control
 * CellBuffer.c — C translation of scintilla/src/CellBuffer.cxx
 *
 * ── C++ → C translation summary ──────────────────────────────────────
 *
 *  template <typename POS> class LineStartIndex
 *  → two concrete structs: LineStartIndex_Int, LineStartIndex_PD
 *    using the same X-macro trick as SplitVector / Partitioning.
 *
 *  template <typename POS> class LineVector : public ILineVector
 *  → LineVector32 (POS=int) and LineVector64 (POS=ptrdiff_t)
 *    each with its own static ILineVector_vtbl instance.
 *    C++ virtual dispatch: vtbl->fn(self, ...) — same as C++ vptr call.
 *
 *  std::unique_ptr<UndoHistory> uh       → UndoHistory *uh
 *  std::unique_ptr<ChangeHistory> ch     → ChangeHistory *changeHistory
 *  std::unique_ptr<ILineVector> plv      → ILineVector *plv
 *
 *  uh = std::make_unique<UndoHistory>()  → UndoHistory_create()
 *  uh.reset()                            → UndoHistory_destroy(uh); uh=NULL
 *  changeHistory.reset()                 → ChangeHistory_destroy(ch); ch=NULL
 *
 *  std::string text(width, '\0'); text.resize(w); GetCharRange(text.data(), …)
 *  → char *text = malloc(w); CellBuffer_GetCharRange(self, text, …); free(text)
 *    (we use a VLA or malloc depending on size)
 *
 *  throw std::runtime_error("…")         → assert(0); abort()
 *
 *  if constexpr (std::is_convertible_v<Sci::Position *, POS *>)
 *  → determined at runtime by checking largeDocument flag:
 *      largeDocument = 0 → LineVector32 (POS=int, not convertible from ptrdiff_t*)
 *      largeDocument ≠ 0 → LineVector64 (POS=ptrdiff_t, convertible from ptrdiff_t*)
 *
 *  bool                 → int (0/1)
 *  noexcept             → removed
 *  [[nodiscard]]        → SCI_NODISCARD (attribute, in header)
 *  PLATFORM_ASSERT(x)   → assert(x)
 *  Platform::DebugPrintf → fprintf(stderr, …)
 *
 *  constexpr POS pos_cast(Sci::Position p)    → (POS)(p)
 *  constexpr Sci::Line line_from_pos_cast(POS l) → (SciLine)(l)
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include "Position.h"
#include "ScintillaTypes.h"
#include "CellBufferTypes.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "UniConversion.h"
#include "UndoHistory.h"
#include "ChangeHistory.h"
#include "CellBuffer.h"

/* ────────────────────────────────────────────────────────────────────
 * 1. LineStartIndex  (two instantiations: _Int and _PD)
 *
 * C++ original:
 *   template <typename POS>
 *   class LineStartIndex {
 *       int refCount;
 *       Partitioning<POS> starts;
 *       bool Allocate(Sci::Line lines);
 *       bool Release();
 *       bool Active() const noexcept;
 *       Sci::Position LineWidth(Sci::Line line) const noexcept;
 *       void SetLineWidth(Sci::Line line, Sci::Position width) noexcept;
 *       void AllocateLines(Sci::Line lines);
 *       void InsertLines(Sci::Line line, Sci::Line lines);
 *   };
 * ────────────────────────────────────────────────────────────────────
 */

/* ── LineStartIndex_Int ──────────────────────────────────────────── */
typedef struct {
    int              refCount;
    Partitioning_Int starts;
} LineStartIndex_Int;

static void LSI_Int_init(LineStartIndex_Int *self) {
    self->refCount = 0;
    Partitioning_Int_init(&self->starts);
    /* Minimal allocation: starts already has 2 slots from init() */
}

static void LSI_Int_destroy(LineStartIndex_Int *self) {
    Partitioning_Int_destroy(&self->starts);
}

static int LSI_Int_Allocate(LineStartIndex_Int *self, SciLine lines) {
    self->refCount++;
    SciPosition length = Partitioning_Int_PositionFromPartition(
        &self->starts, Partitioning_Int_Partitions(&self->starts));
    for (SciLine line = Partitioning_Int_Partitions(&self->starts); line < lines; line++) {
        length++;
        Partitioning_Int_InsertPartition(&self->starts, (int)line, (int)length);
    }
    return self->refCount == 1;
}

static int LSI_Int_Release(LineStartIndex_Int *self) {
    if (self->refCount == 1) {
        Partitioning_Int_DeleteAll(&self->starts);
    }
    self->refCount--;
    return self->refCount == 0;
}

static int LSI_Int_Active(const LineStartIndex_Int *self) {
    return self->refCount > 0;
}

static SciPosition LSI_Int_LineWidth(const LineStartIndex_Int *self, SciLine line) {
    return (SciPosition)Partitioning_Int_PositionFromPartition(&self->starts, (int)line + 1) -
           (SciPosition)Partitioning_Int_PositionFromPartition(&self->starts, (int)line);
}

static void LSI_Int_SetLineWidth(LineStartIndex_Int *self, SciLine line,
                                   SciPosition width) {
    const SciPosition widthCurrent = LSI_Int_LineWidth(self, line);
    Partitioning_Int_InsertText(&self->starts, (int)line, (int)(width - widthCurrent));
}

static void LSI_Int_AllocateLines(LineStartIndex_Int *self, SciLine lines) {
    if (lines > Partitioning_Int_Partitions(&self->starts)) {
        Partitioning_Int_ReAllocate(&self->starts, (ptrdiff_t)lines);
    }
}

static void LSI_Int_InsertLines(LineStartIndex_Int *self, SciLine line, SciLine lines) {
    const int lineAsPos = (int)line;
    const int lineStart = Partitioning_Int_PositionFromPartition(&self->starts,
                                                                    lineAsPos - 1) + 1;
    for (int l = 0; l < (int)lines; l++) {
        Partitioning_Int_InsertPartition(&self->starts, lineAsPos + l, lineStart + l);
    }
}

/* ── LineStartIndex_PD ───────────────────────────────────────────── */
typedef struct {
    int             refCount;
    Partitioning_PD starts;
} LineStartIndex_PD;

static void LSI_PD_init(LineStartIndex_PD *self) {
    self->refCount = 0;
    Partitioning_PD_init(&self->starts);
}

static void LSI_PD_destroy(LineStartIndex_PD *self) {
    Partitioning_PD_destroy(&self->starts);
}

static int LSI_PD_Allocate(LineStartIndex_PD *self, SciLine lines) {
    self->refCount++;
    SciPosition length = Partitioning_PD_PositionFromPartition(
        &self->starts, Partitioning_PD_Partitions(&self->starts));
    for (SciLine line = Partitioning_PD_Partitions(&self->starts); line < lines; line++) {
        length++;
        Partitioning_PD_InsertPartition(&self->starts, (ptrdiff_t)line, (ptrdiff_t)length);
    }
    return self->refCount == 1;
}

static int LSI_PD_Release(LineStartIndex_PD *self) {
    if (self->refCount == 1) {
        Partitioning_PD_DeleteAll(&self->starts);
    }
    self->refCount--;
    return self->refCount == 0;
}

static int LSI_PD_Active(const LineStartIndex_PD *self) {
    return self->refCount > 0;
}

static SciPosition LSI_PD_LineWidth(const LineStartIndex_PD *self, SciLine line) {
    return Partitioning_PD_PositionFromPartition(&self->starts, (ptrdiff_t)line + 1) -
           Partitioning_PD_PositionFromPartition(&self->starts, (ptrdiff_t)line);
}

static void LSI_PD_SetLineWidth(LineStartIndex_PD *self, SciLine line,
                                  SciPosition width) {
    const SciPosition widthCurrent = LSI_PD_LineWidth(self, line);
    Partitioning_PD_InsertText(&self->starts, (ptrdiff_t)line, (ptrdiff_t)(width - widthCurrent));
}

static void LSI_PD_AllocateLines(LineStartIndex_PD *self, SciLine lines) {
    if (lines > Partitioning_PD_Partitions(&self->starts)) {
        Partitioning_PD_ReAllocate(&self->starts, (ptrdiff_t)lines);
    }
}

static void LSI_PD_InsertLines(LineStartIndex_PD *self, SciLine line, SciLine lines) {
    const ptrdiff_t lineAsPos = (ptrdiff_t)line;
    const ptrdiff_t lineStart = Partitioning_PD_PositionFromPartition(&self->starts,
                                                                          lineAsPos - 1) + 1;
    for (ptrdiff_t l = 0; l < (ptrdiff_t)lines; l++) {
        Partitioning_PD_InsertPartition(&self->starts, lineAsPos + l, lineStart + l);
    }
}

/* ────────────────────────────────────────────────────────────────────
 * 2. LineVector32 and LineVector64
 *
 * C++ original:
 *   template <typename POS> class LineVector : public ILineVector { … }
 *
 * Each concrete type has:
 *   Partitioning<POS> starts          — line-start positions
 *   PerLine *perLine                  — per-line callback
 *   LineStartIndex<POS> startsUTF16   — character-width index (UTF-16)
 *   LineStartIndex<POS> startsUTF32   — character-width index (UTF-32)
 *   LineCharacterIndexType activeIndices
 *
 * Virtual dispatch: the ILineVector vtable pointer is the FIRST member of
 * the LineVector struct.  Callers hold ILineVector * which they cast to the
 * concrete type when needed.
 * ────────────────────────────────────────────────────────────────────
 */

/* ── Helper: count UTF-8 character widths (for character index) ────── */
static CountWidths CountCharacterWidthsUTF8(const char *s, size_t len) {
    CountWidths cw;
    cw.countBasePlane  = 0;
    cw.countOtherPlanes = 0;
    size_t remaining = len;
    while (remaining > 0) {
        const int utf8Status = UTF8Classify((const unsigned char *)s, remaining);
        const int lenChar = utf8Status & UTF8MaskWidth;
        CountWidths_CountChar(&cw, lenChar);
        s         += lenChar;
        remaining -= (size_t)lenChar;
    }
    return cw;
}

/* ═══ LineVector32 (POS = int, small documents) ══════════════════════ */

typedef struct {
    const ILineVector_vtbl  *vtbl;       /* MUST be first */
    Partitioning_Int         starts;
    PerLine                 *perLine;
    LineStartIndex_Int       startsUTF16;
    LineStartIndex_Int       startsUTF32;
    LineCharacterIndexType   activeIndices;
} LineVector32;

static void LV32_SetActiveIndices(LineVector32 *self) {
    self->activeIndices =
        (LineCharacterIndexType)(
            (LSI_Int_Active(&self->startsUTF32) ? LineCharacterIndexType_Utf32 : LineCharacterIndexType_None) |
            (LSI_Int_Active(&self->startsUTF16) ? LineCharacterIndexType_Utf16 : LineCharacterIndexType_None));
}

/* Forward declarations of vtable functions */
static void LV32_destroy         (ILineVector *self);
static void LV32_Init            (ILineVector *self);
static void LV32_SetPerLine      (ILineVector *self, PerLine *pl);
static void LV32_InsertText      (ILineVector *self, SciLine line, SciPosition delta);
static void LV32_InsertLine      (ILineVector *self, SciLine line, SciPosition position, int lineStart);
static void LV32_InsertLines     (ILineVector *self, SciLine line, const SciPosition *positions, size_t lines, int lineStart);
static void LV32_SetLineStart    (ILineVector *self, SciLine line, SciPosition position);
static void LV32_RemoveLine      (ILineVector *self, SciLine line);
static SciLine LV32_Lines        (const ILineVector *self);
static void LV32_AllocateLines   (ILineVector *self, SciLine lines);
static SciLine LV32_LineFromPosition(const ILineVector *self, SciPosition pos);
static SciPosition LV32_LineStart(const ILineVector *self, SciLine line);
static void LV32_InsertCharacters(ILineVector *self, SciLine line, CountWidths delta);
static void LV32_SetLineCharactersWidth(ILineVector *self, SciLine line, CountWidths width);
static LineCharacterIndexType LV32_LineCharacterIndex(const ILineVector *self);
static int  LV32_AllocateLineCharacterIndex(ILineVector *self, LineCharacterIndexType lci, SciLine lines);
static int  LV32_ReleaseLineCharacterIndex(ILineVector *self, LineCharacterIndexType lci);
static SciPosition LV32_IndexLineStart(const ILineVector *self, SciLine line, LineCharacterIndexType lci);
static SciLine LV32_LineFromPositionIndex(const ILineVector *self, SciPosition pos, LineCharacterIndexType lci);

static const ILineVector_vtbl lv32_vtbl = {
    LV32_destroy,
    LV32_Init,
    LV32_SetPerLine,
    LV32_InsertText,
    LV32_InsertLine,
    LV32_InsertLines,
    LV32_SetLineStart,
    LV32_RemoveLine,
    LV32_Lines,
    LV32_AllocateLines,
    LV32_LineFromPosition,
    LV32_LineStart,
    LV32_InsertCharacters,
    LV32_SetLineCharactersWidth,
    LV32_LineCharacterIndex,
    LV32_AllocateLineCharacterIndex,
    LV32_ReleaseLineCharacterIndex,
    LV32_IndexLineStart,
    LV32_LineFromPositionIndex,
};

static ILineVector *LineVector32_create(void) {
    LineVector32 *lv = (LineVector32 *)calloc(1, sizeof(LineVector32));
    assert(lv);
    lv->vtbl = &lv32_vtbl;
    Partitioning_Int_init(&lv->starts);
    /* Change from Partitioning(256): call ReAllocate to hint size */
    Partitioning_Int_ReAllocate(&lv->starts, 256);
    lv->perLine = NULL;
    LSI_Int_init(&lv->startsUTF16);
    LSI_Int_init(&lv->startsUTF32);
    lv->activeIndices = LineCharacterIndexType_None;
    return (ILineVector *)lv;
}

static void LV32_destroy(ILineVector *ilv) {
    LineVector32 *self = (LineVector32 *)ilv;
    Partitioning_Int_destroy(&self->starts);
    LSI_Int_destroy(&self->startsUTF16);
    LSI_Int_destroy(&self->startsUTF32);
    free(self);
}

static void LV32_Init(ILineVector *ilv) {
    LineVector32 *self = (LineVector32 *)ilv;
    Partitioning_Int_DeleteAll(&self->starts);
    if (self->perLine)
        self->perLine->vtbl->Init(self->perLine);
    Partitioning_Int_DeleteAll(&self->startsUTF32.starts);
    Partitioning_Int_DeleteAll(&self->startsUTF16.starts);
}

static void LV32_SetPerLine(ILineVector *ilv, PerLine *pl) {
    ((LineVector32 *)ilv)->perLine = pl;
}

static void LV32_InsertText(ILineVector *ilv, SciLine line, SciPosition delta) {
    Partitioning_Int_InsertText(&((LineVector32 *)ilv)->starts, (int)line, (int)delta);
}

static void LV32_InsertLine(ILineVector *ilv, SciLine line, SciPosition position, int lineStart) {
    LineVector32 *self = (LineVector32 *)ilv;
    Partitioning_Int_InsertPartition(&self->starts, (int)line, (int)position);
    if (self->activeIndices != LineCharacterIndexType_None) {
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
            LSI_Int_InsertLines(&self->startsUTF32, line, 1);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
            LSI_Int_InsertLines(&self->startsUTF16, line, 1);
    }
    if (self->perLine) {
        SciLine l = line;
        if ((l > 0) && lineStart)
            l--;
        self->perLine->vtbl->InsertLine(self->perLine, l);
    }
}

static void LV32_InsertLines(ILineVector *ilv, SciLine line, const SciPosition *positions,
                               size_t lines, int lineStart) {
    LineVector32 *self = (LineVector32 *)ilv;
    /* POS=int; positions are SciPosition (ptrdiff_t) → need WithCast since not same type */
    Partitioning_Int_InsertPartitionsWithCast(&self->starts, (int)line, positions, lines);
    if (self->activeIndices != LineCharacterIndexType_None) {
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
            LSI_Int_InsertLines(&self->startsUTF32, line, (SciLine)lines);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
            LSI_Int_InsertLines(&self->startsUTF16, line, (SciLine)lines);
    }
    if (self->perLine) {
        SciLine l = line;
        if ((l > 0) && lineStart)
            l--;
        self->perLine->vtbl->InsertLines(self->perLine, l, (SciLine)lines);
    }
}

static void LV32_SetLineStart(ILineVector *ilv, SciLine line, SciPosition position) {
    Partitioning_Int_SetPartitionStartPosition(&((LineVector32 *)ilv)->starts,
                                                (int)line, (int)position);
}

static void LV32_RemoveLine(ILineVector *ilv, SciLine line) {
    LineVector32 *self = (LineVector32 *)ilv;
    Partitioning_Int_RemovePartition(&self->starts, (int)line);
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
        Partitioning_Int_RemovePartition(&self->startsUTF32.starts, (int)line);
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
        Partitioning_Int_RemovePartition(&self->startsUTF16.starts, (int)line);
    if (self->perLine)
        self->perLine->vtbl->RemoveLine(self->perLine, line);
}

static SciLine LV32_Lines(const ILineVector *ilv) {
    return (SciLine)Partitioning_Int_Partitions(&((const LineVector32 *)ilv)->starts);
}

static void LV32_AllocateLines(ILineVector *ilv, SciLine lines) {
    LineVector32 *self = (LineVector32 *)ilv;
    if (lines > LV32_Lines(ilv)) {
        Partitioning_Int_ReAllocate(&self->starts, (ptrdiff_t)lines);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
            LSI_Int_AllocateLines(&self->startsUTF32, lines);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
            LSI_Int_AllocateLines(&self->startsUTF16, lines);
    }
}

static SciLine LV32_LineFromPosition(const ILineVector *ilv, SciPosition pos) {
    return (SciLine)Partitioning_Int_PartitionFromPosition(
        &((const LineVector32 *)ilv)->starts, (int)pos);
}

static SciPosition LV32_LineStart(const ILineVector *ilv, SciLine line) {
    return (SciPosition)Partitioning_Int_PositionFromPartition(
        &((const LineVector32 *)ilv)->starts, (int)line);
}

static void LV32_InsertCharacters(ILineVector *ilv, SciLine line, CountWidths delta) {
    LineVector32 *self = (LineVector32 *)ilv;
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
        Partitioning_Int_InsertText(&self->startsUTF32.starts, (int)line,
                                     (int)CountWidths_WidthUTF32(&delta));
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
        Partitioning_Int_InsertText(&self->startsUTF16.starts, (int)line,
                                     (int)CountWidths_WidthUTF16(&delta));
}

static void LV32_SetLineCharactersWidth(ILineVector *ilv, SciLine line, CountWidths width) {
    LineVector32 *self = (LineVector32 *)ilv;
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32)) {
        assert(Partitioning_Int_Partitions(&self->startsUTF32.starts) ==
               Partitioning_Int_Partitions(&self->starts));
        LSI_Int_SetLineWidth(&self->startsUTF32, line, CountWidths_WidthUTF32(&width));
    }
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16)) {
        assert(Partitioning_Int_Partitions(&self->startsUTF16.starts) ==
               Partitioning_Int_Partitions(&self->starts));
        LSI_Int_SetLineWidth(&self->startsUTF16, line, CountWidths_WidthUTF16(&width));
    }
}

static LineCharacterIndexType LV32_LineCharacterIndex(const ILineVector *ilv) {
    return ((const LineVector32 *)ilv)->activeIndices;
}

static int LV32_AllocateLineCharacterIndex(ILineVector *ilv, LineCharacterIndexType lci,
                                             SciLine lines) {
    LineVector32 *self = (LineVector32 *)ilv;
    const LineCharacterIndexType before = self->activeIndices;
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf32)) {
        LSI_Int_Allocate(&self->startsUTF32, lines);
        assert(Partitioning_Int_Partitions(&self->startsUTF32.starts) ==
               Partitioning_Int_Partitions(&self->starts));
    }
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf16)) {
        LSI_Int_Allocate(&self->startsUTF16, lines);
        assert(Partitioning_Int_Partitions(&self->startsUTF16.starts) ==
               Partitioning_Int_Partitions(&self->starts));
    }
    LV32_SetActiveIndices(self);
    return before != self->activeIndices;
}

static int LV32_ReleaseLineCharacterIndex(ILineVector *ilv, LineCharacterIndexType lci) {
    LineVector32 *self = (LineVector32 *)ilv;
    const LineCharacterIndexType before = self->activeIndices;
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf32))
        LSI_Int_Release(&self->startsUTF32);
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf16))
        LSI_Int_Release(&self->startsUTF16);
    LV32_SetActiveIndices(self);
    return before != self->activeIndices;
}

static SciPosition LV32_IndexLineStart(const ILineVector *ilv, SciLine line,
                                        LineCharacterIndexType lci) {
    const LineVector32 *self = (const LineVector32 *)ilv;
    if (lci == LineCharacterIndexType_Utf32)
        return (SciPosition)Partitioning_Int_PositionFromPartition(
            &self->startsUTF32.starts, (int)line);
    else
        return (SciPosition)Partitioning_Int_PositionFromPartition(
            &self->startsUTF16.starts, (int)line);
}

static SciLine LV32_LineFromPositionIndex(const ILineVector *ilv, SciPosition pos,
                                           LineCharacterIndexType lci) {
    const LineVector32 *self = (const LineVector32 *)ilv;
    if (lci == LineCharacterIndexType_Utf32)
        return (SciLine)Partitioning_Int_PartitionFromPosition(
            &self->startsUTF32.starts, (int)pos);
    else
        return (SciLine)Partitioning_Int_PartitionFromPosition(
            &self->startsUTF16.starts, (int)pos);
}

/* ═══ LineVector64 (POS = ptrdiff_t, large documents) ════════════════ */

typedef struct {
    const ILineVector_vtbl  *vtbl;       /* MUST be first */
    Partitioning_PD          starts;
    PerLine                 *perLine;
    LineStartIndex_PD        startsUTF16;
    LineStartIndex_PD        startsUTF32;
    LineCharacterIndexType   activeIndices;
} LineVector64;

static void LV64_SetActiveIndices(LineVector64 *self) {
    self->activeIndices =
        (LineCharacterIndexType)(
            (LSI_PD_Active(&self->startsUTF32) ? LineCharacterIndexType_Utf32 : LineCharacterIndexType_None) |
            (LSI_PD_Active(&self->startsUTF16) ? LineCharacterIndexType_Utf16 : LineCharacterIndexType_None));
}

/* Forward declarations */
static void LV64_destroy         (ILineVector *self);
static void LV64_Init            (ILineVector *self);
static void LV64_SetPerLine      (ILineVector *self, PerLine *pl);
static void LV64_InsertText      (ILineVector *self, SciLine line, SciPosition delta);
static void LV64_InsertLine      (ILineVector *self, SciLine line, SciPosition position, int lineStart);
static void LV64_InsertLines     (ILineVector *self, SciLine line, const SciPosition *positions, size_t lines, int lineStart);
static void LV64_SetLineStart    (ILineVector *self, SciLine line, SciPosition position);
static void LV64_RemoveLine      (ILineVector *self, SciLine line);
static SciLine LV64_Lines        (const ILineVector *self);
static void LV64_AllocateLines   (ILineVector *self, SciLine lines);
static SciLine LV64_LineFromPosition(const ILineVector *self, SciPosition pos);
static SciPosition LV64_LineStart(const ILineVector *self, SciLine line);
static void LV64_InsertCharacters(ILineVector *self, SciLine line, CountWidths delta);
static void LV64_SetLineCharactersWidth(ILineVector *self, SciLine line, CountWidths width);
static LineCharacterIndexType LV64_LineCharacterIndex(const ILineVector *self);
static int  LV64_AllocateLineCharacterIndex(ILineVector *self, LineCharacterIndexType lci, SciLine lines);
static int  LV64_ReleaseLineCharacterIndex(ILineVector *self, LineCharacterIndexType lci);
static SciPosition LV64_IndexLineStart(const ILineVector *self, SciLine line, LineCharacterIndexType lci);
static SciLine LV64_LineFromPositionIndex(const ILineVector *self, SciPosition pos, LineCharacterIndexType lci);

static const ILineVector_vtbl lv64_vtbl = {
    LV64_destroy,
    LV64_Init,
    LV64_SetPerLine,
    LV64_InsertText,
    LV64_InsertLine,
    LV64_InsertLines,
    LV64_SetLineStart,
    LV64_RemoveLine,
    LV64_Lines,
    LV64_AllocateLines,
    LV64_LineFromPosition,
    LV64_LineStart,
    LV64_InsertCharacters,
    LV64_SetLineCharactersWidth,
    LV64_LineCharacterIndex,
    LV64_AllocateLineCharacterIndex,
    LV64_ReleaseLineCharacterIndex,
    LV64_IndexLineStart,
    LV64_LineFromPositionIndex,
};

static ILineVector *LineVector64_create(void) {
    LineVector64 *lv = (LineVector64 *)calloc(1, sizeof(LineVector64));
    assert(lv);
    lv->vtbl = &lv64_vtbl;
    Partitioning_PD_init(&lv->starts);
    Partitioning_PD_ReAllocate(&lv->starts, 256);
    lv->perLine = NULL;
    LSI_PD_init(&lv->startsUTF16);
    LSI_PD_init(&lv->startsUTF32);
    lv->activeIndices = LineCharacterIndexType_None;
    return (ILineVector *)lv;
}

static void LV64_destroy(ILineVector *ilv) {
    LineVector64 *self = (LineVector64 *)ilv;
    Partitioning_PD_destroy(&self->starts);
    LSI_PD_destroy(&self->startsUTF16);
    LSI_PD_destroy(&self->startsUTF32);
    free(self);
}

static void LV64_Init(ILineVector *ilv) {
    LineVector64 *self = (LineVector64 *)ilv;
    Partitioning_PD_DeleteAll(&self->starts);
    if (self->perLine)
        self->perLine->vtbl->Init(self->perLine);
    Partitioning_PD_DeleteAll(&self->startsUTF32.starts);
    Partitioning_PD_DeleteAll(&self->startsUTF16.starts);
}

static void LV64_SetPerLine(ILineVector *ilv, PerLine *pl) {
    ((LineVector64 *)ilv)->perLine = pl;
}

static void LV64_InsertText(ILineVector *ilv, SciLine line, SciPosition delta) {
    Partitioning_PD_InsertText(&((LineVector64 *)ilv)->starts, (ptrdiff_t)line, (ptrdiff_t)delta);
}

static void LV64_InsertLine(ILineVector *ilv, SciLine line, SciPosition position, int lineStart) {
    LineVector64 *self = (LineVector64 *)ilv;
    Partitioning_PD_InsertPartition(&self->starts, (ptrdiff_t)line, (ptrdiff_t)position);
    if (self->activeIndices != LineCharacterIndexType_None) {
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
            LSI_PD_InsertLines(&self->startsUTF32, line, 1);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
            LSI_PD_InsertLines(&self->startsUTF16, line, 1);
    }
    if (self->perLine) {
        SciLine l = line;
        if ((l > 0) && lineStart)
            l--;
        self->perLine->vtbl->InsertLine(self->perLine, l);
    }
}

static void LV64_InsertLines(ILineVector *ilv, SciLine line, const SciPosition *positions,
                               size_t lines, int lineStart) {
    LineVector64 *self = (LineVector64 *)ilv;
    /* POS=ptrdiff_t == SciPosition → can use InsertPartitions directly */
    Partitioning_PD_InsertPartitions(&self->starts, (ptrdiff_t)line, positions, lines);
    if (self->activeIndices != LineCharacterIndexType_None) {
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
            LSI_PD_InsertLines(&self->startsUTF32, line, (SciLine)lines);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
            LSI_PD_InsertLines(&self->startsUTF16, line, (SciLine)lines);
    }
    if (self->perLine) {
        SciLine l = line;
        if ((l > 0) && lineStart)
            l--;
        self->perLine->vtbl->InsertLines(self->perLine, l, (SciLine)lines);
    }
}

static void LV64_SetLineStart(ILineVector *ilv, SciLine line, SciPosition position) {
    Partitioning_PD_SetPartitionStartPosition(&((LineVector64 *)ilv)->starts,
                                               (ptrdiff_t)line, (ptrdiff_t)position);
}

static void LV64_RemoveLine(ILineVector *ilv, SciLine line) {
    LineVector64 *self = (LineVector64 *)ilv;
    Partitioning_PD_RemovePartition(&self->starts, (ptrdiff_t)line);
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
        Partitioning_PD_RemovePartition(&self->startsUTF32.starts, (ptrdiff_t)line);
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
        Partitioning_PD_RemovePartition(&self->startsUTF16.starts, (ptrdiff_t)line);
    if (self->perLine)
        self->perLine->vtbl->RemoveLine(self->perLine, line);
}

static SciLine LV64_Lines(const ILineVector *ilv) {
    return (SciLine)Partitioning_PD_Partitions(&((const LineVector64 *)ilv)->starts);
}

static void LV64_AllocateLines(ILineVector *ilv, SciLine lines) {
    LineVector64 *self = (LineVector64 *)ilv;
    if (lines > LV64_Lines(ilv)) {
        Partitioning_PD_ReAllocate(&self->starts, (ptrdiff_t)lines);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
            LSI_PD_AllocateLines(&self->startsUTF32, lines);
        if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
            LSI_PD_AllocateLines(&self->startsUTF16, lines);
    }
}

static SciLine LV64_LineFromPosition(const ILineVector *ilv, SciPosition pos) {
    return (SciLine)Partitioning_PD_PartitionFromPosition(
        &((const LineVector64 *)ilv)->starts, (ptrdiff_t)pos);
}

static SciPosition LV64_LineStart(const ILineVector *ilv, SciLine line) {
    return (SciPosition)Partitioning_PD_PositionFromPartition(
        &((const LineVector64 *)ilv)->starts, (ptrdiff_t)line);
}

static void LV64_InsertCharacters(ILineVector *ilv, SciLine line, CountWidths delta) {
    LineVector64 *self = (LineVector64 *)ilv;
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32))
        Partitioning_PD_InsertText(&self->startsUTF32.starts, (ptrdiff_t)line,
                                    CountWidths_WidthUTF32(&delta));
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16))
        Partitioning_PD_InsertText(&self->startsUTF16.starts, (ptrdiff_t)line,
                                    CountWidths_WidthUTF16(&delta));
}

static void LV64_SetLineCharactersWidth(ILineVector *ilv, SciLine line, CountWidths width) {
    LineVector64 *self = (LineVector64 *)ilv;
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf32)) {
        assert(Partitioning_PD_Partitions(&self->startsUTF32.starts) ==
               Partitioning_PD_Partitions(&self->starts));
        LSI_PD_SetLineWidth(&self->startsUTF32, line, CountWidths_WidthUTF32(&width));
    }
    if (SCI_FLAG_SET(self->activeIndices, LineCharacterIndexType_Utf16)) {
        assert(Partitioning_PD_Partitions(&self->startsUTF16.starts) ==
               Partitioning_PD_Partitions(&self->starts));
        LSI_PD_SetLineWidth(&self->startsUTF16, line, CountWidths_WidthUTF16(&width));
    }
}

static LineCharacterIndexType LV64_LineCharacterIndex(const ILineVector *ilv) {
    return ((const LineVector64 *)ilv)->activeIndices;
}

static int LV64_AllocateLineCharacterIndex(ILineVector *ilv, LineCharacterIndexType lci,
                                             SciLine lines) {
    LineVector64 *self = (LineVector64 *)ilv;
    const LineCharacterIndexType before = self->activeIndices;
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf32)) {
        LSI_PD_Allocate(&self->startsUTF32, lines);
        assert(Partitioning_PD_Partitions(&self->startsUTF32.starts) ==
               Partitioning_PD_Partitions(&self->starts));
    }
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf16)) {
        LSI_PD_Allocate(&self->startsUTF16, lines);
        assert(Partitioning_PD_Partitions(&self->startsUTF16.starts) ==
               Partitioning_PD_Partitions(&self->starts));
    }
    LV64_SetActiveIndices(self);
    return before != self->activeIndices;
}

static int LV64_ReleaseLineCharacterIndex(ILineVector *ilv, LineCharacterIndexType lci) {
    LineVector64 *self = (LineVector64 *)ilv;
    const LineCharacterIndexType before = self->activeIndices;
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf32))
        LSI_PD_Release(&self->startsUTF32);
    if (SCI_FLAG_SET(lci, LineCharacterIndexType_Utf16))
        LSI_PD_Release(&self->startsUTF16);
    LV64_SetActiveIndices(self);
    return before != self->activeIndices;
}

static SciPosition LV64_IndexLineStart(const ILineVector *ilv, SciLine line,
                                        LineCharacterIndexType lci) {
    const LineVector64 *self = (const LineVector64 *)ilv;
    if (lci == LineCharacterIndexType_Utf32)
        return (SciPosition)Partitioning_PD_PositionFromPartition(
            &self->startsUTF32.starts, (ptrdiff_t)line);
    else
        return (SciPosition)Partitioning_PD_PositionFromPartition(
            &self->startsUTF16.starts, (ptrdiff_t)line);
}

static SciLine LV64_LineFromPositionIndex(const ILineVector *ilv, SciPosition pos,
                                           LineCharacterIndexType lci) {
    const LineVector64 *self = (const LineVector64 *)ilv;
    if (lci == LineCharacterIndexType_Utf32)
        return (SciLine)Partitioning_PD_PartitionFromPosition(
            &self->startsUTF32.starts, (ptrdiff_t)pos);
    else
        return (SciLine)Partitioning_PD_PartitionFromPosition(
            &self->startsUTF16.starts, (ptrdiff_t)pos);
}

/* ────────────────────────────────────────────────────────────────────
 * 3. CellBuffer implementation
 * ────────────────────────────────────────────────────────────────────
 */

/* ── Internal helpers (private) ─────────────────────────────────────── */

static int CB_UTF8LineEndOverlaps(const CellBuffer *self, SciPosition position) {
    const unsigned char bytes[4] = {
        (unsigned char)SplitVector_Char_ValueAt(&self->substance, position - 2),
        (unsigned char)SplitVector_Char_ValueAt(&self->substance, position - 1),
        (unsigned char)SplitVector_Char_ValueAt(&self->substance, position),
        (unsigned char)SplitVector_Char_ValueAt(&self->substance, position + 1),
    };
    return UTF8IsSeparator(bytes) || UTF8IsSeparator(bytes + 1) || UTF8IsNEL(bytes + 1);
}

static int CB_UTF8IsCharacterBoundary(const CellBuffer *self, SciPosition position) {
    assert(position >= 0 && position <= SplitVector_Char_Length(&self->substance));
    if (position > 0) {
        char back[UTF8MaxBytes + 1];
        int backLen = 0;
        for (int i = 0; i < UTF8MaxBytes; i++) {
            const SciPosition posBack = position - i;
            if (posBack < 0)
                return 0;
            /* prepend byte */
            memmove(back + 1, back, (size_t)backLen);
            back[0] = SplitVector_Char_ValueAt(&self->substance, posBack);
            backLen++;
            if (!UTF8IsTrailByte((unsigned char)back[0])) {
                if (i > 0) {
                    const int cla = UTF8Classify((const unsigned char *)back, (size_t)backLen);
                    if ((cla & UTF8MaskInvalid) || (cla != i))
                        return 0;
                }
                break;
            }
        }
    }
    if (position < SplitVector_Char_Length(&self->substance)) {
        const unsigned char fore = (unsigned char)SplitVector_Char_ValueAt(
            &self->substance, position);
        if (UTF8IsTrailByte(fore))
            return 0;
    }
    return 1;
}

static int CB_MaintainingLineCharacterIndex(const CellBuffer *self) {
    return self->plv->vtbl->LineCharacterIndex(self->plv) != LineCharacterIndexType_None;
}

static void CB_ResetLineEnds(CellBuffer *self);
static void CB_RecalculateIndexLineStarts(CellBuffer *self, SciLine lineFirst, SciLine lineLast);
static void CB_BasicInsertString(CellBuffer *self, SciPosition position, const char *s,
                                  SciPosition insertLength);
static void CB_BasicDeleteChars(CellBuffer *self, SciPosition position,
                                 SciPosition deleteLength);

/* ── CellBuffer_init / destroy ────────────────────────────────────── */

void CellBuffer_init(CellBuffer *self, int hasStyles, int largeDocument) {
    self->hasStyles      = hasStyles;
    self->largeDocument  = largeDocument;
    self->readOnly       = 0;
    self->utf8Substance  = 0;
    self->utf8LineEnds   = LineEndType_Default;
    self->collectingUndo = 1;
    SplitVector_Char_init(&self->substance);
    SplitVector_Char_init(&self->style);
    self->uh             = UndoHistory_create();
    self->changeHistory  = NULL;
    if (largeDocument)
        self->plv = LineVector64_create();
    else
        self->plv = LineVector32_create();
}

void CellBuffer_destroy(CellBuffer *self) {
    self->plv->vtbl->destroy(self->plv);
    self->plv = NULL;
    if (self->changeHistory) {
        ChangeHistory_destroy(self->changeHistory);
        self->changeHistory = NULL;
    }
    if (self->uh) {
        UndoHistory_destroy(self->uh);
        self->uh = NULL;
    }
    SplitVector_Char_destroy(&self->substance);
    SplitVector_Char_destroy(&self->style);
}

/* ── Text access ──────────────────────────────────────────────────── */

char CellBuffer_CharAt(const CellBuffer *self, SciPosition position) {
    return SplitVector_Char_ValueAt(&self->substance, position);
}

unsigned char CellBuffer_UCharAt(const CellBuffer *self, SciPosition position) {
    return (unsigned char)SplitVector_Char_ValueAt(&self->substance, position);
}

void CellBuffer_GetCharRange(const CellBuffer *self, char *buffer,
                              SciPosition position, SciPosition lengthRetrieve) {
    if (lengthRetrieve <= 0) return;
    if (position < 0)         return;
    if ((position + lengthRetrieve) > SplitVector_Char_Length(&self->substance)) {
        fprintf(stderr, "Bad GetCharRange %.0f for %.0f of %.0f\n",
                (double)position, (double)lengthRetrieve,
                (double)SplitVector_Char_Length(&self->substance));
        return;
    }
    SplitVector_Char_GetRange(&self->substance, buffer, position, lengthRetrieve);
}

char CellBuffer_StyleAt(const CellBuffer *self, SciPosition position) {
    return self->hasStyles ? SplitVector_Char_ValueAt(&self->style, position) : '\0';
}

void CellBuffer_GetStyleRange(const CellBuffer *self, unsigned char *buffer,
                               SciPosition position, SciPosition lengthRetrieve) {
    if (lengthRetrieve < 0) return;
    if (position < 0)       return;
    if (!self->hasStyles) {
        memset(buffer, 0, (size_t)lengthRetrieve);
        return;
    }
    if ((position + lengthRetrieve) > SplitVector_Char_Length(&self->style)) {
        fprintf(stderr, "Bad GetStyleRange %.0f for %.0f of %.0f\n",
                (double)position, (double)lengthRetrieve,
                (double)SplitVector_Char_Length(&self->style));
        return;
    }
    SplitVector_Char_GetRange(&self->style, (char *)buffer, position, lengthRetrieve);
}

const char *CellBuffer_BufferPointer(CellBuffer *self) {
    return SplitVector_Char_BufferPointer(&self->substance);
}

const char *CellBuffer_RangePointer(CellBuffer *self, SciPosition position,
                                     SciPosition rangeLength) {
    return SplitVector_Char_RangePointer(&self->substance, position, rangeLength);
}

SciPosition CellBuffer_GapPosition(const CellBuffer *self) {
    return SplitVector_Char_GapPosition(&self->substance);
}

SplitView CellBuffer_AllView(const CellBuffer *self) {
    const size_t length  = (size_t)SplitVector_Char_Length(&self->substance);
    size_t length1 = (size_t)SplitVector_Char_GapPosition(&self->substance);
    if (length1 == 0) {
        length1 = length;   /* avoid useless test for 0 length1 */
    }
    SplitView sv;
    sv.segment1 = SplitVector_Char_ElementPointer(&self->substance, 0);
    sv.length1  = length1;
    sv.segment2 = SplitVector_Char_ElementPointer(&self->substance, (ptrdiff_t)length1) - length1;
    sv.length   = length;
    return sv;
}

/* ── Length / allocate / config ───────────────────────────────────── */

SciPosition CellBuffer_Length(const CellBuffer *self) {
    return SplitVector_Char_Length(&self->substance);
}

void CellBuffer_Allocate(CellBuffer *self, SciPosition newSize) {
    if (!self->largeDocument && (newSize > INT32_MAX)) {
        assert(!"CellBuffer_Allocate: size of standard document limited to 2G.");
        abort();
    }
    SplitVector_Char_ReAllocate(&self->substance, (size_t)newSize);
    if (self->hasStyles)
        SplitVector_Char_ReAllocate(&self->style, (size_t)newSize);
}

void CellBuffer_SetUTF8Substance(CellBuffer *self, int utf8Substance) {
    self->utf8Substance = utf8Substance;
}

LineEndType CellBuffer_GetLineEndTypes(const CellBuffer *self) {
    return self->utf8LineEnds;
}

void CellBuffer_SetLineEndTypes(CellBuffer *self, LineEndType utf8LineEnds) {
    if (self->utf8LineEnds != utf8LineEnds) {
        const LineCharacterIndexType indexes = self->plv->vtbl->LineCharacterIndex(self->plv);
        self->utf8LineEnds = utf8LineEnds;
        CB_ResetLineEnds(self);
        CellBuffer_AllocateLineCharacterIndex(self, indexes);
    }
}

int CellBuffer_ContainsLineEnd(const CellBuffer *self, const char *s, SciPosition length) {
    unsigned char chBeforePrev = 0;
    unsigned char chPrev = 0;
    for (SciPosition i = 0; i < length; i++) {
        const unsigned char ch = (unsigned char)s[i];
        if ((ch == '\r') || (ch == '\n'))
            return 1;
        if (self->utf8LineEnds == LineEndType_Unicode) {
            if (UTF8IsMultibyteLineEnd(chBeforePrev, chPrev, ch))
                return 1;
        }
        chBeforePrev = chPrev;
        chPrev = ch;
    }
    return 0;
}

void CellBuffer_SetPerLine(CellBuffer *self, PerLine *pl) {
    self->plv->vtbl->SetPerLine(self->plv, pl);
}

/* ── Line character index ─────────────────────────────────────────── */

LineCharacterIndexType CellBuffer_LineCharacterIndex(const CellBuffer *self) {
    return self->plv->vtbl->LineCharacterIndex(self->plv);
}

void CellBuffer_AllocateLineCharacterIndex(CellBuffer *self, LineCharacterIndexType lci) {
    if (self->utf8Substance) {
        if (self->plv->vtbl->AllocateLineCharacterIndex(self->plv, lci,
                                                         CellBuffer_Lines(self))) {
            CB_RecalculateIndexLineStarts(self, 0, CellBuffer_Lines(self) - 1);
        }
    }
}

void CellBuffer_ReleaseLineCharacterIndex(CellBuffer *self, LineCharacterIndexType lci) {
    self->plv->vtbl->ReleaseLineCharacterIndex(self->plv, lci);
}

/* ── Line navigation ──────────────────────────────────────────────── */

SciLine CellBuffer_Lines(const CellBuffer *self) {
    return self->plv->vtbl->Lines(self->plv);
}

void CellBuffer_AllocateLines(CellBuffer *self, SciLine lines) {
    self->plv->vtbl->AllocateLines(self->plv, lines);
}

SciPosition CellBuffer_LineStart(const CellBuffer *self, SciLine line) {
    if (line < 0)
        return 0;
    if (line >= CellBuffer_Lines(self))
        return CellBuffer_Length(self);
    return self->plv->vtbl->LineStart(self->plv, line);
}

SciPosition CellBuffer_LineEnd(const CellBuffer *self, SciLine line) {
    if (line >= CellBuffer_Lines(self) - 1) {
        return CellBuffer_LineStart(self, line + 1);
    } else {
        SciPosition position = CellBuffer_LineStart(self, line + 1);
        if (self->utf8LineEnds == LineEndType_Unicode) {
            const unsigned char bytes[3] = {
                CellBuffer_UCharAt(self, position - 3),
                CellBuffer_UCharAt(self, position - 2),
                CellBuffer_UCharAt(self, position - 1),
            };
            if (UTF8IsSeparator(bytes))
                return position - UTF8SeparatorLength;
            if (UTF8IsNEL(bytes + 1))
                return position - UTF8NELLength;
        }
        position--;   /* back over CR or LF */
        /* When line terminator is CR+LF, may need to go back one more */
        if ((position > CellBuffer_LineStart(self, line)) &&
            (CellBuffer_CharAt(self, position - 1) == '\r'))
            position--;
        return position;
    }
}

SciLine CellBuffer_LineFromPosition(const CellBuffer *self, SciPosition pos) {
    return self->plv->vtbl->LineFromPosition(self->plv, pos);
}

SciPosition CellBuffer_IndexLineStart(const CellBuffer *self, SciLine line,
                                       LineCharacterIndexType lci) {
    return self->plv->vtbl->IndexLineStart(self->plv, line, lci);
}

SciLine CellBuffer_LineFromPositionIndex(const CellBuffer *self, SciPosition pos,
                                          LineCharacterIndexType lci) {
    return self->plv->vtbl->LineFromPositionIndex(self->plv, pos, lci);
}

/* ── Flags ────────────────────────────────────────────────────────── */

int CellBuffer_IsReadOnly(const CellBuffer *self)  { return self->readOnly; }
void CellBuffer_SetReadOnly(CellBuffer *self, int set) { self->readOnly = set; }
int CellBuffer_IsLarge(const CellBuffer *self)     { return self->largeDocument; }
int CellBuffer_HasStyles(const CellBuffer *self)   { return self->hasStyles; }

/* ── Save point ───────────────────────────────────────────────────── */

void CellBuffer_SetSavePoint(CellBuffer *self) {
    UndoHistory_SetSavePoint(self->uh);
    if (self->changeHistory)
        ChangeHistory_SetSavePoint(self->changeHistory);
}

int CellBuffer_IsSavePoint(const CellBuffer *self) {
    return UndoHistory_IsSavePoint(self->uh);
}

/* ── Tentative ────────────────────────────────────────────────────── */

void CellBuffer_TentativeStart(CellBuffer *self)  { UndoHistory_TentativeStart(self->uh); }
void CellBuffer_TentativeCommit(CellBuffer *self) { UndoHistory_TentativeCommit(self->uh); }
int  CellBuffer_TentativeActive(const CellBuffer *self) { return UndoHistory_TentativeActive(self->uh); }
int  CellBuffer_TentativeSteps(CellBuffer *self)  { return UndoHistory_TentativeSteps(self->uh); }

/* ── Line insert / remove (no undo) ──────────────────────────────── */

void CellBuffer_InsertLine(CellBuffer *self, SciLine line, SciPosition position, int lineStart) {
    self->plv->vtbl->InsertLine(self->plv, line, position, lineStart);
}

void CellBuffer_RemoveLine(CellBuffer *self, SciLine line) {
    self->plv->vtbl->RemoveLine(self->plv, line);
}

/* ── Style mutation ───────────────────────────────────────────────── */

int CellBuffer_SetStyleAt(CellBuffer *self, SciPosition position, char styleValue) {
    if (!self->hasStyles) return 0;
    const char curVal = SplitVector_Char_ValueAt(&self->style, position);
    if (curVal != styleValue) {
        SplitVector_Char_SetValueAt(&self->style, position, styleValue);
        return 1;
    }
    return 0;
}

int CellBuffer_SetStyleFor(CellBuffer *self, SciPosition position,
                            SciPosition lengthStyle, char styleValue) {
    if (!self->hasStyles) return 0;
    int changed = 0;
    assert(lengthStyle == 0 ||
           (lengthStyle > 0 && lengthStyle + position <= SplitVector_Char_Length(&self->style)));
    while (lengthStyle--) {
        const char curVal = SplitVector_Char_ValueAt(&self->style, position);
        if (curVal != styleValue) {
            SplitVector_Char_SetValueAt(&self->style, position, styleValue);
            changed = 1;
        }
        position++;
    }
    return changed;
}

/* ── Text mutation ────────────────────────────────────────────────── */

const char *CellBuffer_InsertString(CellBuffer *self, SciPosition position,
                                     const char *s, SciPosition insertLength,
                                     int *startSequence) {
    const char *data = s;
    if (!self->readOnly) {
        if (self->collectingUndo) {
            data = UndoHistory_AppendAction(self->uh, ActionType_insert, position,
                                             s, insertLength, startSequence, 1);
        }
        CB_BasicInsertString(self, position, s, insertLength);
        if (self->changeHistory) {
            ChangeHistory_Insert(self->changeHistory, position, insertLength,
                                  self->collectingUndo,
                                  UndoHistory_BeforeReachableSavePoint(self->uh));
        }
    }
    return data;
}

const char *CellBuffer_DeleteChars(CellBuffer *self, SciPosition position,
                                    SciPosition deleteLength, int *startSequence) {
    assert(deleteLength > 0);
    const char *data = NULL;
    if (!self->readOnly) {
        if (self->collectingUndo) {
            data = SplitVector_Char_RangePointer(&self->substance, position, deleteLength);
            data = UndoHistory_AppendAction(self->uh, ActionType_remove, position,
                                             data, deleteLength, startSequence, 1);
        }
        if (self->changeHistory) {
            ChangeHistory_DeleteRangeSavingHistory(
                self->changeHistory, position, deleteLength,
                UndoHistory_BeforeReachableSavePoint(self->uh),
                UndoHistory_AfterOrAtDetachPoint(self->uh));
        }
        CB_BasicDeleteChars(self, position, deleteLength);
    }
    return data;
}

/* ── Undo collection ──────────────────────────────────────────────── */

int CellBuffer_SetUndoCollection(CellBuffer *self, int collectUndo) {
    self->collectingUndo = collectUndo;
    UndoHistory_DropUndoSequence(self->uh);
    return self->collectingUndo;
}

int CellBuffer_IsCollectingUndo(const CellBuffer *self) {
    return self->collectingUndo;
}

void CellBuffer_BeginUndoAction(CellBuffer *self, int mayCoalesce) {
    UndoHistory_BeginUndoAction(self->uh, mayCoalesce);
}

void CellBuffer_EndUndoAction(CellBuffer *self) {
    UndoHistory_EndUndoAction(self->uh);
}

int CellBuffer_UndoSequenceDepth(const CellBuffer *self) {
    return UndoHistory_UndoSequenceDepth(self->uh);
}

int CellBuffer_AfterUndoSequenceStart(const CellBuffer *self) {
    return UndoHistory_AfterUndoSequenceStart(self->uh);
}

void CellBuffer_AddUndoAction(CellBuffer *self, SciPosition token, int mayCoalesce) {
    int startSequence = 0;
    UndoHistory_AppendAction(self->uh, ActionType_container, token, NULL, 0,
                              &startSequence, mayCoalesce);
}

void CellBuffer_DeleteUndoHistory(CellBuffer *self) {
    UndoHistory_DeleteUndoHistory(self->uh);
}

/* ── Undo / redo ──────────────────────────────────────────────────── */

int CellBuffer_CanUndo(const CellBuffer *self)   { return UndoHistory_CanUndo(self->uh); }
int CellBuffer_StartUndo(CellBuffer *self)       { return UndoHistory_StartUndo(self->uh); }
Action CellBuffer_GetUndoStep(const CellBuffer *self) { return UndoHistory_GetUndoStep(self->uh); }

void CellBuffer_PerformUndoStep(CellBuffer *self) {
    const Action previousStep = UndoHistory_GetUndoStep(self->uh);
    if (self->changeHistory && UndoHistory_PreviousBeforeSavePoint(self->uh))
        ChangeHistory_StartReversion(self->changeHistory);
    if (previousStep.at == ActionType_insert) {
        if (CellBuffer_Length(self) < previousStep.lenData) {
            assert(!"CellBuffer_PerformUndoStep: deletion must be <= document length.");
            abort();
        }
        if (self->changeHistory) {
            ChangeHistory_DeleteRange(self->changeHistory, previousStep.position,
                previousStep.lenData,
                UndoHistory_PreviousBeforeSavePoint(self->uh) &&
                !UndoHistory_AfterDetachPoint(self->uh));
        }
        CB_BasicDeleteChars(self, previousStep.position, previousStep.lenData);
    } else if (previousStep.at == ActionType_remove) {
        CB_BasicInsertString(self, previousStep.position, previousStep.data,
                              previousStep.lenData);
        if (self->changeHistory) {
            ChangeHistory_UndoDeleteStep(self->changeHistory, previousStep.position,
                previousStep.lenData, UndoHistory_AfterDetachPoint(self->uh));
        }
    }
    UndoHistory_CompletedUndoStep(self->uh);
}

int CellBuffer_CanRedo(const CellBuffer *self)   { return UndoHistory_CanRedo(self->uh); }
int CellBuffer_StartRedo(CellBuffer *self)       { return UndoHistory_StartRedo(self->uh); }
Action CellBuffer_GetRedoStep(const CellBuffer *self) { return UndoHistory_GetRedoStep(self->uh); }

void CellBuffer_PerformRedoStep(CellBuffer *self) {
    const Action actionStep = UndoHistory_GetRedoStep(self->uh);
    if (actionStep.at == ActionType_insert) {
        CB_BasicInsertString(self, actionStep.position, actionStep.data, actionStep.lenData);
        if (self->changeHistory) {
            ChangeHistory_Insert(self->changeHistory, actionStep.position,
                actionStep.lenData, self->collectingUndo,
                UndoHistory_BeforeSavePoint(self->uh) &&
                !UndoHistory_AfterOrAtDetachPoint(self->uh));
        }
    } else if (actionStep.at == ActionType_remove) {
        if (self->changeHistory) {
            ChangeHistory_DeleteRangeSavingHistory(self->changeHistory,
                actionStep.position, actionStep.lenData,
                UndoHistory_BeforeReachableSavePoint(self->uh),
                UndoHistory_AfterOrAtDetachPoint(self->uh));
        }
        CB_BasicDeleteChars(self, actionStep.position, actionStep.lenData);
    }
    if (self->changeHistory && UndoHistory_AfterSavePoint(self->uh))
        ChangeHistory_EndReversion(self->changeHistory);
    UndoHistory_CompletedRedoStep(self->uh);
}

/* ── Undo action inspection ───────────────────────────────────────── */

int          CellBuffer_UndoActions(const CellBuffer *self)        { return UndoHistory_Actions(self->uh); }
void         CellBuffer_SetUndoSavePoint(CellBuffer *self, int a)  { UndoHistory_SetSavePointAction(self->uh, a); }
int          CellBuffer_UndoSavePoint(const CellBuffer *self)      { return UndoHistory_SavePoint(self->uh); }
void         CellBuffer_SetUndoDetach(CellBuffer *self, int a)     { UndoHistory_SetDetachPoint(self->uh, a); }
int          CellBuffer_UndoDetach(const CellBuffer *self)         { return UndoHistory_DetachPoint(self->uh); }
void         CellBuffer_SetUndoTentative(CellBuffer *self, int a)  { UndoHistory_SetTentative(self->uh, a); }
int          CellBuffer_UndoTentative(const CellBuffer *self)      { return UndoHistory_TentativePoint(self->uh); }
int          CellBuffer_UndoCurrent(const CellBuffer *self)        { return UndoHistory_Current(self->uh); }
int          CellBuffer_UndoActionType(const CellBuffer *self, int a)     { return UndoHistory_Type(self->uh, a); }
SciPosition  CellBuffer_UndoActionPosition(const CellBuffer *self, int a) { return UndoHistory_Position(self->uh, a); }

SciStringView CellBuffer_UndoActionText(const CellBuffer *self, int action) {
    return UndoHistory_Text(self->uh, action);
}

void CellBuffer_PushUndoActionType(CellBuffer *self, int type, SciPosition position) {
    UndoHistory_PushUndoActionType(self->uh, type, position);
}

void CellBuffer_ChangeLastUndoActionText(CellBuffer *self, size_t length, const char *text) {
    UndoHistory_ChangeLastUndoActionText(self->uh, length, text);
}

void CellBuffer_SetUndoCurrent(CellBuffer *self, int action) {
    UndoHistory_SetCurrent(self->uh, action, CellBuffer_Length(self));
    if (self->changeHistory) {
        if ((UndoHistory_DetachPoint(self->uh) >= 0) &&
            (UndoHistory_SavePoint(self->uh) >= 0)) {
            UndoHistory_DeleteUndoHistory(self->uh);
            ChangeHistory_destroy(self->changeHistory);
            self->changeHistory = NULL;
            assert(!"SetUndoCurrent: invalid undo history.");
            abort();
        }
        const intptr_t sizeChange   = UndoHistory_Delta(self->uh, action);
        const intptr_t lengthOriginal = CellBuffer_Length(self) - sizeChange;
        /* Recreate change history */
        ChangeHistory_destroy(self->changeHistory);
        self->changeHistory = ChangeHistory_create((SciPosition)lengthOriginal);
        /* Replay history into changeHistory */
        {
            const int savePoint   = UndoHistory_SavePoint(self->uh);
            const int detachPoint = UndoHistory_DetachPoint(self->uh);
            const int acts        = UndoHistory_Actions(self->uh);
            /* Forward pass */
            for (int act = 0; act < acts; act++) {
                const int rawType = UndoHistory_Type(self->uh, act);
                /* coalesceFlag = 0x100 from original */
                const ActionType type = (ActionType)(rawType & ~0x100);
                const SciPosition pos = UndoHistory_Position(self->uh, act);
                const SciPosition len = UndoHistory_Length(self->uh, act);
                const int beforeSave  = (act < savePoint) ||
                    ((detachPoint >= 0) && (detachPoint > act));
                const int afterDetach = (detachPoint >= 0) && (detachPoint < act);
                if (type == ActionType_insert)
                    ChangeHistory_Insert(self->changeHistory, pos, len, 1, beforeSave);
                else if (type == ActionType_remove)
                    ChangeHistory_DeleteRangeSavingHistory(self->changeHistory,
                        pos, len, beforeSave, afterDetach);
                ChangeHistory_Check(self->changeHistory);
            }
            /* Reverse pass: undo back to currentPoint */
            const int currentPoint = UndoHistory_Current(self->uh);
            for (int act = acts - 1; act >= currentPoint; act--) {
                const int rawType = UndoHistory_Type(self->uh, act);
                const ActionType type = (ActionType)(rawType & ~0x100);
                const SciPosition pos = UndoHistory_Position(self->uh, act);
                const SciPosition len = UndoHistory_Length(self->uh, act);
                const int beforeSave  = (act < savePoint);
                const int afterDetach = (detachPoint >= 0) && (detachPoint < act);
                if (beforeSave)
                    ChangeHistory_StartReversion(self->changeHistory);
                if (type == ActionType_insert)
                    ChangeHistory_DeleteRange(self->changeHistory, pos, len,
                        beforeSave && !afterDetach);
                else if (type == ActionType_remove)
                    ChangeHistory_UndoDeleteStep(self->changeHistory, pos, len, afterDetach);
                ChangeHistory_Check(self->changeHistory);
            }
        }
        if (CellBuffer_Length(self) != ChangeHistory_Length(self->changeHistory)) {
            UndoHistory_DeleteUndoHistory(self->uh);
            ChangeHistory_destroy(self->changeHistory);
            self->changeHistory = NULL;
            assert(!"SetUndoCurrent: invalid undo history (length mismatch).");
            abort();
        }
    }
}

/* ── Change history ───────────────────────────────────────────────── */

void CellBuffer_ChangeHistorySet(CellBuffer *self, int set) {
    if (set) {
        if (!self->changeHistory && !UndoHistory_CanUndo(self->uh)) {
            self->changeHistory = ChangeHistory_create(CellBuffer_Length(self));
        }
    } else {
        if (self->changeHistory) {
            ChangeHistory_destroy(self->changeHistory);
            self->changeHistory = NULL;
        }
    }
}

int CellBuffer_EditionAt(const CellBuffer *self, SciPosition pos) {
    return self->changeHistory ? ChangeHistory_EditionAt(self->changeHistory, pos) : 0;
}

SciPosition CellBuffer_EditionEndRun(const CellBuffer *self, SciPosition pos) {
    return self->changeHistory ?
        ChangeHistory_EditionEndRun(self->changeHistory, pos) : CellBuffer_Length(self);
}

unsigned int CellBuffer_EditionDeletesAt(const CellBuffer *self, SciPosition pos) {
    return self->changeHistory ?
        ChangeHistory_EditionDeletesAt(self->changeHistory, pos) : 0;
}

SciPosition CellBuffer_EditionNextDelete(const CellBuffer *self, SciPosition pos) {
    return self->changeHistory ?
        ChangeHistory_EditionNextDelete(self->changeHistory, pos) : CellBuffer_Length(self) + 1;
}

/* ── Private helpers (deferred definitions) ──────────────────────── */

static void CB_RecalculateIndexLineStarts(CellBuffer *self, SciLine lineFirst,
                                           SciLine lineLast) {
    SciPosition posLineEnd = CellBuffer_LineStart(self, lineFirst);
    for (SciLine line = lineFirst; line <= lineLast; line++) {
        const SciPosition posLineStart = posLineEnd;
        posLineEnd = CellBuffer_LineStart(self, line + 1);
        const SciPosition width = posLineEnd - posLineStart;
        char *text = (char *)malloc((size_t)(width + 1));
        assert(text);
        CellBuffer_GetCharRange(self, text, posLineStart, width);
        const CountWidths cw = CountCharacterWidthsUTF8(text, (size_t)width);
        free(text);
        self->plv->vtbl->SetLineCharactersWidth(self->plv, line, cw);
    }
}

static void CB_ResetLineEnds(CellBuffer *self) {
    const SciLine lines = self->plv->vtbl->Lines(self->plv);
    self->plv->vtbl->Init(self->plv);
    self->plv->vtbl->AllocateLines(self->plv, lines);

    const SciPosition position = 0;
    const SciPosition length   = CellBuffer_Length(self);
    self->plv->vtbl->InsertText(self->plv, 0, length);
    SciLine lineInsert = 1;
    const int atLineStart = 1;
    unsigned char chBeforePrev = 0;
    unsigned char chPrev = 0;
    for (SciPosition i = 0; i < length; i++) {
        const unsigned char ch =
            (unsigned char)SplitVector_Char_ValueAt(&self->substance, position + i);
        if (ch == '\r') {
            CellBuffer_InsertLine(self, lineInsert, (position + i) + 1, atLineStart);
            lineInsert++;
        } else if (ch == '\n') {
            if (chPrev == '\r') {
                self->plv->vtbl->SetLineStart(self->plv, lineInsert - 1, (position + i) + 1);
            } else {
                CellBuffer_InsertLine(self, lineInsert, (position + i) + 1, atLineStart);
                lineInsert++;
            }
        } else if (self->utf8LineEnds == LineEndType_Unicode) {
            if (UTF8IsMultibyteLineEnd(chBeforePrev, chPrev, ch)) {
                CellBuffer_InsertLine(self, lineInsert, (position + i) + 1, atLineStart);
                lineInsert++;
            }
        }
        chBeforePrev = chPrev;
        chPrev = ch;
    }
}

static void CB_BasicInsertString(CellBuffer *self, SciPosition position, const char *s,
                                   SciPosition insertLength) {
    if (insertLength == 0) return;
    assert(insertLength > 0);

    const unsigned char chAfter = (unsigned char)SplitVector_Char_ValueAt(
        &self->substance, position);
    int breakingUTF8LineEnd = 0;
    if (self->utf8LineEnds == LineEndType_Unicode && UTF8IsTrailByte(chAfter)) {
        breakingUTF8LineEnd = CB_UTF8LineEndOverlaps(self, position);
    }

    const SciLine linePosition = self->plv->vtbl->LineFromPosition(self->plv, position);
    SciLine lineInsert = linePosition + 1;

    int simpleInsertion = 0;
    const int maintainingIndex = CB_MaintainingLineCharacterIndex(self);

    if (self->utf8Substance && maintainingIndex) {
        simpleInsertion = CB_UTF8IsCharacterBoundary(self, position) &&
            UTF8IsValid(s, (size_t)insertLength);
    }

    SplitVector_Char_InsertFromArray(&self->substance, position, s, 0, insertLength);
    if (self->hasStyles)
        SplitVector_Char_InsertValue(&self->style, position, insertLength, 0);

    const int atLineStart =
        (self->plv->vtbl->LineStart(self->plv, lineInsert - 1) == position);
    self->plv->vtbl->InsertText(self->plv, lineInsert - 1, insertLength);

    unsigned char chBeforePrev =
        (unsigned char)SplitVector_Char_ValueAt(&self->substance, position - 2);
    unsigned char chPrev =
        (unsigned char)SplitVector_Char_ValueAt(&self->substance, position - 1);

    if (chPrev == '\r' && chAfter == '\n') {
        CellBuffer_InsertLine(self, lineInsert, position, 0);
        lineInsert++;
    }
    if (breakingUTF8LineEnd)
        CellBuffer_RemoveLine(self, lineInsert);

    enum { PositionBlockSize = 128 };
    SciPosition positions[PositionBlockSize];
    size_t nPositions = 0;
    const SciLine lineStart = lineInsert;
    memset(positions, 0, sizeof(positions));

    const char *const end = s + insertLength - 1;
    const char *ptr = s;
    unsigned char ch = 0;

    if (chPrev == '\r' && *ptr == '\n') {
        ++ptr;
        self->plv->vtbl->SetLineStart(self->plv, lineInsert - 1,
                                       (position + ptr - s));
        simpleInsertion = 0;
    }

    if (ptr < end) {
        uint8_t eolTable[256];
        memset(eolTable, 0, sizeof(eolTable));
        eolTable[(uint8_t)'\n'] = 1;
        eolTable[(uint8_t)'\r'] = 2;
        if (self->utf8LineEnds == LineEndType_Unicode) {
            eolTable[0x85] = 4;
            eolTable[0xa8] = 3;
            eolTable[0xa9] = 3;
        }

        do {
            ch = (unsigned char)*ptr++;
            uint8_t type;
            while ((type = eolTable[ch]) == 0 && ptr < end) {
                chBeforePrev = chPrev;
                chPrev = ch;
                ch = (unsigned char)*ptr++;
            }
            switch (type) {
            case 2: /* '\r' */
                if ((unsigned char)*ptr == '\n')
                    ++ptr;
                /* fallthrough */
            case 1: /* '\n' */
                positions[nPositions++] = position + (ptrdiff_t)(ptr - s);
                if (nPositions == PositionBlockSize) {
                    self->plv->vtbl->InsertLines(self->plv, lineInsert,
                                                  positions, nPositions, atLineStart);
                    lineInsert += (SciLine)nPositions;
                    nPositions = 0;
                }
                break;
            case 3:
            case 4:
                if ((type == 3 && chPrev == 0x80 && chBeforePrev == 0xe2) ||
                    (type == 4 && chPrev == 0xc2)) {
                    positions[nPositions++] = position + (ptrdiff_t)(ptr - s);
                    if (nPositions == PositionBlockSize) {
                        self->plv->vtbl->InsertLines(self->plv, lineInsert,
                                                      positions, nPositions, atLineStart);
                        lineInsert += (SciLine)nPositions;
                        nPositions = 0;
                    }
                }
                break;
            }
            chBeforePrev = chPrev;
            chPrev = ch;
        } while (ptr < end);
    }

    if (nPositions != 0) {
        self->plv->vtbl->InsertLines(self->plv, lineInsert, positions, nPositions, atLineStart);
        lineInsert += (SciLine)nPositions;
    }

    ch = (unsigned char)*end;
    if (ptr == end) {
        ++ptr;
        if (ch == '\r' || ch == '\n') {
            CellBuffer_InsertLine(self, lineInsert, (position + (ptrdiff_t)(ptr - s)),
                                   atLineStart);
            lineInsert++;
        } else if (self->utf8LineEnds == LineEndType_Unicode && !UTF8IsAscii((char)ch)) {
            if (UTF8IsMultibyteLineEnd(chBeforePrev, chPrev, ch)) {
                CellBuffer_InsertLine(self, lineInsert,
                                       (position + (ptrdiff_t)(ptr - s)), atLineStart);
                lineInsert++;
            }
        }
    }

    if (chAfter == '\n') {
        if (ch == '\r') {
            CellBuffer_RemoveLine(self, lineInsert - 1);
            simpleInsertion = 0;
        }
    } else if (self->utf8LineEnds == LineEndType_Unicode && !UTF8IsAscii((char)chAfter)) {
        chBeforePrev = chPrev;
        chPrev = ch;
        for (int j = 0; j < UTF8SeparatorLength - 1; j++) {
            const unsigned char chAt = (unsigned char)SplitVector_Char_ValueAt(
                &self->substance, position + insertLength + j);
            const unsigned char back3[3] = { chBeforePrev, chPrev, chAt };
            if (UTF8IsSeparator(back3)) {
                CellBuffer_InsertLine(self, lineInsert,
                                       (position + insertLength + j) + 1, atLineStart);
                lineInsert++;
            }
            if ((j == 0) && UTF8IsNEL(back3 + 1)) {
                CellBuffer_InsertLine(self, lineInsert,
                                       (position + insertLength + j) + 1, atLineStart);
                lineInsert++;
            }
            chBeforePrev = chPrev;
            chPrev = chAt;
        }
    }

    if (maintainingIndex) {
        if (simpleInsertion && (lineInsert == lineStart)) {
            const CountWidths cw = CountCharacterWidthsUTF8(s, (size_t)insertLength);
            self->plv->vtbl->InsertCharacters(self->plv, linePosition, cw);
        } else {
            CB_RecalculateIndexLineStarts(self, linePosition, lineInsert - 1);
        }
    }
}

static void CB_BasicDeleteChars(CellBuffer *self, SciPosition position,
                                  SciPosition deleteLength) {
    if (deleteLength == 0) return;

    SciLine lineRecalculateStart = SCI_INVALID_POSITION;

    if ((position == 0) && (deleteLength == SplitVector_Char_Length(&self->substance))) {
        self->plv->vtbl->Init(self->plv);
    } else {
        const SciLine linePosition = self->plv->vtbl->LineFromPosition(self->plv, position);
        SciLine lineRemove = linePosition + 1;

        self->plv->vtbl->InsertText(self->plv, lineRemove - 1, -deleteLength);

        const unsigned char chPrev =
            (unsigned char)SplitVector_Char_ValueAt(&self->substance, position - 1);
        const unsigned char chBefore = chPrev;
        unsigned char chNext =
            (unsigned char)SplitVector_Char_ValueAt(&self->substance, position);

        if (self->utf8Substance && CB_MaintainingLineCharacterIndex(self)) {
            const SciPosition posEnd = position + deleteLength;
            const SciLine lineEndRemove = self->plv->vtbl->LineFromPosition(self->plv, posEnd);
            const int simpleDeletion =
                (linePosition == lineEndRemove) &&
                CB_UTF8IsCharacterBoundary(self, position) &&
                CB_UTF8IsCharacterBoundary(self, posEnd);
            if (simpleDeletion) {
                char *text = (char *)malloc((size_t)(deleteLength + 1));
                assert(text);
                CellBuffer_GetCharRange(self, text, position, deleteLength);
                if (UTF8IsValid(text, (size_t)deleteLength)) {
                    const CountWidths cw = CountCharacterWidthsUTF8(text, (size_t)deleteLength);
                    CountWidths neg = CountWidths_negate(cw);
                    self->plv->vtbl->InsertCharacters(self->plv, linePosition, neg);
                } else {
                    lineRecalculateStart = linePosition;
                }
                free(text);
            } else {
                lineRecalculateStart = linePosition;
            }
        }

        int ignoreNL = 0;
        if (chPrev == '\r' && chNext == '\n') {
            self->plv->vtbl->SetLineStart(self->plv, lineRemove, position);
            lineRemove++;
            ignoreNL = 1;
        }
        if (self->utf8LineEnds == LineEndType_Unicode && UTF8IsTrailByte(chNext)) {
            if (CB_UTF8LineEndOverlaps(self, position))
                CellBuffer_RemoveLine(self, lineRemove);
        }

        unsigned char chCur = chNext;
        for (SciPosition i = 0; i < deleteLength; i++) {
            chNext = (unsigned char)SplitVector_Char_ValueAt(&self->substance,
                                                              position + i + 1);
            if (chCur == '\r') {
                if (chNext != '\n')
                    CellBuffer_RemoveLine(self, lineRemove);
            } else if (chCur == '\n') {
                if (ignoreNL) {
                    ignoreNL = 0;
                } else {
                    CellBuffer_RemoveLine(self, lineRemove);
                }
            } else if (self->utf8LineEnds == LineEndType_Unicode) {
                if (!UTF8IsAscii((char)chCur)) {
                    const unsigned char chAt =
                        (unsigned char)SplitVector_Char_ValueAt(
                            &self->substance, position + i + 2);
                    const unsigned char next3[3] = { chCur, chNext, chAt };
                    if (UTF8IsSeparator(next3) || UTF8IsNEL(next3))
                        CellBuffer_RemoveLine(self, lineRemove);
                }
            }
            chCur = chNext;
        }

        const char chAfter = SplitVector_Char_ValueAt(&self->substance,
                                                       position + deleteLength);
        if (chBefore == '\r' && chAfter == '\n') {
            CellBuffer_RemoveLine(self, lineRemove - 1);
            self->plv->vtbl->SetLineStart(self->plv, lineRemove - 1, position + 1);
        }
    }

    SplitVector_Char_DeleteRange(&self->substance, position, deleteLength);
    if (lineRecalculateStart >= 0)
        CB_RecalculateIndexLineStarts(self, lineRecalculateStart, lineRecalculateStart);
    if (self->hasStyles)
        SplitVector_Char_DeleteRange(&self->style, position, deleteLength);
}
