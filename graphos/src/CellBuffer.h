/* Scintilla source code edit control
 * CellBuffer.h — C translation of scintilla/src/CellBuffer.h
 *
 * ── C++ → C translation map ──────────────────────────────────────────
 *
 *   namespace Scintilla::Internal   → removed; names prefixed with CB_ or kept flat
 *   class PerLine                   → vtable struct PerLine_vtbl + opaque PerLine
 *   class UndoHistory               → opaque struct UndoHistory (UndoHistory.h)
 *   class ChangeHistory             → opaque struct ChangeHistory (ChangeHistory.h)
 *   class ILineVector               → vtable struct ILineVector_vtbl + opaque ILineVector
 *   enum class ActionType           → CellBufferTypes.h ActionType enum
 *   struct Action                   → CellBufferTypes.h Action struct
 *   struct SplitView                → CellBufferTypes.h SplitView struct
 *   class CellBuffer                → struct CellBuffer + CellBuffer_* functions
 *
 *   std::unique_ptr<UndoHistory>    → UndoHistory *   (CellBuffer owns, must free)
 *   std::unique_ptr<ChangeHistory>  → ChangeHistory *  (same)
 *   std::unique_ptr<ILineVector>    → ILineVector *    (same)
 *   SplitVector<char> substance     → SplitVector_Char substance
 *   SplitVector<char> style         → SplitVector_Char style
 *
 *   virtual ~PerLine()              → function pointer in vtbl
 *   virtual void Init()=0           → function pointer in vtbl
 *   ...
 *
 *   [[nodiscard]]                   → SCI_NODISCARD  (ScintillaTypes.h)
 *   noexcept                        → removed
 *   bool                            → int
 *   std::string_view                → SciStringView  (CellBufferTypes.h)
 */

#ifndef CELLBUFFER_C_H
#define CELLBUFFER_C_H

#include <stddef.h>
#include "Position.h"
#include "ScintillaTypes.h"
#include "CellBufferTypes.h"
#include "SplitVector.h"
#include "UndoHistory.h"
#include "ChangeHistory.h"

/* ── PerLine virtual interface ───────────────────────────────────────
 *
 * C++ original:
 *   class PerLine {
 *   public:
 *       virtual ~PerLine() {}
 *       virtual void Init()=0;
 *       virtual void InsertLine(Sci::Line line)=0;
 *       virtual void InsertLines(Sci::Line line, Sci::Line lines)=0;
 *       virtual void RemoveLine(Sci::Line line)=0;
 *   };
 *
 * C translation: vtable struct + opaque base.
 * Concrete implementations store their data in memory immediately after
 * the vtable pointer (the vtable pointer is the first member — same as
 * C++ vptr position).
 */
typedef struct PerLine PerLine;

typedef struct {
    void (*destroy)(PerLine *self);
    void (*Init)        (PerLine *self);
    void (*InsertLine)  (PerLine *self, SciLine line);
    void (*InsertLines) (PerLine *self, SciLine line, SciLine lines);
    void (*RemoveLine)  (PerLine *self, SciLine line);
} PerLine_vtbl;

struct PerLine {
    const PerLine_vtbl *vtbl;   /* must be first member */
};

/* ── CountWidths ─────────────────────────────────────────────────────
 *
 * C++ original: struct CountWidths { Sci::Position countBasePlane;
 *                                     Sci::Position countOtherPlanes; … }
 *
 * operator-() → CountWidths_negate()
 */
typedef struct {
    SciPosition countBasePlane;
    SciPosition countOtherPlanes;
} CountWidths;

static inline SciPosition CountWidths_WidthUTF32(const CountWidths *cw) {
    return cw->countBasePlane + cw->countOtherPlanes;
}
static inline SciPosition CountWidths_WidthUTF16(const CountWidths *cw) {
    return cw->countBasePlane + 2 * cw->countOtherPlanes;
}
static inline void CountWidths_CountChar(CountWidths *cw, int lenChar) {
    if (lenChar == 4) cw->countOtherPlanes++;
    else              cw->countBasePlane++;
}
static inline CountWidths CountWidths_negate(CountWidths cw) {
    CountWidths r = { -cw.countBasePlane, -cw.countOtherPlanes };
    return r;
}

/* ── ILineVector virtual interface ───────────────────────────────────
 *
 * C++ original: class ILineVector { virtual … = 0; };
 *
 * C translation: vtable struct (function pointer table).
 * Concrete impls (LineVector32 and LineVector64) store their data after
 * the vtable pointer and register a static vtable instance.
 */
typedef struct ILineVector ILineVector;

