/* Scintilla source code edit control
 * ChangeHistory.c — C translation of scintilla/src/ChangeHistory.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   std::vector<int> steps          →  int *steps; stepsLen; stepsCap
 *   std::vector<ChangeSpan> changes →  ChangeSpan *changes; changesLen; changesCap
 *   std::vector<EditionCount> (EditionSet)  →  EditionSet (heap array)
 *   std::unique_ptr<EditionSet>     →  EditionSet *  (manually freed)
 *   std::unique_ptr<ChangeLog>      →  ChangeLog *   (manually freed)
 *   SparseVector<EditionSetOwned>   →  SparseVector_Ptr (void * = EditionSet *)
 *   RunStyles<Sci::Position, int>   →  RunStyles_PD_Int
 *   enum class Direction            →  int  (CS_DIRECTION_INSERTION / _DELETION)
 *   for (const T &x : container)   →  explicit index loop
 *   std::min(a, b)                  →  (a < b ? a : b)
 *   assert / PLATFORM_ASSERT        →  assert
 *   noexcept / [[nodiscard]]        →  removed
 *   namespace Scintilla::Internal   →  removed
 *
 * Ownership of EditionSet * values stored in SparseVector_Ptr:
 *   SparseVector_Ptr does NOT free void * values automatically.
 *   ChangeLog_destroy / ChangeLog_Clear must iterate and free them.
 *   Functions that overwrite a SparseVector_Ptr slot must free the old
 *   value before calling SetValueAt with a new one.
 */

#include "ChangeHistory.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ═══════════════════════════════════════════════════════════════════════
 * EditionSet
 * ═══════════════════════════════════════════════════════════════════════ */

void EditionSet_init(EditionSet *self) {
    self->items = NULL;
    self->len   = 0;
    self->cap   = 0;
}

void EditionSet_destroy(EditionSet *self) {
    free(self->items);
    self->items = NULL;
    self->len   = 0;
    self->cap   = 0;
}

static void ES_EnsureCap(EditionSet *self) {
    if (self->len < self->cap) return;
    int cap = self->cap ? self->cap * 2 : 4;
    EditionCount *p = realloc(self->items, (size_t)cap * sizeof(EditionCount));
    assert(p);
    self->items = p;
    self->cap   = cap;
}

void EditionSet_push(EditionSet *self, EditionCount ec) {
    if (self->len > 0 && self->items[self->len - 1].edition == ec.edition) {
        self->items[self->len - 1].count += ec.count;
    } else {
        ES_EnsureCap(self);
        self->items[self->len++] = ec;
    }
}

void EditionSet_pop(EditionSet *self) {
    assert(self->len > 0);
    if (self->items[self->len - 1].count == 1) {
        self->len--;
    } else {
        self->items[self->len - 1].count--;
    }
}

void EditionSet_insert_front(EditionSet *self, EditionCount ec) {
    ES_EnsureCap(self);
    memmove(self->items + 1, self->items,
            (size_t)self->len * sizeof(EditionCount));
    self->items[0] = ec;
    self->len++;
}

int EditionSet_count(const EditionSet *self) {
    int total = 0;
    for (int i = 0; i < self->len; i++)
        total += self->items[i].count;
    return total;
}

int EditionSet_empty(const EditionSet *self) {
    return self->len == 0;
}

EditionSet *EditionSet_create(void) {
    EditionSet *es = malloc(sizeof(EditionSet));
    assert(es);
    EditionSet_init(es);
    return es;
}

void EditionSet_free(EditionSet *self) {
    if (!self) return;
    EditionSet_destroy(self);
    free(self);
}

/* ═══════════════════════════════════════════════════════════════════════
 * ChangeStack
 * ═══════════════════════════════════════════════════════════════════════ */

void ChangeStack_init(ChangeStack *self) {
    self->steps      = NULL;
    self->stepsLen   = 0;
    self->stepsCap   = 0;
    self->changes    = NULL;
    self->changesLen = 0;
    self->changesCap = 0;
}

