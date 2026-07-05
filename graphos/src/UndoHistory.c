/* Scintilla source code edit control
 * UndoHistory.c — C translation of scintilla/src/UndoHistory.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   std::vector<T>::erase(begin+n, end)  →  .len = n  (truncate in place)
 *   std::vector<T>::emplace_back()       →  grow array + zero-init element
 *   std::vector<T>::clear()              →  .len = 0  (keep capacity)
 *   std::swap(a, b)                      →  tmp = a; a = b; b = tmp
 *   std::optional<int> detach            →  int detach + int detach_set
 *   detach.reset()                       →  detach_set = 0
 *   detach = value                       →  detach = value; detach_set = 1
 *   detach.value_or(-1)                  →  detach_set ? detach : -1
 *   !detach                              →  !detach_set
 *   *detach                              →  detach
 *   std::optional<actPos> memory         →  memory_act + memory_pos + memory_set
 *   memory = {}                          →  memory_set = 0
 *   std::make_unique<ScrapStack>()       →  malloc + ScrapStack_init
 *   std::string_view                     →  SciStringView
 *   throw std::runtime_error(…)         →  assert(!"…"); abort()
 *   PLATFORM_ASSERT(x)                  →  assert(x)
 *   std::min(a, b)                       →  (a < b ? a : b)
 */

#include "UndoHistory.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

/* ═══════════════════════════════════════════════════════════════════════
 * ScaledVector
 * ═══════════════════════════════════════════════════════════════════════ */

static const size_t BYTE_MASK = UINT8_MAX;
static const size_t BYTE_BITS = 8;

static size_t ReadValue(const uint8_t *bytes, size_t length) {
    size_t value = 0;
    for (size_t i = 0; i < length; i++)
        value = (value << BYTE_BITS) + bytes[i];
    return value;
}

static void WriteValue(uint8_t *bytes, size_t length, size_t value) {
    while (length != 0) {
        --length;
        bytes[length] = (uint8_t)(value & BYTE_MASK);
        value >>= BYTE_BITS;
    }
}

static SizeMax ElementForValue(size_t value) {
    size_t maxN = BYTE_MASK;
    size_t i = 1;
    while (value > BYTE_MASK) {
        i++;
        value >>= BYTE_BITS;
        maxN = (maxN << BYTE_BITS) + BYTE_MASK;
    }
    SizeMax sm = { i, maxN };
    return sm;
}

void ScaledVector_init(ScaledVector *self) {
    SizeMax sm = { 1, UINT8_MAX };
    self->element  = sm;
    self->bytes    = NULL;
    self->bytesLen = 0;
    self->bytesCap = 0;
}

void ScaledVector_destroy(ScaledVector *self) {
    free(self->bytes);
    self->bytes    = NULL;
    self->bytesLen = 0;
    self->bytesCap = 0;
}

size_t ScaledVector_Size(const ScaledVector *self) {
    return self->bytesLen / self->element.size;
}

size_t ScaledVector_ValueAt(const ScaledVector *self, size_t index) {
    return ReadValue(self->bytes + index * self->element.size, self->element.size);
}

intptr_t ScaledVector_SignedValueAt(const ScaledVector *self, size_t index) {
    return (intptr_t)ReadValue(self->bytes + index * self->element.size, self->element.size);
}

static void SV_EnsureCap(ScaledVector *self, size_t newCap) {
    if (newCap <= self->bytesCap) return;
    size_t cap = self->bytesCap ? self->bytesCap * 2 : 16;
    if (cap < newCap) cap = newCap;
    uint8_t *p = realloc(self->bytes, cap);
    assert(p);
    self->bytes    = p;
    self->bytesCap = cap;
}

void ScaledVector_SetValueAt(ScaledVector *self, size_t index, size_t value) {
    if (value > self->element.maxValue) {
        const SizeMax elemNew = ElementForValue(value);
        const size_t count = self->bytesLen / self->element.size;
        const size_t newBytesLen = elemNew.size * count;
        uint8_t *bytesNew = calloc(newBytesLen, 1);
        assert(bytesNew);
        for (size_t i = 0; i < count; i++) {
            const uint8_t *src = self->bytes + i * self->element.size;
            /* Copy old bytes right-aligned into the new wider slot. */
            uint8_t *dst = bytesNew + (i + 1) * elemNew.size - self->element.size;
            memcpy(dst, src, self->element.size);
        }
        free(self->bytes);
        self->bytes    = bytesNew;
        self->bytesLen = newBytesLen;
        self->bytesCap = newBytesLen;
        self->element  = elemNew;
    }
    WriteValue(self->bytes + index * self->element.size, self->element.size, value);
}

