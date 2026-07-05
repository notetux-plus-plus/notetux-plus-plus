/* Scintilla source code edit control
 * ChangeHistory.h — C translation of scintilla/src/ChangeHistory.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   constexpr int changeOriginal …   →  #define CH_ORIGINAL …
 *   constexpr unsigned int bitSaved  →  #define CH_BIT_SAVED …
 *
 *   struct ChangeSpan          →  ChangeSpan  (enum Direction → int macros)
 *   struct EditionCount        →  EditionCount
 *   using EditionSet = std::vector<EditionCount>  →  EditionSet (heap array)
 *   using EditionSetOwned = std::unique_ptr<EditionSet>  →  EditionSet *
 *
 *   class ChangeStack:
 *     std::vector<int> steps        →  int *steps; int stepsLen; int stepsCap
 *     std::vector<ChangeSpan> changes → ChangeSpan *changes; int changesLen; int changesCap
 *
 *   struct ChangeLog:
 *     RunStyles<Sci::Position, int> insertEdition  →  RunStyles_PD_Int
 *     SparseVector<EditionSetOwned> deleteEdition  →  SparseVector_Ptr
 *     (void * elements are EditionSet *)
 *
 *   class ChangeHistory:
 *     ChangeLog changeLog                  →  ChangeLog changeLog  (value)
 *     std::unique_ptr<ChangeLog> changeLogReversions  →  ChangeLog *changeLogReversions
 *     int historicEpoch = -1               →  int historicEpoch
 *
 *   std::make_unique<ChangeLog>()  →  malloc + ChangeLog_init (via helper)
 *   changeLogReversions.reset()    →  ChangeLog_destroy_ptr(p); p = NULL
 *   std::min(a, b)                 →  (a < b ? a : b)
 *   PLATFORM_ASSERT / assert       →  assert
 *   noexcept / [[nodiscard]]       →  removed
 *   namespace Scintilla::Internal  →  removed (flat C namespace)
 */

#ifndef CHANGEHISTORY_C_H
#define CHANGEHISTORY_C_H

#include <stddef.h>
#include "Position.h"
#include "RunStyles.h"
#include "SparseVector.h"

/* ── Edition constants ─────────────────────────────────────────────── */
#define CH_ORIGINAL             0
#define CH_REVERTED_ORIGINAL    1
#define CH_SAVED                2
#define CH_MODIFIED             3
#define CH_REVERTED_TO_CHANGE   4

#define CH_BIT_REVERTED_TO_ORIGINAL   1u
#define CH_BIT_SAVED                  2u
#define CH_BIT_MODIFIED               4u
#define CH_BIT_REVERTED_TO_MODIFIED   8u

/* ── EditionCount ──────────────────────────────────────────────────── */
typedef struct {
    int edition;
    int count;
} EditionCount;

/* ── EditionSet  (heap-allocated growable array of EditionCount) ───── */
typedef struct {
    EditionCount *items;
    int           len;
    int           cap;
} EditionSet;

void EditionSet_init    (EditionSet *self);
void EditionSet_destroy (EditionSet *self);
void EditionSet_push    (EditionSet *self, EditionCount ec);
void EditionSet_pop     (EditionSet *self);   /* removes/decrements last item */
void EditionSet_insert_front(EditionSet *self, EditionCount ec);
int  EditionSet_count   (const EditionSet *self);
int  EditionSet_empty   (const EditionSet *self);

/* Heap lifecycle (mirrors unique_ptr<EditionSet>) */
EditionSet *EditionSet_create (void);
void        EditionSet_free   (EditionSet *self);   /* free items + struct */

/* ── ChangeSpan ────────────────────────────────────────────────────── */
#define CS_DIRECTION_INSERTION 0
#define CS_DIRECTION_DELETION  1

typedef struct {
    SciPosition start;
    SciPosition length;
    int         edition;
    int         count;
    int         direction;   /* CS_DIRECTION_INSERTION or CS_DIRECTION_DELETION */
} ChangeSpan;

/* ── ChangeStack ───────────────────────────────────────────────────── */
typedef struct {
    int        *steps;
    int         stepsLen;
    int         stepsCap;
    ChangeSpan *changes;
    int         changesLen;
    int         changesCap;
} ChangeStack;

void       ChangeStack_init          (ChangeStack *self);
void       ChangeStack_destroy       (ChangeStack *self);
void       ChangeStack_Clear         (ChangeStack *self);
void       ChangeStack_AddStep       (ChangeStack *self);
void       ChangeStack_PushDeletion  (ChangeStack *self, SciPosition positionDeletion,
                                      EditionCount ec);
void       ChangeStack_PushInsertion (ChangeStack *self, SciPosition positionInsertion,
                                      SciPosition length, int edition);
