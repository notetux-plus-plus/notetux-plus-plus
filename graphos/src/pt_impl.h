/* pt_impl.h — Partitioning implementation template
 *
 * Include with PT_T, PT_SUFFIX, PT_SV defined.
 * PT_STRUCT and PT_FN are defined by the including .c file.
 *
 * Companion SplitVector accessor macros are generated from PT_SV:
 *   PT_SV_FN(fn)  →  SplitVector_Int_fn  or  SplitVector_PD_fn
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   body[middle]
 *   → PT_SV_FN(ValueAt)(&self->body, middle)
 *   (operator[] returns a const ref; in C ValueAt returns by value)
 *
 *   &body[position]
 *   → (PT_T *)PT_SV_FN(ElementPointer)(&self->body, position)
 *   Used in RangeAddDelta for the pointer-walk.  ElementPointer does NOT
 *   rearrange the gap, so caller must handle the two-segment structure.
 *   RangeAddDelta accounts for this (it mirrors the original C++ which
 *   also reads across both gap segments via two pointer-walk loops).
 *
 *   PLATFORM_ASSERT → assert()
 *   throw std::runtime_error → assert(0) (debug-only Check())
 */

#ifndef PT_T
#  error "pt_impl.h requires PT_T defined"
#endif

#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* ── Helper: SV function name from PT_SV ────────────────────────────── */
#define _PT_CAT2(a,b) a##b
#define _PT_CAT(a,b)  _PT_CAT2(a,b)
#define PT_SV_FN(fn)  _PT_CAT(PT_SV, _PT_CAT(_, fn))

/* ── Internal: add delta to all values in a raw range ────────────────
 *
 * C++ original:
 *   void RangeAddDelta(T start, T end, T delta) noexcept { ... }
 *
 * The original walked across the two physical segments of the SplitVector
 * by obtaining a pointer via &body[i] and advancing it. In C we replicate
 * this two-segment walk: one loop for elements before the gap, one for
 * elements after. ElementPointer() gives an unmanaged pointer into the
 * raw storage without moving the gap.
 *
 * Note: 'end' is exclusive (one-past-end), matching C++ convention.
 */
static void PT_FN(RangeAddDelta)(PT_STRUCT *self, PT_T start, PT_T end, PT_T delta) {
    const ptrdiff_t position      = (ptrdiff_t)start;
    const ptrdiff_t rangeLength   = (ptrdiff_t)(end - start);
    const ptrdiff_t gapPos        = PT_SV_FN(GapPosition)(&self->body);
    const ptrdiff_t part1Left     = gapPos - position;
    ptrdiff_t range1Length = rangeLength;
    if (range1Length > part1Left)
        range1Length = part1Left;
    if (range1Length < 0) range1Length = 0;

    /* First segment: before the gap */
    if (range1Length > 0) {
        PT_T *writer = (PT_T *)PT_SV_FN(ElementPointer)(&self->body, position);
        for (ptrdiff_t i = 0; i < range1Length; i++)
            writer[i] += delta;
    }
    /* Second segment: after the gap */
    ptrdiff_t remaining = rangeLength - range1Length;
    if (remaining > 0) {
        ptrdiff_t pos2 = position + range1Length;
        PT_T *writer2 = (PT_T *)PT_SV_FN(ElementPointer)(&self->body, pos2);
        for (ptrdiff_t i = 0; i < remaining; i++)
            writer2[i] += delta;
    }
}

/* ── Internal: move step forward ─────────────────────────────────────
 *
 * C++ original:
 *   void ApplyStep(T partitionUpTo) noexcept { ... }
 */
static void PT_FN(ApplyStep)(PT_STRUCT *self, PT_T partitionUpTo) {
    if (self->stepLength != 0) {
        PT_FN(RangeAddDelta)(self, self->stepPartition + 1, partitionUpTo + 1,
                              self->stepLength);
    }
    self->stepPartition = partitionUpTo;
    if (self->stepPartition >= PT_FN(Partitions)(self)) {
        self->stepPartition = PT_FN(Partitions)(self);
        self->stepLength    = 0;
    }
}

/* ── Internal: move step backward ────────────────────────────────────
 *
 * C++ original:
 *   void BackStep(T partitionDownTo) noexcept { ... }
 */
static void PT_FN(BackStep)(PT_STRUCT *self, PT_T partitionDownTo) {
    if (self->stepLength != 0) {
        PT_FN(RangeAddDelta)(self, partitionDownTo + 1, self->stepPartition + 1,
                              -self->stepLength);
    }
    self->stepPartition = partitionDownTo;
}

