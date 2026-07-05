/* Scintilla source code edit control
 * SparseVector.h — C translation of scintilla/src/SparseVector.h
 *
 * SparseVector stores values at specific positions within a range, leaving
 * all other positions "empty" (NULL / zero).  Unlike RunStyles, which stores
 * a value for every contiguous run, SparseVector is optimised for the case
 * where values occur at individual positions rather than ranges.
 *
 * Internally it holds:
 *   starts  — Partitioning_PD  : the positions where a value exists
 *   values  — SplitVector_Ptr  : the value at each such position + a sentinel
 *
 * The first and last slots always exist (positions 0 and Length()), making
 * the element count always >= 1.
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   template <typename T>                →  single concrete type SparseVector_Ptr
 *   T empty                              →  NULL  (no struct field needed)
 *   SplitVector<T>                       →  SplitVector_Ptr   (void * elements)
 *   Partitioning<Sci::Position>          →  Partitioning_PD
 *
 *   The only instantiation in Scintilla is SparseVector<EditionSetOwned>
 *   where EditionSetOwned = std::unique_ptr<std::vector<EditionCount>>.
 *   In C this is a plain void * (owned pointer to a heap-allocated struct).
 *
 *   Ownership: SparseVector_Ptr does NOT free stored pointers — the caller
 *   (ChangeHistory) must free any non-NULL value before overwriting or
 *   deleting it.  This mirrors the C++ unique_ptr semantics but makes
 *   ownership explicit rather than automatic.
 *
 *   const T& ValueAt()  →  void *   (NULL when no element at position)
 *   T Extract()         →  void *   (removes from SparseVector, caller owns)
 *   T()                 →  NULL
 *   value == T()        →  value == NULL
 *   throw               →  assert(0); abort()
 */

#ifndef SPARSEVECTOR_C_H
#define SPARSEVECTOR_C_H

#include <stddef.h>
#include "SplitVector.h"
#include "Partitioning.h"

/* ── SparseVector_Ptr ────────────────────────────────────────────────── */

typedef struct {
    Partitioning_PD  starts;
    SplitVector_Ptr  values;
} SparseVector_Ptr;

/* Lifecycle */
void       SparseVector_Ptr_init    (SparseVector_Ptr *self);
void       SparseVector_Ptr_destroy (SparseVector_Ptr *self);

/* Queries */
ptrdiff_t  SparseVector_Ptr_Length            (const SparseVector_Ptr *self);
ptrdiff_t  SparseVector_Ptr_Elements          (const SparseVector_Ptr *self);
ptrdiff_t  SparseVector_Ptr_PositionOfElement (const SparseVector_Ptr *self, ptrdiff_t element);
ptrdiff_t  SparseVector_Ptr_ElementFromPosition(const SparseVector_Ptr *self, ptrdiff_t position);
void      *SparseVector_Ptr_ValueAt           (const SparseVector_Ptr *self, ptrdiff_t position);

/* Mutation */
void      *SparseVector_Ptr_Extract      (SparseVector_Ptr *self, ptrdiff_t position);
void       SparseVector_Ptr_SetValueAt   (SparseVector_Ptr *self, ptrdiff_t position, void *value);
void       SparseVector_Ptr_InsertSpace  (SparseVector_Ptr *self, ptrdiff_t position, ptrdiff_t insertLength);
void       SparseVector_Ptr_DeletePosition(SparseVector_Ptr *self, ptrdiff_t position);
void       SparseVector_Ptr_DeleteAll    (SparseVector_Ptr *self);
void       SparseVector_Ptr_DeleteRange  (SparseVector_Ptr *self, ptrdiff_t position, ptrdiff_t deleteLength);

/* Navigation */
ptrdiff_t  SparseVector_Ptr_PositionNext (const SparseVector_Ptr *self, ptrdiff_t start);
ptrdiff_t  SparseVector_Ptr_IndexAfter   (const SparseVector_Ptr *self, ptrdiff_t position);

/* Debug */
void       SparseVector_Ptr_Check (const SparseVector_Ptr *self);

#endif /* SPARSEVECTOR_C_H */