int        ChangeStack_PopStep       (ChangeStack *self);
ChangeSpan ChangeStack_PopSpan       (ChangeStack *self, int maxSteps);
void       ChangeStack_SetSavePoint  (ChangeStack *self);
void       ChangeStack_Check         (const ChangeStack *self);

/* ── ChangeLog ─────────────────────────────────────────────────────── */
typedef struct {
    ChangeStack      changeStack;
    RunStyles_PD_Int insertEdition;
    SparseVector_Ptr deleteEdition;   /* void * elements are EditionSet * */
} ChangeLog;

void        ChangeLog_init     (ChangeLog *self);
void        ChangeLog_destroy  (ChangeLog *self);

void        ChangeLog_Clear                  (ChangeLog *self, SciPosition length);
void        ChangeLog_InsertSpace            (ChangeLog *self, SciPosition position,
                                              SciPosition insertLength);
void        ChangeLog_DeleteRange            (ChangeLog *self, SciPosition position,
                                              SciPosition deleteLength);
void        ChangeLog_Insert                 (ChangeLog *self, SciPosition start,
                                              SciPosition length, int edition);
void        ChangeLog_CollapseRange          (ChangeLog *self, SciPosition position,
                                              SciPosition deleteLength);
void        ChangeLog_PushDeletionAt         (ChangeLog *self, SciPosition position,
                                              EditionCount ec);
void        ChangeLog_InsertFrontDeletionAt  (ChangeLog *self, SciPosition position,
                                              EditionCount ec);
void        ChangeLog_SaveRange              (ChangeLog *self, SciPosition position,
                                              SciPosition length);
void        ChangeLog_PopDeletion            (ChangeLog *self, SciPosition position,
                                              SciPosition deleteLength);
void        ChangeLog_SaveHistoryForDelete   (ChangeLog *self, SciPosition position,
                                              SciPosition deleteLength);
void        ChangeLog_DeleteRangeSavingHistory(ChangeLog *self, SciPosition position,
                                              SciPosition deleteLength);
void        ChangeLog_SetSavePoint           (ChangeLog *self);
SciPosition ChangeLog_Length                 (const ChangeLog *self);
size_t      ChangeLog_DeletionCount          (const ChangeLog *self, SciPosition start,
                                              SciPosition length);
void        ChangeLog_Check                  (const ChangeLog *self);

/* Heap lifecycle for changeLogReversions */
ChangeLog  *ChangeLog_create (void);
void        ChangeLog_destroy_ptr(ChangeLog *self);

/* ── ChangeHistory ─────────────────────────────────────────────────── */
typedef struct {
    ChangeLog  changeLog;
    ChangeLog *changeLogReversions;   /* NULL when not reverting */
    int        historicEpoch;
} ChangeHistory;

/* Lifecycle — CellBuffer stores ChangeHistory * so we use create/destroy */
ChangeHistory *ChangeHistory_create (SciPosition length);
void           ChangeHistory_destroy(ChangeHistory *self);

void         ChangeHistory_Insert               (ChangeHistory *self, SciPosition position,
                                                  SciPosition insertLength,
                                                  int collectingUndo, int beforeSave);
void         ChangeHistory_DeleteRange          (ChangeHistory *self, SciPosition position,
                                                  SciPosition deleteLength, int reverting);
void         ChangeHistory_DeleteRangeSavingHistory(ChangeHistory *self, SciPosition position,
                                                  SciPosition deleteLength,
                                                  int beforeSave, int isDetached);
void         ChangeHistory_StartReversion       (ChangeHistory *self);
void         ChangeHistory_EndReversion         (ChangeHistory *self);
void         ChangeHistory_SetSavePoint         (ChangeHistory *self);
void         ChangeHistory_UndoDeleteStep       (ChangeHistory *self, SciPosition position,
                                                  SciPosition deleteLength, int isDetached);
SciPosition  ChangeHistory_Length               (const ChangeHistory *self);
void         ChangeHistory_SetEpoch             (ChangeHistory *self, int epoch);
void         ChangeHistory_EditionCreateHistory (ChangeHistory *self, SciPosition start,
                                                  SciPosition length);
int          ChangeHistory_EditionAt            (const ChangeHistory *self, SciPosition pos);
SciPosition  ChangeHistory_EditionEndRun        (const ChangeHistory *self, SciPosition pos);
unsigned int ChangeHistory_EditionDeletesAt     (const ChangeHistory *self, SciPosition pos);
SciPosition  ChangeHistory_EditionNextDelete    (const ChangeHistory *self, SciPosition pos);
size_t       ChangeHistory_DeletionCount        (const ChangeHistory *self, SciPosition start,
                                                  SciPosition length);
void         ChangeHistory_Check                (ChangeHistory *self);

#endif /* CHANGEHISTORY_C_H */
