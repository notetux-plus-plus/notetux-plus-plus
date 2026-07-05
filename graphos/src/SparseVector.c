/* Scintilla source code edit control
 * SparseVector.c — C translation of scintilla/src/SparseVector.h
 *
 * Concrete type: SparseVector_Ptr  (SparseVector<EditionSetOwned> in C++)
 *
 * ── Private helpers ───────────────────────────────────────────────────
 *
 *   ClearValue(self, partition)
 *     C++ original: values.SetValueAt(partition, T())
 *     In C: T() for void* is NULL.  We just overwrite the slot with NULL.
 *     IMPORTANT: the caller must free the old value before calling this
 *     if it was non-NULL.  SparseVector_Ptr never owns the pointed memory.
 */

#include "SparseVector.h"
#include <assert.h>
#include <stdlib.h>

/* ── Private ─────────────────────────────────────────────────────────── */

static void SP_ClearValue(SparseVector_Ptr *self, ptrdiff_t partition) {
    SplitVector_Ptr_SetValueAt(&self->values, partition, NULL);
}

/* ── init ─────────────────────────────────────────────────────────────
 *
 * C++ original constructor:
 *   SparseVector() : empty() { values.InsertEmpty(0, 2); }
 *
 * Partitioning_PD_init() already sets up one partition spanning [0, 0).
 * We insert two empty (NULL) value slots: one for partition 0 and one
 * sentinel at position 1.
 */
void SparseVector_Ptr_init(SparseVector_Ptr *self) {
    Partitioning_PD_init(&self->starts);
    SplitVector_Ptr_init(&self->values);
    SplitVector_Ptr_InsertEmpty(&self->values, 0, 2);
}

/* ── destroy ──────────────────────────────────────────────────────────
 *
 * Releases the Partitioning and SplitVector memory.
 * Does NOT free stored void* values — caller's responsibility.
 */
void SparseVector_Ptr_destroy(SparseVector_Ptr *self) {
    Partitioning_PD_destroy(&self->starts);
    SplitVector_Ptr_destroy(&self->values);
}

/* ── Length ──────────────────────────────────────────────────────────── */
ptrdiff_t SparseVector_Ptr_Length(const SparseVector_Ptr *self) {
    return Partitioning_PD_Length(&self->starts);
}

/* ── Elements ────────────────────────────────────────────────────────── */
ptrdiff_t SparseVector_Ptr_Elements(const SparseVector_Ptr *self) {
    return Partitioning_PD_Partitions(&self->starts);
}

/* ── PositionOfElement ───────────────────────────────────────────────── */
ptrdiff_t SparseVector_Ptr_PositionOfElement(const SparseVector_Ptr *self, ptrdiff_t element) {
    return Partitioning_PD_PositionFromPartition(&self->starts, element);
}

/* ── ElementFromPosition ─────────────────────────────────────────────── */
ptrdiff_t SparseVector_Ptr_ElementFromPosition(const SparseVector_Ptr *self, ptrdiff_t position) {
    if (position < SparseVector_Ptr_Length(self)) {
        return Partitioning_PD_PartitionFromPosition(&self->starts, position);
    } else {
        return Partitioning_PD_Partitions(&self->starts);
    }
}

/* ── ValueAt ─────────────────────────────────────────────────────────────
 *
 * C++ original:
 *   const T& ValueAt(Position position) const noexcept {
 *       const Position partition = ElementFromPosition(position);
 *       const Position startPartition = starts.PositionFromPartition(partition);
 *       if (startPartition == position)
 *           return values.ValueAt(partition);
 *       else
 *           return empty;    // i.e. NULL
 *   }
 */
void *SparseVector_Ptr_ValueAt(const SparseVector_Ptr *self, ptrdiff_t position) {
    assert(position <= SparseVector_Ptr_Length(self));
    const ptrdiff_t partition = SparseVector_Ptr_ElementFromPosition(self, position);
    const ptrdiff_t startPartition = Partitioning_PD_PositionFromPartition(&self->starts, partition);
    if (startPartition == position) {
        return SplitVector_Ptr_ValueAt(&self->values, partition);
    } else {
        return NULL;
    }
}

/* ── Extract ──────────────────────────────────────────────────────────────
 *
 * Moves the value out of the SparseVector at a given position, removes
 * that partition, and returns the value.  The caller takes ownership.
 * Does not operate on the first or last partition.
 *
 * C++ original:
 *   T Extract(Position position) {
 *       const Position partition = ElementFromPosition(position);
 *       T value = std::move(values.operator[](partition));
 *       if ((partition > 0) && (partition < starts.Partitions())) {
 *           starts.RemovePartition(partition);
 *           values.Delete(partition);
 *       }
 *       Check();
 *       return value;
 *   }
 */
