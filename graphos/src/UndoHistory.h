/* Scintilla source code edit control
 * UndoHistory.h — C translation of scintilla/src/UndoHistory.h
 *
 * ── Type map ─────────────────────────────────────────────────────────
 *
 *   C++ type                       C equivalent
 *   ──────────────────────────     ──────────────────────────────────
 *   struct SizeMax                 SizeMax  (trivial struct, unchanged)
 *   class ScaledVector             ScaledVector + ScaledVector_* fns
 *     std::vector<uint8_t> bytes   uint8_t *bytes; size_t bytesLen; cap
 *   class UndoActionType           UndoActionType  (bit-field struct)
 *     ActionType at : 4            unsigned int at : 4
 *     bool mayCoalesce : 1         unsigned int mayCoalesce : 1
 *   struct UndoActions             UndoActions + UndoActions_* fns
 *     std::vector<UndoActionType>  UndoActionType *types; len; cap
 *   class ScrapStack               ScrapStack + ScrapStack_* fns
 *     std::string stack            char *buf; size_t len; cap
 *   class UndoHistory              UndoHistory + UndoHistory_* fns
 *     std::optional<int> detach    int detach; int detach_set
 *     std::unique_ptr<ScrapStack>  ScrapStack *scraps  (heap-alloc)
 *     std::optional<actPos> memory int memory_act; size_t memory_pos; int memory_set
 *
 *   std::string_view Text()        SciStringView UndoHistory_Text()
 *   bool &startSequence param      int *startSequence
 *   bool mayCoalesce=true          int mayCoalesce (1=true default)
 *   throw std::runtime_error(…)    assert(!"…"); abort()
 *   PLATFORM_ASSERT(x)             assert(x)
 *   constexpr int coalesceFlag     #define COALESCE_FLAG 0x100
 */

#ifndef UNDOHISTORY_C_H
#define UNDOHISTORY_C_H

#include <stddef.h>
#include <stdint.h>
#include "CellBufferTypes.h"   /* ActionType, Action, SciPosition, SciStringView */

#define COALESCE_FLAG 0x100    /* mirrors C++ constexpr int coalesceFlag = 0x100 */

/* ── SizeMax ──────────────────────────────────────────────────────────
 *
 * Describes the current element width of a ScaledVector:
 *   size     — bytes per element (1, 2, 4 or 8)
 *   maxValue — largest value that fits in 'size' bytes
 */
typedef struct {
    size_t size;
    size_t maxValue;
} SizeMax;

/* ── ScaledVector ─────────────────────────────────────────────────────
 *
 * A variable-width unsigned integer array.  All elements share the same
 * width, which grows automatically when a value that doesn't fit is stored.
 * Start at 1 byte/element; expand to 2, 4, 8 as needed.
 * This is a memory optimisation: most undo histories fit in 1–2 bytes/element.
 *
 * C++ bytes.size()  → bytesLen  (total bytes used = count × element.size)
 * C++ bytes.data()  → bytes ptr
 */
typedef struct {
    SizeMax  element;    /* current element width + max representable value */
    uint8_t *bytes;      /* heap buffer, bytesLen bytes used, bytesCap allocated */
    size_t   bytesLen;
    size_t   bytesCap;
} ScaledVector;

void      ScaledVector_init         (ScaledVector *self);
void      ScaledVector_destroy      (ScaledVector *self);
size_t    ScaledVector_Size         (const ScaledVector *self);
size_t    ScaledVector_ValueAt      (const ScaledVector *self, size_t index);
intptr_t  ScaledVector_SignedValueAt(const ScaledVector *self, size_t index);
void      ScaledVector_SetValueAt   (ScaledVector *self, size_t index, size_t value);
void      ScaledVector_ClearValueAt (ScaledVector *self, size_t index);
void      ScaledVector_Clear        (ScaledVector *self);
void      ScaledVector_Truncate     (ScaledVector *self, size_t length);
void      ScaledVector_ReSize       (ScaledVector *self, size_t length);
void      ScaledVector_PushBack     (ScaledVector *self);
size_t    ScaledVector_SizeInBytes  (const ScaledVector *self);

