/* Scintilla source code edit control
 * ChangeHistory.h — C translation of scintilla/src/ChangeHistory.h
 *
 * ChangeHistory is an optional feature (change-tracking gutter).
 * Full translation deferred to a later pass; exposed as opaque struct
 * so CellBuffer.c can compile.
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   class ChangeHistory        → opaque struct ChangeHistory
 *   std::unique_ptr<…>         → ChangeHistory *  (pointer in CellBuffer)
 *   make_unique<ChangeHistory>(length)  → ChangeHistory_create(length)
 *   changeHistory.reset()      → ChangeHistory_destroy(ch); ch = NULL
 *
 * Constants from the original header are translated to #defines:
 *   constexpr int changeOriginal = 0   → #define CH_ORIGINAL 0
 */

#ifndef CHANGEHISTORY_C_H
#define CHANGEHISTORY_C_H

#include "Position.h"

/* ── Edition constants ─────────────────────────────────────────────── */
#define CH_ORIGINAL             0
#define CH_REVERTED_ORIGINAL    1
#define CH_SAVED                2
#define CH_MODIFIED             3
#define CH_REVERTED_TO_CHANGE   4

/* Bit flags */
#define CH_BIT_REVERTED_TO_ORIGINAL   1u
#define CH_BIT_SAVED                  2u
#define CH_BIT_MODIFIED               4u
#define CH_BIT_REVERTED_TO_MODIFIED   8u

/* ── Opaque struct ─────────────────────────────────────────────────── */
typedef struct ChangeHistory ChangeHistory;

/* ── Lifecycle ─────────────────────────────────────────────────────── */
ChangeHistory *ChangeHistory_create(SciPosition length);
void           ChangeHistory_destroy(ChangeHistory *self);

/* ── Mutation ──────────────────────────────────────────────────────── */
void ChangeHistory_Insert(ChangeHistory *self, SciPosition position,
                           SciPosition insertLength,
                           int collectingUndo, int beforeSave);
void ChangeHistory_DeleteRange(ChangeHistory *self, SciPosition position,
                                SciPosition deleteLength, int reverting);
void ChangeHistory_DeleteRangeSavingHistory(ChangeHistory *self,
                                             SciPosition position,
                                             SciPosition deleteLength,
                                             int beforeSave, int isDetached);

/* ── Reversion ─────────────────────────────────────────────────────── */
void ChangeHistory_StartReversion(ChangeHistory *self);
void ChangeHistory_EndReversion(ChangeHistory *self);

/* ── Save point ────────────────────────────────────────────────────── */
void ChangeHistory_SetSavePoint(ChangeHistory *self);

/* ── Undo support ──────────────────────────────────────────────────── */
void ChangeHistory_UndoDeleteStep(ChangeHistory *self, SciPosition position,
                                   SciPosition deleteLength, int isDetached);

/* ── Query ─────────────────────────────────────────────────────────── */
SciPosition  ChangeHistory_Length(const ChangeHistory *self);
int          ChangeHistory_EditionAt(const ChangeHistory *self, SciPosition pos);
SciPosition  ChangeHistory_EditionEndRun(const ChangeHistory *self, SciPosition pos);
unsigned int ChangeHistory_EditionDeletesAt(const ChangeHistory *self, SciPosition pos);
SciPosition  ChangeHistory_EditionNextDelete(const ChangeHistory *self, SciPosition pos);

/* ── Debug ─────────────────────────────────────────────────────────── */
void ChangeHistory_Check(ChangeHistory *self);

#endif /* CHANGEHISTORY_C_H */