void *SparseVector_Ptr_Extract(SparseVector_Ptr *self, ptrdiff_t position) {
    assert(position <= SparseVector_Ptr_Length(self));
    const ptrdiff_t partition = SparseVector_Ptr_ElementFromPosition(self, position);
    assert(partition >= 0);
    assert(partition <= Partitioning_PD_Partitions(&self->starts));
    assert(Partitioning_PD_PositionFromPartition(&self->starts, partition) == position);
    void *value = SplitVector_Ptr_ValueAt(&self->values, partition);
    if ((partition > 0) && (partition < Partitioning_PD_Partitions(&self->starts))) {
        Partitioning_PD_RemovePartition(&self->starts, partition);
        SplitVector_Ptr_Delete(&self->values, partition);
    }
    SparseVector_Ptr_Check(self);
    return value;
}

/* ── SetValueAt ───────────────────────────────────────────────────────────
 *
 * Set the value at position to value.  If value == NULL, this is equivalent
 * to deleting the position (removing the partition).
 *
 * OWNERSHIP NOTE: if the old value at this position is non-NULL and the
 * caller is setting it to NULL or replacing it, the caller must have already
 * freed the old value before calling this function.
 *
 * C++ original:
 *   template <typename ParamType>
 *   void SetValueAt(Position position, ParamType &&value) {
 *       const Position partition = ElementFromPosition(position);
 *       const Position startPartition = starts.PositionFromPartition(partition);
 *       if (value == T()) {                       // value == NULL
 *           if (position == 0 || position == Length()) {
 *               ClearValue(partition);
 *           } else if (position == startPartition) {
 *               ClearValue(partition);
 *               starts.RemovePartition(partition);
 *               values.Delete(partition);
 *           }
 *       } else {
 *           if (position == startPartition) {
 *               ClearValue(partition);
 *               values.SetValueAt(partition, value);
 *           } else {
 *               starts.InsertPartition(partition + 1, position);
 *               values.Insert(partition + 1, value);
 *           }
 *       }
 *   }
 */
void SparseVector_Ptr_SetValueAt(SparseVector_Ptr *self, ptrdiff_t position, void *value) {
    assert(position <= SparseVector_Ptr_Length(self));
    const ptrdiff_t partition = SparseVector_Ptr_ElementFromPosition(self, position);
    const ptrdiff_t startPartition = Partitioning_PD_PositionFromPartition(&self->starts, partition);
    if (value == NULL) {
        if (position == 0 || position == SparseVector_Ptr_Length(self)) {
            SP_ClearValue(self, partition);
        } else if (position == startPartition) {
            SP_ClearValue(self, partition);
            Partitioning_PD_RemovePartition(&self->starts, partition);
            SplitVector_Ptr_Delete(&self->values, partition);
        }
        /* else position is between partitions — already empty, nothing to do */
    } else {
        if (position == startPartition) {
            /* replace existing value — caller already freed old value */
            SP_ClearValue(self, partition);
            SplitVector_Ptr_SetValueAt(&self->values, partition, value);
        } else {
            /* insert new element */
            Partitioning_PD_InsertPartition(&self->starts, partition + 1, position);
            SplitVector_Ptr_Insert(&self->values, partition + 1, value);
        }
    }
}

/* ── InsertSpace ──────────────────────────────────────────────────────────
 *
 * Called when text is inserted into the document at position.
 * Shifts partition boundaries right without changing stored values.
 *
 * C++ original:
 *   void InsertSpace(Position position, Position insertLength) {
 *       const Position partition = starts.PartitionFromPosition(position);
 *       const Position startPartition = starts.PositionFromPartition(partition);
 *       if (startPartition == position) {
 *           const bool positionOccupied = values.ValueAt(partition) != T();
 *           if (partition == 0) {
 *               if (positionOccupied) {
 *                   starts.InsertPartition(1, 0);
 *                   values.InsertEmpty(0, 1);
 *               }
 *               starts.InsertText(partition, insertLength);
 *           } else {
 *               if (positionOccupied) {
 *                   starts.InsertText(partition - 1, insertLength);
 *               } else {
 *                   starts.InsertText(partition, insertLength);
 *               }
 *           }
 *       } else {
 *           starts.InsertText(partition, insertLength);
 *       }
 *   }
 */