/* ── UndoActionType ───────────────────────────────────────────────────
 *
 * C++ original:
 *   class UndoActionType {
 *     ActionType at : 4;
 *     bool mayCoalesce : 1;
 *   };
 *
 * Bit-fields use unsigned int (C99 §6.7.2.1) — 'bool' bit-fields are
 * not portable in C.  Values of ActionType enum fit in 4 bits.
 */
typedef struct {
    unsigned int at          : 4;
    unsigned int mayCoalesce : 1;
} UndoActionType;

/* ── UndoActions ──────────────────────────────────────────────────────
 *
 * Parallel arrays for a sequence of undo actions:
 *   types     — one UndoActionType per action
 *   positions — document position of each action (ScaledVector)
 *   lengths   — byte length of each action's text (ScaledVector)
 *
 * C++ std::vector<UndoActionType> types  →  UndoActionType *types; len; cap
 */
typedef struct {
    UndoActionType *types;
    size_t          typesLen;
    size_t          typesCap;
    ScaledVector    positions;
    ScaledVector    lengths;
} UndoActions;

void        UndoActions_init     (UndoActions *self);
void        UndoActions_destroy  (UndoActions *self);
void        UndoActions_Truncate (UndoActions *self, size_t length);
void        UndoActions_PushBack (UndoActions *self);
void        UndoActions_Clear    (UndoActions *self);
intptr_t    UndoActions_SSize    (const UndoActions *self);
void        UndoActions_Create   (UndoActions *self, size_t index, ActionType at_,
                                   SciPosition position_, SciPosition lenData_, int mayCoalesce_);
int         UndoActions_AtStart  (const UndoActions *self, size_t index);
size_t      UndoActions_LengthTo (const UndoActions *self, size_t index);
SciPosition UndoActions_Position (const UndoActions *self, int action);
SciPosition UndoActions_Length   (const UndoActions *self, int action);

/* ── ScrapStack ───────────────────────────────────────────────────────
 *
 * A growable byte buffer used as a stack of text scraps (the actual
 * character data for each undo action).
 *
 * C++ std::string stack → char *buf; size_t len; size_t cap
 * The 'current' cursor points into the buffer and advances/retreats
 * as undo/redo steps are replayed.
 */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
    size_t current;
} ScrapStack;

void        ScrapStack_init       (ScrapStack *self);
void        ScrapStack_destroy    (ScrapStack *self);
void        ScrapStack_Clear      (ScrapStack *self);
const char *ScrapStack_Push       (ScrapStack *self, const char *text, size_t length);
void        ScrapStack_SetCurrent (ScrapStack *self, size_t position);
void        ScrapStack_MoveForward(ScrapStack *self, size_t length);
void        ScrapStack_MoveBack   (ScrapStack *self, size_t length);
const char *ScrapStack_CurrentText(const ScrapStack *self);
const char *ScrapStack_TextAt     (const ScrapStack *self, size_t position);

/* ── UndoHistory ──────────────────────────────────────────────────────
 *
 * The main undo/redo controller.  Owns an UndoActions sequence and a
 * ScrapStack.
 *
 * std::optional<int> detach      →  int detach; int detach_set
 * std::unique_ptr<ScrapStack>    →  ScrapStack *scraps  (heap via malloc)
 * std::optional<actPos> memory   →  int memory_act; size_t memory_pos; int memory_set
 *
 * Default field values match the C++ constructor:
 *   currentAction = 0, undoSequenceDepth = 0, savePoint = 0,
 *   tentativePoint = -1, detach_set = 0, memory_set = 0
 */
typedef struct {
    UndoActions actions;
    int         currentAction;
    int         undoSequenceDepth;
    int         savePoint;
    int         tentativePoint;
    int         detach;          /* value when detach_set != 0 */
    int         detach_set;      /* 1 = optional has value */
    ScrapStack *scraps;          /* heap-allocated, never NULL after init */
    int         memory_act;      /* cached position hint for Text() */
    size_t      memory_pos;
    int         memory_set;
} UndoHistory;