void ChangeStack_destroy(ChangeStack *self) {
    free(self->steps);
    free(self->changes);
    self->steps      = NULL;
    self->changes    = NULL;
    self->stepsLen   = self->stepsCap   = 0;
    self->changesLen = self->changesCap = 0;
}

void ChangeStack_Clear(ChangeStack *self) {
    self->stepsLen   = 0;
    self->changesLen = 0;
}

void ChangeStack_AddStep(ChangeStack *self) {
    if (self->stepsLen == self->stepsCap) {
        int cap = self->stepsCap ? self->stepsCap * 2 : 8;
        int *p  = realloc(self->steps, (size_t)cap * sizeof(int));
        assert(p);
        self->steps    = p;
        self->stepsCap = cap;
    }
    self->steps[self->stepsLen++] = 0;
}

static int CS_SameDeletion(const ChangeSpan *cs, SciPosition pos, int edition) {
    return cs->direction == CS_DIRECTION_DELETION &&
           cs->start   == pos &&
           cs->length  == 0   &&
           cs->edition == edition;
}

void ChangeStack_PushDeletion(ChangeStack *self, SciPosition positionDeletion,
                               EditionCount ec) {
    assert(self->stepsLen > 0);
    self->steps[self->stepsLen - 1] += ec.count;
    if (self->changesLen > 0 &&
        CS_SameDeletion(&self->changes[self->changesLen - 1],
                        positionDeletion, ec.edition)) {
        self->changes[self->changesLen - 1].count += ec.count;
    } else {
        if (self->changesLen == self->changesCap) {
            int cap = self->changesCap ? self->changesCap * 2 : 8;
            ChangeSpan *p = realloc(self->changes,
                                    (size_t)cap * sizeof(ChangeSpan));
            assert(p);
            self->changes    = p;
            self->changesCap = cap;
        }
        ChangeSpan span = { positionDeletion, 0, ec.edition, ec.count,
                            CS_DIRECTION_DELETION };
        self->changes[self->changesLen++] = span;
    }
}

void ChangeStack_PushInsertion(ChangeStack *self, SciPosition positionInsertion,
                                SciPosition length, int edition) {
    assert(self->stepsLen > 0);
    self->steps[self->stepsLen - 1]++;
    if (self->changesLen == self->changesCap) {
        int cap = self->changesCap ? self->changesCap * 2 : 8;
        ChangeSpan *p = realloc(self->changes, (size_t)cap * sizeof(ChangeSpan));
        assert(p);
        self->changes    = p;
        self->changesCap = cap;
    }
    ChangeSpan span = { positionInsertion, length, edition, 1,
                        CS_DIRECTION_INSERTION };
    self->changes[self->changesLen++] = span;
}

int ChangeStack_PopStep(ChangeStack *self) {
    assert(self->stepsLen > 0);
    return self->steps[--self->stepsLen];
}

ChangeSpan ChangeStack_PopSpan(ChangeStack *self, int maxSteps) {
    assert(self->changesLen > 0);
    ChangeSpan span = self->changes[self->changesLen - 1];
    const int remove = maxSteps < span.count ? maxSteps : span.count;
    if (span.count == remove) {
        self->changesLen--;
    } else {
        self->changes[self->changesLen - 1].count -= remove;
        span.count = remove;
    }
    return span;
}

void ChangeStack_SetSavePoint(ChangeStack *self) {
    for (int i = 0; i < self->changesLen; i++) {
        if (self->changes[i].edition == CH_MODIFIED)
            self->changes[i].edition = CH_SAVED;
    }
}