void SparseVector_Ptr_InsertSpace(SparseVector_Ptr *self, ptrdiff_t position, ptrdiff_t insertLength) {
    assert(position <= SparseVector_Ptr_Length(self));
    const ptrdiff_t partition = Partitioning_PD_PartitionFromPosition(&self->starts, position);
    const ptrdiff_t startPartition = Partitioning_PD_PositionFromPartition(&self->starts, partition);
    if (startPartition == position) {
        const int positionOccupied = (SplitVector_Ptr_ValueAt(&self->values, partition) != NULL);
        if (partition == 0) {
            if (positionOccupied) {
                Partitioning_PD_InsertPartition(&self->starts, 1, 0);
                SplitVector_Ptr_InsertEmpty(&self->values, 0, 1);
            }
            Partitioning_PD_InsertText(&self->starts, partition, insertLength);
        } else {
            if (positionOccupied) {
                Partitioning_PD_InsertText(&self->starts, partition - 1, insertLength);
            } else {
                Partitioning_PD_InsertText(&self->starts, partition, insertLength);
            }
        }
    } else {
        Partitioning_PD_InsertText(&self->starts, partition, insertLength);
    }
}

/* ── DeletePosition ───────────────────────────────────────────────────────
 *
 * Delete one character position from the document at position.
 * If a value was stored there, it is cleared (caller must have freed it).
 *
 * C++ original:
 *   void DeletePosition(Position position) {
 *       Position partition = starts.PartitionFromPosition(position);
 *       const Position startPartition = starts.PositionFromPartition(partition);
 *       if (startPartition == position) {
 *           if (partition == 0) {
 *               ClearValue(0);
 *               if (starts.PositionFromPartition(1) == 1) {
 *                   if (Elements() > 1) {
 *                       starts.RemovePartition(partition + 1);
 *                       values.Delete(partition);
 *                   }
 *               }
 *           } else if (partition == starts.Partitions()) {
 *               ClearValue(partition);
 *               throw std::runtime_error("SparseVector: deleting end partition.");
 *           } else {
 *               ClearValue(partition);
 *               starts.RemovePartition(partition);
 *               values.Delete(partition);
 *               partition--;
 *           }
 *       }
 *       starts.InsertText(partition, -1);
 *       Check();
 *   }
 */
void SparseVector_Ptr_DeletePosition(SparseVector_Ptr *self, ptrdiff_t position) {
    assert(position < SparseVector_Ptr_Length(self));
    ptrdiff_t partition = Partitioning_PD_PartitionFromPosition(&self->starts, position);
    const ptrdiff_t startPartition = Partitioning_PD_PositionFromPartition(&self->starts, partition);
    if (startPartition == position) {
        if (partition == 0) {
            SP_ClearValue(self, 0);
            if (Partitioning_PD_PositionFromPartition(&self->starts, 1) == 1) {
                if (SparseVector_Ptr_Elements(self) > 1) {
                    Partitioning_PD_RemovePartition(&self->starts, partition + 1);
                    SplitVector_Ptr_Delete(&self->values, partition);
                }
            }
        } else if (partition == Partitioning_PD_Partitions(&self->starts)) {
            /* Should not be possible */
            SP_ClearValue(self, partition);
            assert(!"SparseVector: deleting end partition.");
            abort();
        } else {
            SP_ClearValue(self, partition);
            Partitioning_PD_RemovePartition(&self->starts, partition);
            SplitVector_Ptr_Delete(&self->values, partition);
            partition--;
        }
    }
    Partitioning_PD_InsertText(&self->starts, partition, -1);
    SparseVector_Ptr_Check(self);
}

/* ── DeleteAll ────────────────────────────────────────────────────────────
 *
 * Reset to empty state.  DOES NOT free stored void* values — caller must
 * have freed them first.
 *
 * C++ original:
 *   void DeleteAll() {
 *       starts = Partitioning<Sci::Position>();
 *       values = SplitVector<T>();
 *       values.InsertEmpty(0, 2);
 *   }
 */
void SparseVector_Ptr_DeleteAll(SparseVector_Ptr *self) {
    Partitioning_PD_destroy(&self->starts);
    Partitioning_PD_init(&self->starts);
    SplitVector_Ptr_destroy(&self->values);
    SplitVector_Ptr_init(&self->values);
    SplitVector_Ptr_InsertEmpty(&self->values, 0, 2);
}

/* ── DeleteRange ──────────────────────────────────────────────────────────
 *
 * Delete a range of positions.  Values within the range are cleared
 * (set to NULL); caller must have freed them first.
 *
 * C++ original:
 *   void DeleteRange(Position position, Position deleteLength) {
 *       if (position > Length() || (deleteLength == 0)) return;
 *       const Position positionEnd = position + deleteLength;
 *       if (position == 0) {
 *           while ((Elements() > 1) &&
 *                  (starts.PositionFromPartition(1) <= deleteLength)) {
 *               starts.RemovePartition(1);
 *               values.Delete(0);
 *           }
 *           starts.InsertText(0, -deleteLength);
 *           if (Length() == 0) ClearValue(0);
 *       } else {
 *           const Position partition = starts.PartitionFromPosition(position);
 *           const bool atPartitionStart =
 *               position == starts.PositionFromPartition(partition);
 *           const Position partitionDelete = partition + (atPartitionStart ? 0 : 1);
 *           for (;;) {
 *               const Position positionAtIndex =
 *                   starts.PositionFromPartition(partitionDelete);
 *               if (positionAtIndex >= positionEnd) break;
 *               starts.RemovePartition(partitionDelete);
 *               values.Delete(partitionDelete);
 *           }
 *           starts.InsertText(partition - (atPartitionStart ? 1 : 0), -deleteLength);
 *       }
 *       Check();
 *   }
 */
