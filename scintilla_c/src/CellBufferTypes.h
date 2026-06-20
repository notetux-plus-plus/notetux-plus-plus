/* CellBufferTypes.h — shared types used by CellBuffer, UndoHistory, ChangeHistory
 *
 * Extracted to avoid circular inclusion between CellBuffer.h and UndoHistory.h.
 *
 * ── C++ → C translations ─────────────────────────────────────────────
 *
 *   enum class ActionType : unsigned char { insert, remove, container }
 *   → typedef enum (unsigned char underlying type emulated with uint8_t values)
 *
 *   struct Action { ActionType at; bool mayCoalesce; Sci::Position position;
 *                   const char *data; Sci::Position lenData; }
 *   → same in C with C-compatible types
 *
 *   std::string_view UndoHistory::Text(int action)
 *   → SciStringView  (const char *ptr, size_t len)
 *   std::string_view maps to a fat pointer; C has no equivalent built in.
 */

#ifndef CELLBUFFERTYPES_C_H
#define CELLBUFFERTYPES_C_H

#include <stddef.h>   /* size_t */
#include "Position.h"

/* ── ActionType ──────────────────────────────────────────────────────
 *
 * C++ original:
 *   enum class ActionType : unsigned char { insert, remove, container };
 *
 * We add a _ActionType_ prefix to avoid clashes with any POSIX identifiers.
 */
typedef enum {
    ActionType_insert    = 0,
    ActionType_remove    = 1,
    ActionType_container = 2
} ActionType;

/* ── Action ──────────────────────────────────────────────────────────
 *
 * C++ original:
 *   struct Action {
 *       ActionType at = ActionType::insert;
 *       bool mayCoalesce = false;
 *       Sci::Position position = 0;
 *       const char *data = nullptr;
 *       Sci::Position lenData = 0;
 *   };
 */
typedef struct {
    ActionType  at;
    int         mayCoalesce;    /* bool → int */
    SciPosition position;
    const char *data;
    SciPosition lenData;
} Action;

/* ── SciStringView ───────────────────────────────────────────────────
 *
 * C equivalent of std::string_view.  The pointer is NOT NUL-terminated
 * unless the caller guarantees it.
 */
typedef struct {
    const char *ptr;
    size_t      len;
} SciStringView;

/* ── SplitView ───────────────────────────────────────────────────────
 *
 * C++ original:
 *   struct SplitView {
 *       const char *segment1 = nullptr; size_t length1 = 0;
 *       const char *segment2 = nullptr; size_t length = 0;
 *       bool operator==(…) / bool operator!=(…)
 *       char CharAt(size_t position) const noexcept { … }
 *   };
 *
 * operator== / operator!= → SplitView_equal / SplitView_notequal helpers
 * CharAt → static inline SplitView_CharAt
 */
typedef struct {
    const char *segment1;
    size_t      length1;
    const char *segment2;
    size_t      length;   /* total length (segment1 + segment2) */
} SplitView;

static inline int SplitView_equal(const SplitView *a, const SplitView *b) {
    return a->segment1 == b->segment1 && a->length1 == b->length1 &&
           a->segment2 == b->segment2 && a->length == b->length;
}

static inline char SplitView_CharAt(const SplitView *sv, size_t position) {
    if (position < sv->length1)
        return sv->segment1[position];
    if (position < sv->length)
        return sv->segment2[position];
    return 0;
}

#endif /* CELLBUFFERTYPES_C_H */