/* ── init ─────────────────────────────────────────────────────────────
 *
 * C++ original: explicit Partitioning(size_t growSize=8) :
 *   stepPartition(0), stepLength(0), body(growSize) {
 *     body.Insert(0, 0);   // stays 0 forever
 *     body.Insert(1, 0);   // end of first partition
 * }
 */
void PT_FN(init)(PT_STRUCT *self) {
    self->stepPartition = 0;
    self->stepLength    = 0;
    PT_SV_FN(init)(&self->body);
    PT_SV_FN(Insert)(&self->body, 0, (PT_T)0);
    PT_SV_FN(Insert)(&self->body, 1, (PT_T)0);
}

/* ── destroy ───────────────────────────────────────────────────────── */
void PT_FN(destroy)(PT_STRUCT *self) {
    PT_SV_FN(destroy)(&self->body);
}

/* ── Partitions ──────────────────────────────────────────────────────
 *
 * C++ original:
 *   T Partitions() const noexcept { return static_cast<T>(body.Length())-1; }
 */
PT_T PT_FN(Partitions)(const PT_STRUCT *self) {
    return (PT_T)(PT_SV_FN(Length)(&self->body) - 1);
}

/* ── ReAllocate ─────────────────────────────────────────────────────── */
void PT_FN(ReAllocate)(PT_STRUCT *self, ptrdiff_t newSize) {
    PT_SV_FN(ReAllocate)(&self->body, (size_t)(newSize + 1));
}

/* ── Length ───────────────────────────────────────────────────────────
 *
 * C++ original:
 *   T Length() const noexcept { return PositionFromPartition(Partitions()); }
 */
PT_T PT_FN(Length)(const PT_STRUCT *self) {
    return PT_FN(PositionFromPartition)(self, PT_FN(Partitions)(self));
}

/* ── InsertPartition ─────────────────────────────────────────────────
 *
 * C++ original:
 *   void InsertPartition(T partition, T pos) {
 *       if (stepPartition < partition) { ApplyStep(partition); }
 *       body.Insert(partition, pos);
 *       stepPartition++;
 *   }
 */
void PT_FN(InsertPartition)(PT_STRUCT *self, PT_T partition, PT_T pos) {
    if (self->stepPartition < partition)
        PT_FN(ApplyStep)(self, partition);
    PT_SV_FN(Insert)(&self->body, (ptrdiff_t)partition, pos);
    self->stepPartition++;
}

/* ── InsertPartitions ────────────────────────────────────────────────
 *
 * C++ original:
 *   void InsertPartitions(T partition, const T *positions, size_t length) {
 *       if (stepPartition < partition) { ApplyStep(partition); }
 *       body.InsertFromArray(partition, positions, 0, length);
 *       stepPartition += static_cast<T>(length);
 *   }
 */
void PT_FN(InsertPartitions)(PT_STRUCT *self, PT_T partition,
                               const PT_T *positions, size_t length) {
    if (self->stepPartition < partition)
        PT_FN(ApplyStep)(self, partition);
    PT_SV_FN(InsertFromArray)(&self->body, (ptrdiff_t)partition,
                               positions, 0, (ptrdiff_t)length);
    self->stepPartition += (PT_T)length;
}

/* ── InsertPartitionsWithCast ─────────────────────────────────────────
 *
 * Used for 64-bit builds when T is 32-bits.
 *
 * C++ original:
 *   void InsertPartitionsWithCast(T partition, const ptrdiff_t *positions, size_t length) {
 *       if (stepPartition < partition) { ApplyStep(partition); }
 *       T *pInsertion = body.InsertEmpty(partition, length);
 *       for (size_t i = 0; i < length; i++) pInsertion[i] = static_cast<T>(positions[i]);
 *       stepPartition += static_cast<T>(length);
 *   }
 */
void PT_FN(InsertPartitionsWithCast)(PT_STRUCT *self, PT_T partition,
                                      const ptrdiff_t *positions, size_t length) {
    if (self->stepPartition < partition)
        PT_FN(ApplyStep)(self, partition);
    PT_T *pInsertion = PT_SV_FN(InsertEmpty)(&self->body,
                                               (ptrdiff_t)partition,
                                               (ptrdiff_t)length);
    if (pInsertion) {
        for (size_t i = 0; i < length; i++)
            pInsertion[i] = (PT_T)positions[i];
    }
    self->stepPartition += (PT_T)length;
}

/* ── SetPartitionStartPosition ───────────────────────────────────────
 *
 * C++ original:
 *   void SetPartitionStartPosition(T partition, T pos) noexcept {
 *       ApplyStep(partition+1);
 *       if ((partition < 0) || (partition >= body.Length())) return;
 *       body.SetValueAt(partition, pos);
 *   }
 */