typedef struct {
    void   (*destroy)                (ILineVector *self);
    void   (*Init)                   (ILineVector *self);
    void   (*SetPerLine)             (ILineVector *self, PerLine *pl);
    void   (*InsertText)             (ILineVector *self, SciLine line,
                                      SciPosition delta);
    void   (*InsertLine)             (ILineVector *self, SciLine line,
                                      SciPosition position, int lineStart);
    void   (*InsertLines)            (ILineVector *self, SciLine line,
                                      const SciPosition *positions,
                                      size_t lines, int lineStart);
    void   (*SetLineStart)           (ILineVector *self, SciLine line,
                                      SciPosition position);
    void   (*RemoveLine)             (ILineVector *self, SciLine line);
    SciLine (*Lines)                 (const ILineVector *self);
    void   (*AllocateLines)          (ILineVector *self, SciLine lines);
    SciLine (*LineFromPosition)      (const ILineVector *self, SciPosition pos);
    SciPosition (*LineStart)         (const ILineVector *self, SciLine line);
    void   (*InsertCharacters)       (ILineVector *self, SciLine line,
                                      CountWidths delta);
    void   (*SetLineCharactersWidth) (ILineVector *self, SciLine line,
                                      CountWidths width);
    LineCharacterIndexType (*LineCharacterIndex)(const ILineVector *self);
    int    (*AllocateLineCharacterIndex)(ILineVector *self,
                                         LineCharacterIndexType lci,
                                         SciLine lines);
    int    (*ReleaseLineCharacterIndex)(ILineVector *self,
                                        LineCharacterIndexType lci);
    SciPosition (*IndexLineStart)    (const ILineVector *self, SciLine line,
                                      LineCharacterIndexType lci);
    SciLine (*LineFromPositionIndex) (const ILineVector *self, SciPosition pos,
                                      LineCharacterIndexType lci);
} ILineVector_vtbl;

struct ILineVector {
    const ILineVector_vtbl *vtbl;   /* must be first member */
};

/* ── CellBuffer struct ───────────────────────────────────────────────
 *
 * C++ original: class CellBuffer { private: … }
 *
 * In C all fields are accessible (struct); naming convention prevents
 * accidental use from outside CellBuffer.c.
 */
typedef struct {
    int              hasStyles;
    int              largeDocument;
    SplitVector_Char substance;
    SplitVector_Char style;
    int              readOnly;
    int              utf8Substance;
    LineEndType      utf8LineEnds;
    int              collectingUndo;
    UndoHistory     *uh;
    ChangeHistory   *changeHistory;
    ILineVector     *plv;
} CellBuffer;

/* ── CellBuffer public API ───────────────────────────────────────────
 *
 * C++ public methods → CellBuffer_* free functions taking CellBuffer *self.
 * 'noexcept' removed; 'bool' → 'int'; '[[nodiscard]]' → SCI_NODISCARD.
 */

/* Lifecycle */
void CellBuffer_init   (CellBuffer *self, int hasStyles, int largeDocument);
void CellBuffer_destroy(CellBuffer *self);

/* Text access */
char         CellBuffer_CharAt        (const CellBuffer *self, SciPosition position);
unsigned char CellBuffer_UCharAt      (const CellBuffer *self, SciPosition position);
void          CellBuffer_GetCharRange  (const CellBuffer *self, char *buffer,
                                        SciPosition position,
                                        SciPosition lengthRetrieve);
char          CellBuffer_StyleAt       (const CellBuffer *self, SciPosition position);
void          CellBuffer_GetStyleRange (const CellBuffer *self, unsigned char *buffer,
                                        SciPosition position,
                                        SciPosition lengthRetrieve);
const char   *CellBuffer_BufferPointer (CellBuffer *self);
const char   *CellBuffer_RangePointer  (CellBuffer *self, SciPosition position,
                                        SciPosition rangeLength);
SciPosition   CellBuffer_GapPosition   (const CellBuffer *self);
SplitView     CellBuffer_AllView       (const CellBuffer *self);

/* Sizing / config */
SciPosition CellBuffer_Length     (const CellBuffer *self);
void        CellBuffer_Allocate   (CellBuffer *self, SciPosition newSize);
void        CellBuffer_SetUTF8Substance(CellBuffer *self, int utf8Substance);
LineEndType CellBuffer_GetLineEndTypes(const CellBuffer *self);
void        CellBuffer_SetLineEndTypes(CellBuffer *self, LineEndType utf8LineEnds);
int         CellBuffer_ContainsLineEnd(const CellBuffer *self, const char *s,
                                        SciPosition length);
void        CellBuffer_SetPerLine (CellBuffer *self, PerLine *pl);

/* Line character index */
LineCharacterIndexType CellBuffer_LineCharacterIndex(const CellBuffer *self);
void   CellBuffer_AllocateLineCharacterIndex(CellBuffer *self,
                                              LineCharacterIndexType lci);
void   CellBuffer_ReleaseLineCharacterIndex (CellBuffer *self,
                                              LineCharacterIndexType lci);

/* Line navigation */
SciLine     CellBuffer_Lines              (const CellBuffer *self);
void        CellBuffer_AllocateLines      (CellBuffer *self, SciLine lines);
SciPosition CellBuffer_LineStart          (const CellBuffer *self, SciLine line);
SciPosition CellBuffer_LineEnd            (const CellBuffer *self, SciLine line);
SciPosition CellBuffer_IndexLineStart     (const CellBuffer *self, SciLine line,
                                            LineCharacterIndexType lci);