void SparseVector_Ptr_DeleteRange(SparseVector_Ptr *self, ptrdiff_t position, ptrdiff_t deleteLength) {
    if (position > SparseVector_Ptr_Length(self) || (deleteLength == 0))
        return;
    const ptrdiff_t positionEnd = position + deleteLength;
    assert(positionEnd <= SparseVector_Ptr_Length(self));
    if (position == 0) {
        while ((SparseVector_Ptr_Elements(self) > 1) &&
               (Partitioning_PD_PositionFromPartition(&self->starts, 1) <= deleteLength)) {
            Partitioning_PD_RemovePartition(&self->starts, 1);
            SplitVector_Ptr_Delete(&self->values, 0);
        }
        Partitioning_PD_InsertText(&self->starts, 0, -deleteLength);
        if (SparseVector_Ptr_Length(self) == 0)
            SP_ClearValue(self, 0);
    } else {
        const ptrdiff_t partition = Partitioning_PD_PartitionFromPosition(&self->starts, position);
        const int atPartitionStart =
            (position == Partitioning_PD_PositionFromPartition(&self->starts, partition));
        const ptrdiff_t partitionDelete = partition + (atPartitionStart ? 0 : 1);
        assert(partitionDelete > 0);
        for (;;) {
            const ptrdiff_t positionAtIndex =
                Partitioning_PD_PositionFromPartition(&self->starts, partitionDelete);
            assert(position <= positionAtIndex);
            if (positionAtIndex >= positionEnd)
                break;
            assert(partitionDelete <= SparseVector_Ptr_Elements(self));
            Partitioning_PD_RemovePartition(&self->starts, partitionDelete);
            SplitVector_Ptr_Delete(&self->values, partitionDelete);
        }
        Partitioning_PD_InsertText(&self->starts, partition - (atPartitionStart ? 1 : 0), -deleteLength);
    }
    SparseVector_Ptr_Check(self);
}

/* ── PositionNext ─────────────────────────────────────────────────────────
 *
 * Returns the position of the element after 'start', or Length()+1 when
 * past the end (sentinel value to terminate iteration loops).
 *
 * C++ original:
 *   Position PositionNext(Position start) const noexcept {
 *       const Position element = ElementFromPosition(start);
 *       if (element < Elements())
 *           return PositionOfElement(element + 1);
 *       return Length() + 1;
 *   }
 */
ptrdiff_t SparseVector_Ptr_PositionNext(const SparseVector_Ptr *self, ptrdiff_t start) {
    const ptrdiff_t element = SparseVector_Ptr_ElementFromPosition(self, start);
    if (element < SparseVector_Ptr_Elements(self)) {
        return SparseVector_Ptr_PositionOfElement(self, element + 1);
    }
    return SparseVector_Ptr_Length(self) + 1;
}

/* ── IndexAfter ───────────────────────────────────────────────────────────
 *
 * Returns the index of the partition that starts after position.
 *
 * C++ original:
 *   Position IndexAfter(Position position) const noexcept {
 *       if (position < 0) return 0;
 *       const Position partition = starts.PartitionFromPosition(position);
 *       return partition + 1;
 *   }
 */
ptrdiff_t SparseVector_Ptr_IndexAfter(const SparseVector_Ptr *self, ptrdiff_t position) {
    assert(position < SparseVector_Ptr_Length(self));
    if (position < 0)
        return 0;
    const ptrdiff_t partition = Partitioning_PD_PartitionFromPosition(&self->starts, position);
    return partition + 1;
}

/* ── Check ────────────────────────────────────────────────────────────────
 *
 * Debug-mode consistency check.
 * throw std::runtime_error → assert(0); abort()
 */
void SparseVector_Ptr_Check(const SparseVector_Ptr *self) {
#ifdef CHECK_CORRECTNESS
    Partitioning_PD_Check(&self->starts);
    if (Partitioning_PD_Partitions(&self->starts) !=
        SplitVector_Ptr_Length(&self->values) - 1) {
        assert(!"SparseVector: Partitions and values different lengths.");
        abort();
    }
#else
    (void)self;
#endif
}