void ChangeStack_Check(const ChangeStack *self) {
#ifdef _DEBUG
    int sizeSteps = 0;
    for (int i = 0; i < self->stepsLen; i++)
        sizeSteps += self->steps[i];
    int sizeInsertions = 0;
    for (int i = 0; i < self->changesLen; i++)
        sizeInsertions += self->changes[i].count;
    assert(sizeSteps == sizeInsertions);
#else
    (void)self;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════
 * ChangeLog helper — free all EditionSet * values stored in the
 * SparseVector_Ptr before clearing/destroying it.
 * ═══════════════════════════════════════════════════════════════════════ */

static void CL_FreeAllEditionSets(SparseVector_Ptr *sv) {
    const ptrdiff_t length = SparseVector_Ptr_Length(sv);
    ptrdiff_t pos = 0;
    while (pos <= length) {
        EditionSet *es = (EditionSet *)SparseVector_Ptr_ValueAt(sv, pos);
        if (es) {
            /* Zero out the slot first so we don't double-free on DeleteAll. */
            SparseVector_Ptr_SetValueAt(sv, pos, NULL);
            EditionSet_free(es);
        }
        pos = SparseVector_Ptr_PositionNext(sv, pos);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * ChangeLog
 * ═══════════════════════════════════════════════════════════════════════ */

void ChangeLog_init(ChangeLog *self) {
    ChangeStack_init(&self->changeStack);
    RunStyles_PD_Int_init(&self->insertEdition);
    SparseVector_Ptr_init(&self->deleteEdition);
}

void ChangeLog_destroy(ChangeLog *self) {
    CL_FreeAllEditionSets(&self->deleteEdition);
    ChangeStack_destroy(&self->changeStack);
    RunStyles_PD_Int_destroy(&self->insertEdition);
    SparseVector_Ptr_destroy(&self->deleteEdition);
}

ChangeLog *ChangeLog_create(void) {
    ChangeLog *cl = malloc(sizeof(ChangeLog));
    assert(cl);
    ChangeLog_init(cl);
    return cl;
}

void ChangeLog_destroy_ptr(ChangeLog *self) {
    if (!self) return;
    ChangeLog_destroy(self);
    free(self);
}

void ChangeLog_Clear(ChangeLog *self, SciPosition length) {
    ChangeStack_Clear(&self->changeStack);
    CL_FreeAllEditionSets(&self->deleteEdition);
    RunStyles_PD_Int_DeleteAll(&self->insertEdition);
    SparseVector_Ptr_DeleteAll(&self->deleteEdition);
    ChangeLog_InsertSpace(self, 0, length);
}

void ChangeLog_InsertSpace(ChangeLog *self, SciPosition position,
                            SciPosition insertLength) {
    assert(RunStyles_PD_Int_Length(&self->insertEdition) ==
           SparseVector_Ptr_Length(&self->deleteEdition));
    RunStyles_PD_Int_InsertSpace(&self->insertEdition, position, insertLength);
    SparseVector_Ptr_InsertSpace(&self->deleteEdition, position, insertLength);
}

void ChangeLog_DeleteRange(ChangeLog *self, SciPosition position,
                            SciPosition deleteLength) {
    RunStyles_PD_Int_DeleteRange(&self->insertEdition, position, deleteLength);
    EditionSet *editions = (EditionSet *)SparseVector_Ptr_ValueAt(
        &self->deleteEdition, position);
    if (editions) {
        /* Extract the edition set at position, collapse range, put it back. */
        EditionSet *saved = (EditionSet *)SparseVector_Ptr_Extract(
            &self->deleteEdition, position);
        SparseVector_Ptr_DeleteRange(&self->deleteEdition, position, deleteLength);
        SparseVector_Ptr_SetValueAt(&self->deleteEdition, position, saved);
    } else {
        SparseVector_Ptr_DeleteRange(&self->deleteEdition, position, deleteLength);
    }
    assert(RunStyles_PD_Int_Length(&self->insertEdition) ==
           SparseVector_Ptr_Length(&self->deleteEdition));
}

void ChangeLog_Insert(ChangeLog *self, SciPosition start, SciPosition length,
                       int edition) {
    RunStyles_PD_Int_FillRange(&self->insertEdition, start, edition, length);
}

void ChangeLog_CollapseRange(ChangeLog *self, SciPosition position,
                              SciPosition deleteLength) {
    const SciPosition positionMax = position + deleteLength;
    SciPosition positionDeletion = position + 1;
    while (positionDeletion <= positionMax) {
        EditionSet *editions = (EditionSet *)SparseVector_Ptr_ValueAt(
            &self->deleteEdition, positionDeletion);
        if (editions) {
            for (int i = 0; i < editions->len; i++)
                ChangeLog_PushDeletionAt(self, position, editions->items[i]);
            EditionSet *old = (EditionSet *)SparseVector_Ptr_Extract(
                &self->deleteEdition, positionDeletion);
            EditionSet_free(old);
        }
        positionDeletion = SparseVector_Ptr_PositionNext(
            &self->deleteEdition, positionDeletion);
    }
}

void ChangeLog_PushDeletionAt(ChangeLog *self, SciPosition position,
                               EditionCount ec) {
    EditionSet *editions = (EditionSet *)SparseVector_Ptr_ValueAt(
        &self->deleteEdition, position);
    if (!editions) {
        editions = EditionSet_create();
        SparseVector_Ptr_SetValueAt(&self->deleteEdition, position, editions);
    }
    EditionSet_push(editions, ec);
}

void ChangeLog_InsertFrontDeletionAt(ChangeLog *self, SciPosition position,
                                      EditionCount ec) {
    EditionSet *editions = (EditionSet *)SparseVector_Ptr_ValueAt(
        &self->deleteEdition, position);
    if (!editions) {
        editions = EditionSet_create();
        SparseVector_Ptr_SetValueAt(&self->deleteEdition, position, editions);
    }
    EditionSet_insert_front(editions, ec);
}

void ChangeLog_SaveRange(ChangeLog *self, SciPosition position,
                          SciPosition length) {
    ChangeStack_AddStep(&self->changeStack);
    SciPosition positionInsertion = position;
    const ptrdiff_t editionStart = RunStyles_PD_Int_ValueAt(
        &self->insertEdition, positionInsertion);
    if (editionStart == 0) {
        positionInsertion = RunStyles_PD_Int_EndRun(
            &self->insertEdition, positionInsertion);
    }
    const SciPosition positionMax = position + length;
    while (positionInsertion < positionMax) {
        const SciPosition positionEnd = RunStyles_PD_Int_EndRun(
            &self->insertEdition, positionInsertion);
        const SciPosition end = positionEnd < positionMax ? positionEnd : positionMax;
        ChangeStack_PushInsertion(
            &self->changeStack, positionInsertion,
            end - positionInsertion,
            RunStyles_PD_Int_ValueAt(&self->insertEdition, positionInsertion));
        positionInsertion = RunStyles_PD_Int_EndRun(
            &self->insertEdition, positionEnd);
    }
    SciPosition positionDeletion = position + 1;
    while (positionDeletion <= positionMax) {
        EditionSet *editions = (EditionSet *)SparseVector_Ptr_ValueAt(
            &self->deleteEdition, positionDeletion);
        if (editions) {
            for (int i = 0; i < editions->len; i++)
                ChangeStack_PushDeletion(&self->changeStack, positionDeletion,
                                         editions->items[i]);
        }
        positionDeletion = SparseVector_Ptr_PositionNext(
            &self->deleteEdition, positionDeletion);
    }
}

void ChangeLog_PopDeletion(ChangeLog *self, SciPosition position,
                            SciPosition deleteLength) {
    /* After InsertSpace(position, deleteLength) the element at position has
     * moved to position + deleteLength. Extract it and move it back. */
    EditionSet *eso = (EditionSet *)SparseVector_Ptr_Extract(
        &self->deleteEdition, position + deleteLength);
    /* Free any existing value at the target slot before replacing. */
    EditionSet *existing = (EditionSet *)SparseVector_Ptr_ValueAt(
        &self->deleteEdition, position);
    if (existing) {
        EditionSet *old = (EditionSet *)SparseVector_Ptr_Extract(
            &self->deleteEdition, position);
        EditionSet_free(old);
    }
    SparseVector_Ptr_SetValueAt(&self->deleteEdition, position, eso);
    EditionSet *editions = (EditionSet *)SparseVector_Ptr_ValueAt(
        &self->deleteEdition, position);
    assert(editions);
    EditionSet_pop(editions);

    const int inserts = ChangeStack_PopStep(&self->changeStack);
    for (int i = 0; i < inserts;) {
        const ChangeSpan span = ChangeStack_PopSpan(&self->changeStack, inserts);
        if (span.direction == CS_DIRECTION_INSERTION) {
            assert(span.count == 1);
            RunStyles_PD_Int_FillRange(&self->insertEdition,
                                        span.start, span.edition, span.length);
            i++;
        } else {
            assert(editions);
            assert(editions->items[editions->len - 1].edition == span.edition);
            for (int j = 0; j < span.count; j++)
                EditionSet_pop(editions);
            EditionCount ec = { span.edition, span.count };
            ChangeLog_InsertFrontDeletionAt(self, span.start, ec);
            i += span.count;
        }
    }

    if (EditionSet_empty(editions)) {
        EditionSet *e = (EditionSet *)SparseVector_Ptr_Extract(
            &self->deleteEdition, position);
        EditionSet_free(e);
    }
}

void ChangeLog_SaveHistoryForDelete(ChangeLog *self, SciPosition position,
                                     SciPosition deleteLength) {
    assert(position >= 0);
    assert(deleteLength >= 0);
    assert(position + deleteLength <= ChangeLog_Length(self));
    ChangeLog_SaveRange(self, position, deleteLength);
    ChangeLog_CollapseRange(self, position, deleteLength);
}

void ChangeLog_DeleteRangeSavingHistory(ChangeLog *self, SciPosition position,
                                         SciPosition deleteLength) {
    ChangeLog_SaveHistoryForDelete(self, position, deleteLength);
    ChangeLog_DeleteRange(self, position, deleteLength);
}

void ChangeLog_SetSavePoint(ChangeLog *self) {
    ChangeStack_SetSavePoint(&self->changeStack);
    const SciPosition length = RunStyles_PD_Int_Length(&self->insertEdition);
    for (SciPosition startRun = 0; startRun < length;) {
        const SciPosition endRun = RunStyles_PD_Int_EndRun(
            &self->insertEdition, startRun);
        if (RunStyles_PD_Int_ValueAt(&self->insertEdition, startRun) == CH_MODIFIED)
            RunStyles_PD_Int_FillRange(&self->insertEdition, startRun,
                                        CH_SAVED, endRun - startRun);
        startRun = endRun;
    }
    for (SciPosition positionDeletion = 0; positionDeletion <= length;) {
        EditionSet *editions = (EditionSet *)SparseVector_Ptr_ValueAt(
            &self->deleteEdition, positionDeletion);
        if (editions) {
            for (int i = 0; i < editions->len; i++) {
                if (editions->items[i].edition == CH_MODIFIED)
                    editions->items[i].edition = CH_SAVED;
            }
        }
        positionDeletion = SparseVector_Ptr_PositionNext(
            &self->deleteEdition, positionDeletion);
    }
}

SciPosition ChangeLog_Length(const ChangeLog *self) {
    return RunStyles_PD_Int_Length(&self->insertEdition);
}

size_t ChangeLog_DeletionCount(const ChangeLog *self, SciPosition start,
                                SciPosition length) {
    const SciPosition end = start + length;
    size_t count = 0;
    while (start <= end) {
        const EditionSet *editions = (const EditionSet *)SparseVector_Ptr_ValueAt(
            &self->deleteEdition, start);
        if (editions)
            count += (size_t)EditionSet_count(editions);
        start = SparseVector_Ptr_PositionNext(&self->deleteEdition, start);
    }
    return count;
}

void ChangeLog_Check(const ChangeLog *self) {
    assert(RunStyles_PD_Int_Length(&self->insertEdition) ==
           SparseVector_Ptr_Length(&self->deleteEdition));
    ChangeStack_Check(&self->changeStack);
}

/* ═══════════════════════════════════════════════════════════════════════
 * ChangeHistory
 * ═══════════════════════════════════════════════════════════════════════ */

ChangeHistory *ChangeHistory_create(SciPosition length) {
    ChangeHistory *self = malloc(sizeof(ChangeHistory));
    assert(self);
    ChangeLog_init(&self->changeLog);
    self->changeLogReversions = NULL;
    self->historicEpoch       = -1;
    ChangeLog_Clear(&self->changeLog, length);
    return self;
}

void ChangeHistory_destroy(ChangeHistory *self) {
    ChangeLog_destroy(&self->changeLog);
    ChangeLog_destroy_ptr(self->changeLogReversions);
    self->changeLogReversions = NULL;
    free(self);
}

void ChangeHistory_Insert(ChangeHistory *self, SciPosition position,
                           SciPosition insertLength,
                           int collectingUndo, int beforeSave) {
    ChangeHistory_Check(self);
    ChangeLog_InsertSpace(&self->changeLog, position, insertLength);
    const int edition = collectingUndo
                        ? (beforeSave ? CH_SAVED : CH_MODIFIED)
                        : CH_ORIGINAL;
    ChangeLog_Insert(&self->changeLog, position, insertLength, edition);
    if (self->changeLogReversions) {
        ChangeLog_InsertSpace(self->changeLogReversions, position, insertLength);
        if (beforeSave)
            ChangeLog_PopDeletion(self->changeLogReversions, position,
                                  insertLength);
    }
    ChangeHistory_Check(self);
}

void ChangeHistory_DeleteRange(ChangeHistory *self, SciPosition position,
                                SciPosition deleteLength, int reverting) {
    ChangeHistory_Check(self);
    assert(ChangeHistory_DeletionCount(self, position, deleteLength - 1) == 0);
    ChangeLog_DeleteRange(&self->changeLog, position, deleteLength);
    if (self->changeLogReversions) {
        ChangeLog_DeleteRangeSavingHistory(self->changeLogReversions, position,
                                            deleteLength);
        if (reverting) {
            EditionCount ec = { CH_REVERTED_ORIGINAL, 1 };
            ChangeLog_PushDeletionAt(self->changeLogReversions, position, ec);
        }
    }
    ChangeHistory_Check(self);
}

void ChangeHistory_DeleteRangeSavingHistory(ChangeHistory *self,
                                             SciPosition position,
                                             SciPosition deleteLength,
                                             int beforeSave, int isDetached) {
    ChangeLog_DeleteRangeSavingHistory(&self->changeLog, position, deleteLength);
    EditionCount ec = { beforeSave ? CH_SAVED : CH_MODIFIED, 1 };
    ChangeLog_PushDeletionAt(&self->changeLog, position, ec);
    if (self->changeLogReversions) {
        if (isDetached)
            ChangeLog_SaveHistoryForDelete(self->changeLogReversions, position,
                                           deleteLength);
        ChangeLog_DeleteRange(self->changeLogReversions, position, deleteLength);
    }
    ChangeHistory_Check(self);
}

void ChangeHistory_StartReversion(ChangeHistory *self) {
    if (!self->changeLogReversions) {
        self->changeLogReversions = ChangeLog_create();
        ChangeLog_Clear(self->changeLogReversions,
                        ChangeLog_Length(&self->changeLog));
    }
    ChangeHistory_Check(self);
}

void ChangeHistory_EndReversion(ChangeHistory *self) {
    ChangeLog_destroy_ptr(self->changeLogReversions);
    self->changeLogReversions = NULL;
    ChangeHistory_Check(self);
}

void ChangeHistory_SetSavePoint(ChangeHistory *self) {
    ChangeLog_SetSavePoint(&self->changeLog);
    ChangeHistory_EndReversion(self);
}

void ChangeHistory_UndoDeleteStep(ChangeHistory *self, SciPosition position,
                                   SciPosition deleteLength, int isDetached) {
    ChangeHistory_Check(self);
    ChangeLog_InsertSpace(&self->changeLog, position, deleteLength);
    ChangeLog_PopDeletion(&self->changeLog, position, deleteLength);
    if (self->changeLogReversions) {
        ChangeLog_InsertSpace(self->changeLogReversions, position, deleteLength);
        if (!isDetached)
            ChangeLog_Insert(self->changeLogReversions, position, deleteLength, 1);
    }
    ChangeHistory_Check(self);
}

SciPosition ChangeHistory_Length(const ChangeHistory *self) {
    return ChangeLog_Length(&self->changeLog);
}

void ChangeHistory_SetEpoch(ChangeHistory *self, int epoch) {
    self->historicEpoch = epoch;
}

void ChangeHistory_EditionCreateHistory(ChangeHistory *self, SciPosition start,
                                         SciPosition length) {
    if (start <= ChangeLog_Length(&self->changeLog)) {
        if (length) {
            RunStyles_PD_Int_FillRange(&self->changeLog.insertEdition, start,
                                        self->historicEpoch, length);
        } else {
            EditionCount ec = { self->historicEpoch, 1 };
            ChangeLog_PushDeletionAt(&self->changeLog, start, ec);
        }
    }
}

int ChangeHistory_EditionAt(const ChangeHistory *self, SciPosition pos) {
    const int edition = RunStyles_PD_Int_ValueAt(
        &self->changeLog.insertEdition, pos);
    if (self->changeLogReversions) {
        const int editionReversion = RunStyles_PD_Int_ValueAt(
            &self->changeLogReversions->insertEdition, pos);
        if (editionReversion) {
            if (edition < 0)
                return CH_REVERTED_ORIGINAL;
            return edition ? CH_REVERTED_TO_CHANGE : CH_REVERTED_ORIGINAL;
        }
    }
    return edition;
}

SciPosition ChangeHistory_EditionEndRun(const ChangeHistory *self,
                                         SciPosition pos) {
    if (self->changeLogReversions) {
        assert(ChangeLog_Length(self->changeLogReversions) ==
               ChangeLog_Length(&self->changeLog));
        const SciPosition nextReversion = RunStyles_PD_Int_EndRun(
            &self->changeLogReversions->insertEdition, pos);
        const SciPosition next = RunStyles_PD_Int_EndRun(
            &self->changeLog.insertEdition, pos);
        return next < nextReversion ? next : nextReversion;
    }
    return RunStyles_PD_Int_EndRun(&self->changeLog.insertEdition, pos);
}

unsigned int ChangeHistory_EditionDeletesAt(const ChangeHistory *self,
                                             SciPosition pos) {
    unsigned int editionSet = 0;
    const EditionSet *editionSetDeletions = (const EditionSet *)SparseVector_Ptr_ValueAt(
        &self->changeLog.deleteEdition, pos);
    if (editionSetDeletions) {
        for (int i = 0; i < editionSetDeletions->len; i++)
            editionSet |= 1u << (unsigned)(editionSetDeletions->items[i].edition - 1);
    }
    if (self->changeLogReversions) {
        const EditionSet *editionSetReversions = (const EditionSet *)SparseVector_Ptr_ValueAt(
            &self->changeLogReversions->deleteEdition, pos);
        if (editionSetReversions) {
            if (!(editionSet & (CH_BIT_SAVED | CH_BIT_MODIFIED)))
                editionSet |= CH_BIT_REVERTED_TO_ORIGINAL;
            else
                editionSet |= CH_BIT_REVERTED_TO_MODIFIED;
        }
    }
    return editionSet;
}

SciPosition ChangeHistory_EditionNextDelete(const ChangeHistory *self,
                                             SciPosition pos) {
    const SciPosition next = SparseVector_Ptr_PositionNext(
        &self->changeLog.deleteEdition, pos);
    if (self->changeLogReversions) {
        const SciPosition nextReversion = SparseVector_Ptr_PositionNext(
            &self->changeLogReversions->deleteEdition, pos);
        return next < nextReversion ? next : nextReversion;
    }
    return next;
}

size_t ChangeHistory_DeletionCount(const ChangeHistory *self, SciPosition start,
                                    SciPosition length) {
    return ChangeLog_DeletionCount(&self->changeLog, start, length);
}

void ChangeHistory_Check(ChangeHistory *self) {
    ChangeLog_Check(&self->changeLog);
    if (self->changeLogReversions) {
        ChangeLog_Check(self->changeLogReversions);
        assert(ChangeLog_Length(self->changeLogReversions) ==
               ChangeLog_Length(&self->changeLog));
    }
}