void PT_FN(SetPartitionStartPosition)(PT_STRUCT *self, PT_T partition, PT_T pos) {
    PT_FN(ApplyStep)(self, partition + 1);
    if (partition < 0 || (ptrdiff_t)partition >= PT_SV_FN(Length)(&self->body))
        return;
    PT_SV_FN(SetValueAt)(&self->body, (ptrdiff_t)partition, pos);
}

/* ── InsertText ───────────────────────────────────────────────────────
 *
 * C++ original (verbatim logic preserved):
 *   void InsertText(T partitionInsert, T delta) noexcept { ... }
 */
void PT_FN(InsertText)(PT_STRUCT *self, PT_T partitionInsert, PT_T delta) {
    if (self->stepLength != 0) {
        if (partitionInsert >= self->stepPartition) {
            PT_FN(ApplyStep)(self, partitionInsert);
            self->stepLength += delta;
        } else if (partitionInsert >= (self->stepPartition -
                   (PT_T)(PT_SV_FN(Length)(&self->body) / 10))) {
            PT_FN(BackStep)(self, partitionInsert);
            self->stepLength += delta;
        } else {
            PT_FN(ApplyStep)(self, PT_FN(Partitions)(self));
            self->stepPartition = partitionInsert;
            self->stepLength    = delta;
        }
    } else {
        self->stepPartition = partitionInsert;
        self->stepLength    = delta;
    }
}

/* ── RemovePartition ──────────────────────────────────────────────────
 *
 * C++ original:
 *   void RemovePartition(T partition) {
 *       if (partition > stepPartition) {
 *           ApplyStep(partition); stepPartition--;
 *       } else {
 *           stepPartition--;
 *       }
 *       body.Delete(partition);
 *   }
 */
void PT_FN(RemovePartition)(PT_STRUCT *self, PT_T partition) {
    if (partition > self->stepPartition) {
        PT_FN(ApplyStep)(self, partition);
        self->stepPartition--;
    } else {
        self->stepPartition--;
    }
    PT_SV_FN(Delete)(&self->body, (ptrdiff_t)partition);
}

/* ── PositionFromPartition ───────────────────────────────────────────
 *
 * C++ original:
 *   T PositionFromPartition(T partition) const noexcept { ... }
 */
PT_T PT_FN(PositionFromPartition)(const PT_STRUCT *self, PT_T partition) {
    assert(partition >= 0);
    assert(partition < PT_SV_FN(Length)(&self->body));
    const ptrdiff_t lengthBody = PT_SV_FN(Length)(&self->body);
    if (partition < 0 || (ptrdiff_t)partition >= lengthBody)
        return (PT_T)0;
    PT_T pos = PT_SV_FN(ValueAt)(&self->body, (ptrdiff_t)partition);
    if (partition > self->stepPartition)
        pos += self->stepLength;
    return pos;
}

/* ── PartitionFromPosition ───────────────────────────────────────────
 *
 * Binary search; uses ValueAt in place of body[middle].
 *
 * C++ original:
 *   T PartitionFromPosition(T pos) const noexcept { ... }
 */
PT_T PT_FN(PartitionFromPosition)(const PT_STRUCT *self, PT_T pos) {
    if (PT_SV_FN(Length)(&self->body) <= 1)
        return (PT_T)0;
    if (pos >= PT_FN(PositionFromPartition)(self, PT_FN(Partitions)(self)))
        return PT_FN(Partitions)(self) - 1;
    PT_T lower = 0;
    PT_T upper = PT_FN(Partitions)(self);
    do {
        const PT_T middle    = (upper + lower + 1) / 2;   /* Round high */
        PT_T posMiddle = PT_SV_FN(ValueAt)(&self->body, (ptrdiff_t)middle);
        if (middle > self->stepPartition)
            posMiddle += self->stepLength;
        if (pos < posMiddle) {
            upper = middle - 1;
        } else {
            lower = middle;
        }
    } while (lower < upper);
    return lower;
}

/* ── DeleteAll ────────────────────────────────────────────────────────
 *
 * C++ original:
 *   void DeleteAll() {
 *       body.DeleteAll();
 *       stepPartition = stepLength = 0;
 *       body.Insert(0, 0);
 *       body.Insert(1, 0);
 *   }
 */
void PT_FN(DeleteAll)(PT_STRUCT *self) {
    PT_SV_FN(DeleteAll)(&self->body);
    self->stepPartition = 0;
    self->stepLength    = 0;
    PT_SV_FN(Insert)(&self->body, 0, (PT_T)0);
    PT_SV_FN(Insert)(&self->body, 1, (PT_T)0);
}

#undef _PT_CAT2
#undef _PT_CAT
#undef PT_SV_FN
