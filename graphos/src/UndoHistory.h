/* Scintilla source code edit control
 * UndoHistory.h — C translation of scintilla/src/UndoHistory.h
 *
 * UndoHistory is a complex class that will be fully translated in a later
 * pass (it depends on std::vector, std::string, std::optional, ScrapStack,
 * ScaledVector, UndoActions).  For now we expose it as an opaque struct so
 * that CellBuffer.c can compile — all access is through function pointers.
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   class UndoHistory        → opaque struct UndoHistory
 *   std::unique_ptr<…> uh   → UndoHistory *uh  (CellBuffer holds pointer)
 *   uh = std::make_unique<UndoHistory>()  → uh = UndoHistory_create()
 *   uh.reset()               → UndoHistory_destroy(uh); uh = NULL
 *
 *   std::string_view Text()  → SciStringView UndoHistory_Text(…)
 *   where SciStringView = { const char *ptr; size_t len; }
 */

#ifndef UNDOHISTORY_C_H
#define UNDOHISTORY_C_H

#include <stddef.h>   /* size_t */
#include <stdint.h>
#include "Position.h"
#include "ScintillaTypes.h"
#include "CellBufferTypes.h"   /* Action, ActionType, SciStringView */

/* ── Opaque struct ─────────────────────────────────────────────────── */
typedef struct UndoHistory UndoHistory;

/* ── Lifecycle ─────────────────────────────────────────────────────── */
UndoHistory *UndoHistory_create(void);
void         UndoHistory_destroy(UndoHistory *self);

/* ── Core append ───────────────────────────────────────────────────── */
const char *UndoHistory_AppendAction(UndoHistory *self, ActionType at,
                                      SciPosition position, const char *data,
                                      SciPosition lengthData,
                                      int *startSequence, int mayCoalesce);

/* ── Undo sequence control ─────────────────────────────────────────── */
void UndoHistory_BeginUndoAction(UndoHistory *self, int mayCoalesce);
void UndoHistory_EndUndoAction(UndoHistory *self);
int  UndoHistory_UndoSequenceDepth(const UndoHistory *self);
int  UndoHistory_AfterUndoSequenceStart(const UndoHistory *self);
void UndoHistory_DropUndoSequence(UndoHistory *self);
void UndoHistory_DeleteUndoHistory(UndoHistory *self);

/* ── Action count ──────────────────────────────────────────────────── */
int UndoHistory_Actions(const UndoHistory *self);

/* ── Save point ────────────────────────────────────────────────────── */
void UndoHistory_SetSavePointAction(UndoHistory *self, int action);
int  UndoHistory_SavePoint(const UndoHistory *self);
void UndoHistory_SetSavePoint(UndoHistory *self);
int  UndoHistory_IsSavePoint(const UndoHistory *self);
int  UndoHistory_BeforeSavePoint(const UndoHistory *self);
int  UndoHistory_PreviousBeforeSavePoint(const UndoHistory *self);
int  UndoHistory_BeforeReachableSavePoint(const UndoHistory *self);
int  UndoHistory_AfterSavePoint(const UndoHistory *self);

/* ── Detach point ──────────────────────────────────────────────────── */
void UndoHistory_SetDetachPoint(UndoHistory *self, int action);
int  UndoHistory_DetachPoint(const UndoHistory *self);
int  UndoHistory_AfterDetachPoint(const UndoHistory *self);
int  UndoHistory_AfterOrAtDetachPoint(const UndoHistory *self);

/* ── Delta / validate / current ────────────────────────────────────── */
intptr_t     UndoHistory_Delta(const UndoHistory *self, int action);
int          UndoHistory_Validate(const UndoHistory *self, intptr_t lengthDoc);
void         UndoHistory_SetCurrent(UndoHistory *self, int action,
                                     intptr_t lengthDocument);
int          UndoHistory_Current(const UndoHistory *self);

/* ── Inspection by action index ────────────────────────────────────── */
int          UndoHistory_Type(const UndoHistory *self, int action);
SciPosition  UndoHistory_Position(const UndoHistory *self, int action);
SciPosition  UndoHistory_Length(const UndoHistory *self, int action);
SciStringView UndoHistory_Text(UndoHistory *self, int action);
void         UndoHistory_PushUndoActionType(UndoHistory *self, int type,
                                             SciPosition position);
void         UndoHistory_ChangeLastUndoActionText(UndoHistory *self,
                                                   size_t length,
                                                   const char *text);

/* ── Tentative actions ─────────────────────────────────────────────── */
void UndoHistory_SetTentative(UndoHistory *self, int action);
int  UndoHistory_TentativePoint(const UndoHistory *self);
void UndoHistory_TentativeStart(UndoHistory *self);
void UndoHistory_TentativeCommit(UndoHistory *self);
int  UndoHistory_TentativeActive(const UndoHistory *self);
int  UndoHistory_TentativeSteps(const UndoHistory *self);

/* ── Undo / redo ───────────────────────────────────────────────────── */
int    UndoHistory_CanUndo(const UndoHistory *self);
int    UndoHistory_StartUndo(const UndoHistory *self);
Action UndoHistory_GetUndoStep(const UndoHistory *self);
void   UndoHistory_CompletedUndoStep(UndoHistory *self);
int    UndoHistory_CanRedo(const UndoHistory *self);
int    UndoHistory_StartRedo(const UndoHistory *self);
Action UndoHistory_GetRedoStep(const UndoHistory *self);
void   UndoHistory_CompletedRedoStep(UndoHistory *self);

#endif /* UNDOHISTORY_C_H */