SciLine     CellBuffer_LineFromPosition   (const CellBuffer *self, SciPosition pos);
SciLine     CellBuffer_LineFromPositionIndex(const CellBuffer *self, SciPosition pos,
                                              LineCharacterIndexType lci);
void        CellBuffer_InsertLine         (CellBuffer *self, SciLine line,
                                            SciPosition position, int lineStart);
void        CellBuffer_RemoveLine         (CellBuffer *self, SciLine line);

/* Text mutation */
const char *CellBuffer_InsertString(CellBuffer *self, SciPosition position,
                                     const char *s, SciPosition insertLength,
                                     int *startSequence);

/* Style mutation */
int CellBuffer_SetStyleAt (CellBuffer *self, SciPosition position, char styleValue);
int CellBuffer_SetStyleFor(CellBuffer *self, SciPosition position,
                            SciPosition lengthStyle, char styleValue);

/* Delete */
const char *CellBuffer_DeleteChars(CellBuffer *self, SciPosition position,
                                    SciPosition deleteLength, int *startSequence);

/* Read-only / flags */
int  CellBuffer_IsReadOnly(const CellBuffer *self);
void CellBuffer_SetReadOnly(CellBuffer *self, int set);
int  CellBuffer_IsLarge(const CellBuffer *self);
int  CellBuffer_HasStyles(const CellBuffer *self);

/* Save point */
void CellBuffer_SetSavePoint(CellBuffer *self);
int  CellBuffer_IsSavePoint(const CellBuffer *self);

/* Tentative */
void CellBuffer_TentativeStart(CellBuffer *self);
void CellBuffer_TentativeCommit(CellBuffer *self);
int  CellBuffer_TentativeActive(const CellBuffer *self);
int  CellBuffer_TentativeSteps(CellBuffer *self);

/* Undo collection */
int  CellBuffer_SetUndoCollection(CellBuffer *self, int collectUndo);
int  CellBuffer_IsCollectingUndo(const CellBuffer *self);
void CellBuffer_BeginUndoAction(CellBuffer *self, int mayCoalesce);
void CellBuffer_EndUndoAction(CellBuffer *self);
int  CellBuffer_UndoSequenceDepth(const CellBuffer *self);
int  CellBuffer_AfterUndoSequenceStart(const CellBuffer *self);
void CellBuffer_AddUndoAction(CellBuffer *self, SciPosition token, int mayCoalesce);
void CellBuffer_DeleteUndoHistory(CellBuffer *self);

/* Undo / redo */
int    CellBuffer_CanUndo(const CellBuffer *self);
int    CellBuffer_StartUndo(CellBuffer *self);
Action CellBuffer_GetUndoStep(const CellBuffer *self);
void   CellBuffer_PerformUndoStep(CellBuffer *self);
int    CellBuffer_CanRedo(const CellBuffer *self);
int    CellBuffer_StartRedo(CellBuffer *self);
Action CellBuffer_GetRedoStep(const CellBuffer *self);
void   CellBuffer_PerformRedoStep(CellBuffer *self);

/* Undo action inspection */
int          CellBuffer_UndoActions       (const CellBuffer *self);
void         CellBuffer_SetUndoSavePoint  (CellBuffer *self, int action);
int          CellBuffer_UndoSavePoint     (const CellBuffer *self);
void         CellBuffer_SetUndoDetach     (CellBuffer *self, int action);
int          CellBuffer_UndoDetach        (const CellBuffer *self);
void         CellBuffer_SetUndoTentative  (CellBuffer *self, int action);
int          CellBuffer_UndoTentative     (const CellBuffer *self);
void         CellBuffer_SetUndoCurrent    (CellBuffer *self, int action);
int          CellBuffer_UndoCurrent       (const CellBuffer *self);
int          CellBuffer_UndoActionType    (const CellBuffer *self, int action);
SciPosition  CellBuffer_UndoActionPosition(const CellBuffer *self, int action);
SciStringView CellBuffer_UndoActionText   (const CellBuffer *self, int action);
void         CellBuffer_PushUndoActionType(CellBuffer *self, int type,
                                            SciPosition position);
void         CellBuffer_ChangeLastUndoActionText(CellBuffer *self, size_t length,
                                                  const char *text);

/* Change history */
void         CellBuffer_ChangeHistorySet  (CellBuffer *self, int set);
SCI_NODISCARD int          CellBuffer_EditionAt       (const CellBuffer *self, SciPosition pos);
SCI_NODISCARD SciPosition  CellBuffer_EditionEndRun   (const CellBuffer *self, SciPosition pos);
SCI_NODISCARD unsigned int CellBuffer_EditionDeletesAt(const CellBuffer *self, SciPosition pos);
SCI_NODISCARD SciPosition  CellBuffer_EditionNextDelete(const CellBuffer *self, SciPosition pos);

#endif /* CELLBUFFER_C_H */