/* Lifecycle — embedded (init/destroy work on a caller-allocated struct) */
void UndoHistory_init   (UndoHistory *self);
void UndoHistory_destroy(UndoHistory *self);

/* Lifecycle — heap (used by CellBuffer which stores UndoHistory *uh) */
UndoHistory *UndoHistory_create (void);
void         UndoHistory_destroy_ptr(UndoHistory *self);

/* Core recording */
const char *UndoHistory_AppendAction(UndoHistory *self, ActionType at,
                                      SciPosition position, const char *data,
                                      SciPosition lengthData,
                                      int *startSequence, int mayCoalesce);
void UndoHistory_BeginUndoAction        (UndoHistory *self, int mayCoalesce);
void UndoHistory_EndUndoAction          (UndoHistory *self);
int  UndoHistory_UndoSequenceDepth      (const UndoHistory *self);
int  UndoHistory_AfterUndoSequenceStart (const UndoHistory *self);
void UndoHistory_DropUndoSequence       (UndoHistory *self);
void UndoHistory_DeleteUndoHistory      (UndoHistory *self);
int  UndoHistory_Actions                (const UndoHistory *self);

/* Save point */
void UndoHistory_SetSavePointAction      (UndoHistory *self, int action);
int  UndoHistory_SavePoint               (const UndoHistory *self);
void UndoHistory_SetSavePoint            (UndoHistory *self);
int  UndoHistory_IsSavePoint             (const UndoHistory *self);
int  UndoHistory_BeforeSavePoint         (const UndoHistory *self);
int  UndoHistory_PreviousBeforeSavePoint (const UndoHistory *self);
int  UndoHistory_BeforeReachableSavePoint(const UndoHistory *self);
int  UndoHistory_AfterSavePoint          (const UndoHistory *self);

/* Detach point */
void UndoHistory_SetDetachPoint      (UndoHistory *self, int action);
int  UndoHistory_DetachPoint         (const UndoHistory *self);
int  UndoHistory_AfterDetachPoint    (const UndoHistory *self);
int  UndoHistory_AfterOrAtDetachPoint(const UndoHistory *self);

/* Access */
intptr_t      UndoHistory_Delta    (const UndoHistory *self, int action);
int           UndoHistory_Validate (const UndoHistory *self, intptr_t lengthDocument);
void          UndoHistory_SetCurrent(UndoHistory *self, int action, intptr_t lengthDocument);
int           UndoHistory_Current  (const UndoHistory *self);
int           UndoHistory_Type     (const UndoHistory *self, int action);
SciPosition   UndoHistory_Position (const UndoHistory *self, int action);
SciPosition   UndoHistory_Length   (const UndoHistory *self, int action);
SciStringView UndoHistory_Text     (UndoHistory *self, int action);
void          UndoHistory_PushUndoActionType    (UndoHistory *self, int type, SciPosition position);
void          UndoHistory_ChangeLastUndoActionText(UndoHistory *self, size_t length, const char *text);

/* Tentative */
void UndoHistory_SetTentative   (UndoHistory *self, int action);
int  UndoHistory_TentativePoint (const UndoHistory *self);
void UndoHistory_TentativeStart (UndoHistory *self);
void UndoHistory_TentativeCommit(UndoHistory *self);
int  UndoHistory_TentativeActive(const UndoHistory *self);
int  UndoHistory_TentativeSteps (const UndoHistory *self);

/* Undo/redo */
int    UndoHistory_CanUndo          (const UndoHistory *self);
int    UndoHistory_StartUndo        (const UndoHistory *self);
Action UndoHistory_GetUndoStep      (const UndoHistory *self);
void   UndoHistory_CompletedUndoStep(UndoHistory *self);
int    UndoHistory_CanRedo          (const UndoHistory *self);
int    UndoHistory_StartRedo        (const UndoHistory *self);
Action UndoHistory_GetRedoStep      (const UndoHistory *self);
void   UndoHistory_CompletedRedoStep(UndoHistory *self);

#endif /* UNDOHISTORY_C_H */