void ScaledVector_ClearValueAt(ScaledVector *self, size_t index) {
    WriteValue(self->bytes + index * self->element.size, self->element.size, 0);
}

void ScaledVector_Clear(ScaledVector *self) {
    self->bytesLen = 0;
}

void ScaledVector_Truncate(ScaledVector *self, size_t length) {
    self->bytesLen = length * self->element.size;
    assert(self->bytesLen <= self->bytesCap);
}

void ScaledVector_ReSize(ScaledVector *self, size_t length) {
    const size_t needed = length * self->element.size;
    SV_EnsureCap(self, needed);
    if (needed > self->bytesLen)
        memset(self->bytes + self->bytesLen, 0, needed - self->bytesLen);
    self->bytesLen = needed;
}

void ScaledVector_PushBack(ScaledVector *self) {
    const size_t needed = self->bytesLen + self->element.size;
    SV_EnsureCap(self, needed);
    memset(self->bytes + self->bytesLen, 0, self->element.size);
    self->bytesLen = needed;
}

size_t ScaledVector_SizeInBytes(const ScaledVector *self) {
    return self->bytesLen;
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoActions
 * ═══════════════════════════════════════════════════════════════════════ */

void UndoActions_init(UndoActions *self) {
    self->types    = NULL;
    self->typesLen = 0;
    self->typesCap = 0;
    ScaledVector_init(&self->positions);
    ScaledVector_init(&self->lengths);
}

void UndoActions_destroy(UndoActions *self) {
    free(self->types);
    self->types    = NULL;
    self->typesLen = 0;
    self->typesCap = 0;
    ScaledVector_destroy(&self->positions);
    ScaledVector_destroy(&self->lengths);
}

void UndoActions_Truncate(UndoActions *self, size_t length) {
    self->typesLen = length;
    ScaledVector_Truncate(&self->positions, length);
    ScaledVector_Truncate(&self->lengths, length);
}

void UndoActions_PushBack(UndoActions *self) {
    if (self->typesLen == self->typesCap) {
        size_t cap = self->typesCap ? self->typesCap * 2 : 16;
        UndoActionType *p = realloc(self->types, cap * sizeof(UndoActionType));
        assert(p);
        self->types    = p;
        self->typesCap = cap;
    }
    UndoActionType zero = { 0, 0 };
    self->types[self->typesLen++] = zero;
    ScaledVector_PushBack(&self->positions);
    ScaledVector_PushBack(&self->lengths);
}

void UndoActions_Clear(UndoActions *self) {
    self->typesLen = 0;
    ScaledVector_Clear(&self->positions);
    ScaledVector_Clear(&self->lengths);
}

intptr_t UndoActions_SSize(const UndoActions *self) {
    return (intptr_t)self->typesLen;
}

void UndoActions_Create(UndoActions *self, size_t index, ActionType at_,
                         SciPosition position_, SciPosition lenData_, int mayCoalesce_) {
    self->types[index].at          = (unsigned int)at_;
    self->types[index].mayCoalesce = (unsigned int)(mayCoalesce_ ? 1 : 0);
    ScaledVector_SetValueAt(&self->positions, index, (size_t)position_);
    ScaledVector_SetValueAt(&self->lengths,   index, (size_t)lenData_);
}

int UndoActions_AtStart(const UndoActions *self, size_t index) {
    if (index == 0) return 1;
    return !self->types[index - 1].mayCoalesce;
}

size_t UndoActions_LengthTo(const UndoActions *self, size_t index) {
    size_t sum = 0;
    for (size_t act = 0; act < index; act++)
        sum += ScaledVector_ValueAt(&self->lengths, act);
    return sum;
}

SciPosition UndoActions_Position(const UndoActions *self, int action) {
    return (SciPosition)ScaledVector_SignedValueAt(&self->positions, (size_t)action);
}

SciPosition UndoActions_Length(const UndoActions *self, int action) {
    return (SciPosition)ScaledVector_SignedValueAt(&self->lengths, (size_t)action);
}

/* ═══════════════════════════════════════════════════════════════════════
 * ScrapStack
 * ═══════════════════════════════════════════════════════════════════════ */

void ScrapStack_init(ScrapStack *self) {
    self->buf     = NULL;
    self->len     = 0;
    self->cap     = 0;
    self->current = 0;
}

void ScrapStack_destroy(ScrapStack *self) {
    free(self->buf);
    self->buf = NULL;
    self->len = self->cap = self->current = 0;
}

void ScrapStack_Clear(ScrapStack *self) {
    self->len     = 0;
    self->current = 0;
}

const char *ScrapStack_Push(ScrapStack *self, const char *text, size_t length) {
    if (self->current < self->len)
        self->len = self->current;
    const size_t needed = self->len + length;
    if (needed > self->cap) {
        size_t cap = self->cap ? self->cap * 2 : 64;
        if (cap < needed) cap = needed;
        char *p = realloc(self->buf, cap);
        assert(p);
        self->buf = p;
        self->cap = cap;
    }
    memcpy(self->buf + self->len, text, length);
    self->len    += length;
    self->current = self->len;
    return self->buf + self->current - length;
}

void ScrapStack_SetCurrent(ScrapStack *self, size_t position) {
    self->current = position;
}

void ScrapStack_MoveForward(ScrapStack *self, size_t length) {
    if ((self->current + length) <= self->len)
        self->current += length;
}

void ScrapStack_MoveBack(ScrapStack *self, size_t length) {
    if (self->current >= length)
        self->current -= length;
}

const char *ScrapStack_CurrentText(const ScrapStack *self) {
    return self->buf + self->current;
}

const char *ScrapStack_TextAt(const ScrapStack *self, size_t position) {
    return self->buf + position;
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — private helper
 * ═══════════════════════════════════════════════════════════════════════ */

static int UH_PreviousAction(const UndoHistory *self) {
    return self->currentAction - 1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

void UndoHistory_init(UndoHistory *self) {
    UndoActions_init(&self->actions);
    self->currentAction     = 0;
    self->undoSequenceDepth = 0;
    self->savePoint         = 0;
    self->tentativePoint    = -1;
    self->detach            = 0;
    self->detach_set        = 0;
    self->memory_act        = 0;
    self->memory_pos        = 0;
    self->memory_set        = 0;
    self->scraps = malloc(sizeof(ScrapStack));
    assert(self->scraps);
    ScrapStack_init(self->scraps);
}


static void UndoHistory_fini(UndoHistory *self) {
    UndoActions_destroy(&self->actions);
    ScrapStack_destroy(self->scraps);
    free(self->scraps);
    self->scraps = NULL;
}

/* CellBuffer stores UndoHistory *uh and calls UndoHistory_destroy(uh),
 * expecting the pointer itself to be freed. */
void UndoHistory_destroy(UndoHistory *self) {
    UndoHistory_fini(self);
    free(self);
}

UndoHistory *UndoHistory_create(void) {
    UndoHistory *self = malloc(sizeof(UndoHistory));
    assert(self);
    UndoHistory_init(self);
    return self;
}

void UndoHistory_destroy_ptr(UndoHistory *self) {
    UndoHistory_destroy(self);
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — core recording
 * ═══════════════════════════════════════════════════════════════════════ */

const char *UndoHistory_AppendAction(UndoHistory *self, ActionType at,
                                      SciPosition position, const char *data,
                                      SciPosition lengthData,
                                      int *startSequence, int mayCoalesce) {
    if (self->currentAction < self->savePoint) {
        self->savePoint = -1;
        if (!self->detach_set) {
            self->detach     = self->currentAction;
            self->detach_set = 1;
        }
    } else if (self->detach_set && (self->detach > self->currentAction)) {
        self->detach = self->currentAction;
    }

    if (self->undoSequenceDepth > 0)
        mayCoalesce = 1;

    int coalesce = 1;
    if (self->currentAction >= 1) {
        int targetAct = self->currentAction - 1;
        if (self->undoSequenceDepth == 0) {
            while ((targetAct > 0) &&
                   ((ActionType)self->actions.types[targetAct].at == ActionType_container) &&
                   self->actions.types[targetAct].mayCoalesce) {
                targetAct--;
            }
            if ((self->currentAction == self->savePoint) ||
                (self->currentAction == self->tentativePoint)) {
                coalesce = 0;
            } else if (!mayCoalesce || !self->actions.types[targetAct].mayCoalesce) {
                coalesce = 0;
            } else if (at == ActionType_container ||
                       (ActionType)self->actions.types[targetAct].at == ActionType_container) {
                /* coalescible container action — keep coalesce = 1 */
            } else if (at != (ActionType)self->actions.types[targetAct].at) {
                coalesce = 0;
            } else if (at == ActionType_insert &&
                       position != (UndoActions_Position(&self->actions, targetAct) +
                                    UndoActions_Length(&self->actions, targetAct))) {
                coalesce = 0;
            } else if (at == ActionType_remove) {
                if (lengthData == 1 || lengthData == 2) {
                    if ((position + lengthData) ==
                        UndoActions_Position(&self->actions, targetAct)) {
                        /* Backspace — OK */
                    } else if (position ==
                               UndoActions_Position(&self->actions, targetAct)) {
                        /* Delete — OK */
                    } else {
                        coalesce = 0;
                    }
                } else {
                    coalesce = 0;
                }
            }
        } else {
            if (!self->actions.types[targetAct].mayCoalesce)
                coalesce = 0;
        }
    } else {
        coalesce = 0;
    }

    *startSequence = !coalesce;
    if ((self->currentAction > 0) && *startSequence)
        self->actions.types[UH_PreviousAction(self)].mayCoalesce = 0;

    const char *dataNew = lengthData
        ? ScrapStack_Push(self->scraps, data, (size_t)lengthData)
        : NULL;

    if (self->currentAction >= UndoActions_SSize(&self->actions)) {
        UndoActions_PushBack(&self->actions);
    } else {
        UndoActions_Truncate(&self->actions, (size_t)(self->currentAction + 1));
    }
    UndoActions_Create(&self->actions, (size_t)self->currentAction,
                       at, position, lengthData, mayCoalesce);
    self->currentAction++;
    return dataNew;
}

void UndoHistory_BeginUndoAction(UndoHistory *self, int mayCoalesce) {
    if (self->undoSequenceDepth == 0) {
        if (self->currentAction > 0)
            self->actions.types[UH_PreviousAction(self)].mayCoalesce =
                (unsigned int)(mayCoalesce ? 1 : 0);
    }
    self->undoSequenceDepth++;
}

void UndoHistory_EndUndoAction(UndoHistory *self) {
    assert(self->undoSequenceDepth > 0);
    self->undoSequenceDepth--;
    if (self->undoSequenceDepth == 0) {
        if (self->currentAction > 0)
            self->actions.types[UH_PreviousAction(self)].mayCoalesce = 0;
    }
}

int UndoHistory_UndoSequenceDepth(const UndoHistory *self) {
    return self->undoSequenceDepth;
}

int UndoHistory_AfterUndoSequenceStart(const UndoHistory *self) {
    if (self->currentAction == 0) return 0;
    return !UndoActions_AtStart(&self->actions, (size_t)(self->currentAction - 1));
}

void UndoHistory_DropUndoSequence(UndoHistory *self) {
    self->undoSequenceDepth = 0;
}

void UndoHistory_DeleteUndoHistory(UndoHistory *self) {
    UndoActions_Clear(&self->actions);
    self->currentAction  = 0;
    self->savePoint      = 0;
    self->tentativePoint = -1;
    ScrapStack_Clear(self->scraps);
    self->memory_set = 0;
}

int UndoHistory_Actions(const UndoHistory *self) {
    return (int)UndoActions_SSize(&self->actions);
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — save point
 * ═══════════════════════════════════════════════════════════════════════ */

void UndoHistory_SetSavePointAction(UndoHistory *self, int action) {
    self->savePoint = action;
}

int UndoHistory_SavePoint(const UndoHistory *self) {
    return self->savePoint;
}

void UndoHistory_SetSavePoint(UndoHistory *self) {
    self->savePoint  = self->currentAction;
    self->detach_set = 0;
}

int UndoHistory_IsSavePoint(const UndoHistory *self) {
    return self->savePoint == self->currentAction;
}

int UndoHistory_BeforeSavePoint(const UndoHistory *self) {
    return (self->savePoint < 0) || (self->savePoint > self->currentAction);
}

int UndoHistory_PreviousBeforeSavePoint(const UndoHistory *self) {
    return (self->savePoint < 0) || (self->savePoint >= self->currentAction);
}

int UndoHistory_BeforeReachableSavePoint(const UndoHistory *self) {
    return (self->savePoint > 0) && (self->savePoint > self->currentAction);
}

int UndoHistory_AfterSavePoint(const UndoHistory *self) {
    return (self->savePoint >= 0) && (self->savePoint <= self->currentAction);
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — detach point
 * ═══════════════════════════════════════════════════════════════════════ */

void UndoHistory_SetDetachPoint(UndoHistory *self, int action) {
    if (action == -1) {
        self->detach_set = 0;
    } else {
        self->detach     = action;
        self->detach_set = 1;
    }
}

int UndoHistory_DetachPoint(const UndoHistory *self) {
    return self->detach_set ? self->detach : -1;
}

int UndoHistory_AfterDetachPoint(const UndoHistory *self) {
    return self->detach_set && (self->detach < self->currentAction);
}

int UndoHistory_AfterOrAtDetachPoint(const UndoHistory *self) {
    return self->detach_set && (self->detach <= self->currentAction);
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — access and navigation
 * ═══════════════════════════════════════════════════════════════════════ */

intptr_t UndoHistory_Delta(const UndoHistory *self, int action) {
    intptr_t sizeChange = 0;
    for (int act = 0; act < action; act++) {
        const intptr_t lengthChange = UndoActions_Length(&self->actions, act);
        sizeChange += ((ActionType)self->actions.types[act].at == ActionType_insert)
                      ? lengthChange : -lengthChange;
    }
    return sizeChange;
}

int UndoHistory_Validate(const UndoHistory *self, intptr_t lengthDocument) {
    const intptr_t sizeChange = UndoHistory_Delta(self, self->currentAction);
    if (sizeChange > lengthDocument) return 0;
    const intptr_t lengthOriginal = lengthDocument - sizeChange;
    intptr_t lengthCurrent = lengthOriginal;
    for (int act = 0; act < UndoActions_SSize(&self->actions); act++) {
        const intptr_t lengthChange = UndoActions_Length(&self->actions, act);
        if (UndoActions_Position(&self->actions, act) > lengthCurrent) return 0;
        lengthCurrent += ((ActionType)self->actions.types[act].at == ActionType_insert)
                         ? lengthChange : -lengthChange;
        if (lengthCurrent < 0) return 0;
    }
    return 1;
}

void UndoHistory_SetCurrent(UndoHistory *self, int action, intptr_t lengthDocument) {
    self->memory_set = 0;
    const size_t lengthSum = UndoActions_LengthTo(&self->actions, (size_t)action);
    ScrapStack_SetCurrent(self->scraps, lengthSum);
    self->currentAction = action;
    if (!UndoHistory_Validate(self, lengthDocument)) {
        self->currentAction = 0;
        UndoHistory_DeleteUndoHistory(self);
        assert(!"UndoHistory::SetCurrent: invalid undo history.");
        abort();
    }
}

int UndoHistory_Current(const UndoHistory *self) {
    return self->currentAction;
}

int UndoHistory_Type(const UndoHistory *self, int action) {
    const int baseType = (int)self->actions.types[action].at;
    const int open = self->actions.types[action].mayCoalesce ? COALESCE_FLAG : 0;
    return baseType | open;
}

SciPosition UndoHistory_Position(const UndoHistory *self, int action) {
    return UndoActions_Position(&self->actions, action);
}

SciPosition UndoHistory_Length(const UndoHistory *self, int action) {
    return UndoActions_Length(&self->actions, action);
}

SciStringView UndoHistory_Text(UndoHistory *self, int action) {
    if (action == 0)
        self->memory_set = 0;
    int    act      = 0;
    size_t position = 0;
    if (self->memory_set && self->memory_act <= action) {
        act      = self->memory_act;
        position = self->memory_pos;
    }
    for (; act < action; act++)
        position += (size_t)UndoActions_Length(&self->actions, act);
    const size_t length = (size_t)UndoActions_Length(&self->actions, action);
    const char *scrap   = ScrapStack_TextAt(self->scraps, position);
    self->memory_act = action;
    self->memory_pos = position;
    self->memory_set = 1;
    SciStringView sv = { scrap, length };
    return sv;
}

void UndoHistory_PushUndoActionType(UndoHistory *self, int type, SciPosition position) {
    UndoActions_PushBack(&self->actions);
    UndoActions_Create(&self->actions, (size_t)(UndoActions_SSize(&self->actions) - 1),
                       (ActionType)(type & (int)UINT8_MAX),
                       position, 0, type & COALESCE_FLAG);
}

void UndoHistory_ChangeLastUndoActionText(UndoHistory *self, size_t length, const char *text) {
    assert(ScaledVector_ValueAt(&self->actions.lengths,
                                (size_t)(UndoActions_SSize(&self->actions) - 1)) == 0);
    ScaledVector_SetValueAt(&self->actions.lengths,
                            (size_t)(UndoActions_SSize(&self->actions) - 1), length);
    ScrapStack_Push(self->scraps, text, length);
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — tentative
 * ═══════════════════════════════════════════════════════════════════════ */

void UndoHistory_SetTentative(UndoHistory *self, int action) {
    self->tentativePoint = action;
}

int UndoHistory_TentativePoint(const UndoHistory *self) {
    return self->tentativePoint;
}

void UndoHistory_TentativeStart(UndoHistory *self) {
    self->tentativePoint = self->currentAction;
}

void UndoHistory_TentativeCommit(UndoHistory *self) {
    self->tentativePoint = -1;
    UndoActions_Truncate(&self->actions, (size_t)self->currentAction);
}

int UndoHistory_TentativeActive(const UndoHistory *self) {
    return self->tentativePoint >= 0;
}

int UndoHistory_TentativeSteps(const UndoHistory *self) {
    if (self->tentativePoint >= 0)
        return self->currentAction - self->tentativePoint;
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * UndoHistory — undo/redo
 * ═══════════════════════════════════════════════════════════════════════ */

int UndoHistory_CanUndo(const UndoHistory *self) {
    return (self->currentAction > 0) && (UndoActions_SSize(&self->actions) != 0);
}

int UndoHistory_StartUndo(const UndoHistory *self) {
    assert(self->currentAction >= 0);
    if (self->currentAction == 0) return 0;
    int act = self->currentAction - 1;
    while (act > 0 && !UndoActions_AtStart(&self->actions, (size_t)act))
        act--;
    return self->currentAction - act;
}

Action UndoHistory_GetUndoStep(const UndoHistory *self) {
    const int prev = UH_PreviousAction(self);
    Action acta;
    acta.at          = (ActionType)self->actions.types[prev].at;
    acta.mayCoalesce = (int)self->actions.types[prev].mayCoalesce;
    acta.position    = UndoActions_Position(&self->actions, prev);
    acta.data        = NULL;
    acta.lenData     = UndoActions_Length(&self->actions, prev);
    if (acta.lenData)
        acta.data = ScrapStack_CurrentText(self->scraps) - acta.lenData;
    return acta;
}

void UndoHistory_CompletedUndoStep(UndoHistory *self) {
    ScrapStack_MoveBack(self->scraps,
        (size_t)UndoActions_Length(&self->actions, UH_PreviousAction(self)));
    self->currentAction--;
}

int UndoHistory_CanRedo(const UndoHistory *self) {
    return UndoActions_SSize(&self->actions) > self->currentAction;
}

int UndoHistory_StartRedo(const UndoHistory *self) {
    if (self->currentAction >= UndoActions_SSize(&self->actions))
        return 0;
    const int maxAction = UndoHistory_Actions(self) - 1;
    int act = self->currentAction;
    while (act <= maxAction && self->actions.types[act].mayCoalesce)
        act++;
    act = (act < maxAction ? act : maxAction);   /* std::min */
    return act - self->currentAction + 1;
}

Action UndoHistory_GetRedoStep(const UndoHistory *self) {
    Action acta;
    acta.at          = (ActionType)self->actions.types[self->currentAction].at;
    acta.mayCoalesce = (int)self->actions.types[self->currentAction].mayCoalesce;
    acta.position    = UndoActions_Position(&self->actions, self->currentAction);
    acta.data        = NULL;
    acta.lenData     = UndoActions_Length(&self->actions, self->currentAction);
    if (acta.lenData)
        acta.data = ScrapStack_CurrentText(self->scraps);
    return acta;
}

void UndoHistory_CompletedRedoStep(UndoHistory *self) {
    ScrapStack_MoveForward(self->scraps,
        (size_t)UndoActions_Length(&self->actions, self->currentAction));
    self->currentAction++;
}
