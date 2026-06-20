/* sv_impl.h — SplitVector implementation template (included by SplitVector.c)
 *
 * Include with SV_T, SV_SUFFIX, SV_ZERO defined.  Produces function bodies
 * for one concrete instantiation of SplitVector.
 *
 * ── C++ → C translation summary ──────────────────────────────────────
 *
 *  std::move_backward(src_begin, src_end, dst_end)
 *  → memmove(dst_end - (src_end - src_begin),
 *             src_begin,
 *             (src_end - src_begin) * sizeof(SV_T))
 *  Note: dst_end in C++ points PAST the last destination element.
 *  memmove is safe for overlapping ranges.
 *
 *  std::move(src_begin, src_end, dst_begin)
 *  → memmove(dst_begin, src_begin, (src_end - src_begin) * sizeof(SV_T))
 *
 *  std::fill_n(ptr, n, value)
 *  → memset(ptr, value, n * sizeof(SV_T))   [only valid for byte-width v]
 *    For multi-byte T we'd need a loop; since SV_ZERO is always 0 here
 *    memset works correctly for all T.
 *
 *  std::copy_n(src, n, dst)
 *  → memcpy(dst, src, n * sizeof(SV_T))
 *
 *  try { ... } catch (...) {}
 *  → removed — the C equivalent (memmove) cannot throw
 *
 *  body.reserve(n); body.resize(n);
 *  → realloc(self->body, n * sizeof(SV_T))  then update bodySize
 *    We move the gap to the end first (same as the C++ code) so the
 *    data before the gap is contiguous and realloc is safe.
 */

#ifndef SV_T
#  error "sv_impl.h requires SV_T defined"
#endif

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define _SV_CAT2(a,b) a##b
#define _SV_CAT(a,b)  _SV_CAT2(a,b)
#define SV_STRUCT  _SV_CAT(SplitVector_, SV_SUFFIX)
#define SV_FN(fn)  _SV_CAT(_SV_CAT(SplitVector_, SV_SUFFIX), _SV_CAT(_, fn))

/* ── Internal: move the gap to the requested position ────────────────
 *
 * C++ original (noexcept, try/catch around std::move* which can't throw):
 *   void GapTo(ptrdiff_t position) noexcept { ... }
 *
 * In C we omit the try/catch entirely.  memmove is always safe.
 */
static void SV_FN(GapTo)(SV_STRUCT *self, ptrdiff_t position) {
    if (position == self->part1Length) return;
    if (self->gapLength > 0) {
        if (position < self->part1Length) {
            /* Moving gap towards start: shift elements right */
            memmove(
                self->body + position + self->gapLength,
                self->body + position,
                (size_t)(self->part1Length - position) * sizeof(SV_T));
        } else {
            /* Moving gap towards end: shift elements left */
            memmove(
                self->body + self->part1Length,
                self->body + self->part1Length + self->gapLength,
                (size_t)(position - self->part1Length) * sizeof(SV_T));
        }
    }
    self->part1Length = position;
}

/* ── Internal: ensure the gap is at least insertionLength wide ───────
 *
 * C++ original:
 *   void RoomFor(ptrdiff_t insertionLength) {
 *       if (gapLength < insertionLength) {
 *           while (growSize < body.size() / 6) growSize *= 2;
 *           ReAllocate(body.size() + insertionLength + growSize);
 *       }
 *   }
 */
static void SV_FN(RoomFor)(SV_STRUCT *self, ptrdiff_t insertionLength) {
    if (self->gapLength < insertionLength) {
        while (self->growSize < self->bodySize / 6)
            self->growSize *= 2;
        SV_FN(ReAllocate)(self, self->bodySize + (size_t)insertionLength + self->growSize);
    }
}

/* ── init ─────────────────────────────────────────────────────────────
 *
 * C++ original: constructor SplitVector(size_t growSize_ = 8)
 *   : empty(), lengthBody(0), part1Length(0), gapLength(0), growSize(growSize_)
 *
 * We also zero-initialise the empty sentinel and body pointer.
 */
void SV_FN(init)(SV_STRUCT *self) {
    self->body        = NULL;
    self->bodySize    = 0;
    self->lengthBody  = 0;
    self->part1Length = 0;
    self->gapLength   = 0;
    self->growSize    = 8;
    self->empty       = (SV_T)SV_ZERO;
}

/* ── destroy ───────────────────────────────────────────────────────── */
void SV_FN(destroy)(SV_STRUCT *self) {
    free(self->body);
    self->body     = NULL;
    self->bodySize = 0;
}

/* ── ReAllocate ──────────────────────────────────────────────────────
 *
 * C++ original:
 *   void ReAllocate(size_t newSize) {
 *       if (newSize > body.size()) {
 *           GapTo(lengthBody);              // move gap to end
 *           gapLength += newSize - body.size();
 *           body.reserve(newSize);
 *           body.resize(newSize);
 *       }
 *   }
 *
 * std::vector::reserve + resize with an already-reserved size is a
 * no-copy reallocation.  In C, realloc after GapTo(lengthBody) achieves
 * the same: the gap is at the end, so data before it is contiguous, and
 * realloc can simply extend the trailing allocation.
 */
void SV_FN(ReAllocate)(SV_STRUCT *self, size_t newSize) {
    if (newSize > self->bodySize) {
        SV_FN(GapTo)(self, self->lengthBody);   /* move gap to end */
        self->gapLength += (ptrdiff_t)(newSize - self->bodySize);
        SV_T *nb = (SV_T *)realloc(self->body, newSize * sizeof(SV_T));
        assert(nb);
        self->body     = nb;
        self->bodySize = newSize;
    }
}

/* ── ValueAt ─────────────────────────────────────────────────────────
 *
 * C++ original (const T& — avoids copy for complex T; returns empty for
 * out-of-range):
 *   const T& ValueAt(ptrdiff_t position) const noexcept { ... }
 *
 * C returns by value (T is always a scalar here).
 */
SV_T SV_FN(ValueAt)(const SV_STRUCT *self, ptrdiff_t position) {
    if (position < self->part1Length) {
        if (position < 0) return self->empty;
        return self->body[position];
    } else {
        if (position >= self->lengthBody) return self->empty;
        return self->body[self->gapLength + position];
    }
}

/* ── SetValueAt ──────────────────────────────────────────────────────
 *
 * C++ used a forwarding reference (template ParamType&&) to allow moving
 * non-trivial types.  For scalar T the forward is a no-op; in C we just
 * take by value.
 */
void SV_FN(SetValueAt)(SV_STRUCT *self, ptrdiff_t position, SV_T v) {
    if (position < self->part1Length) {
        assert(position >= 0);
        if (position >= 0) self->body[position] = v;
    } else {
        assert(position < self->lengthBody);
        if (position < self->lengthBody)
            self->body[self->gapLength + position] = v;
    }
}

/* ── Length ────────────────────────────────────────────────────────── */
ptrdiff_t SV_FN(Length)(const SV_STRUCT *self) {
    return self->lengthBody;
}

/* ── InsertValue ─────────────────────────────────────────────────────
 *
 * C++ used std::fill_n to fill the gap with v.
 * For byte-size T (char): memset works.
 * For multi-byte T (int, ptrdiff_t): SV_ZERO is always 0, so memset
 * with 0 is correct (all-bits-zero == 0 for any integer type on all
 * platforms we target).
 */
void SV_FN(InsertValue)(SV_STRUCT *self, ptrdiff_t position,
                         ptrdiff_t insertLength, SV_T v) {
    assert(position >= 0 && position <= self->lengthBody);
    if (insertLength <= 0) return;
    if (position < 0 || position > self->lengthBody) return;
    SV_FN(RoomFor)(self, insertLength);
    SV_FN(GapTo)(self, position);
    /* fill inserted region */
    if ((SV_T)(v) == (SV_T)SV_ZERO) {
        memset(self->body + self->part1Length, 0, (size_t)insertLength * sizeof(SV_T));
    } else {
        for (ptrdiff_t i = 0; i < insertLength; i++)
            self->body[self->part1Length + i] = v;
    }
    self->lengthBody  += insertLength;
    self->part1Length += insertLength;
    self->gapLength   -= insertLength;
}

/* ── InsertFromArray ─────────────────────────────────────────────────
 *
 * C++ used std::copy_n → memcpy (no overlap possible since source is
 * external and the gap area is unrelated to it).
 */
void SV_FN(InsertFromArray)(SV_STRUCT *self, ptrdiff_t positionToInsert,
                              const SV_T s[], ptrdiff_t positionFrom,
                              ptrdiff_t insertLength) {
    assert(positionToInsert >= 0 && positionToInsert <= self->lengthBody);
    if (insertLength <= 0) return;
    if (positionToInsert < 0 || positionToInsert > self->lengthBody) return;
    SV_FN(RoomFor)(self, insertLength);
    SV_FN(GapTo)(self, positionToInsert);
    memcpy(self->body + self->part1Length,
           s + positionFrom,
           (size_t)insertLength * sizeof(SV_T));
    self->lengthBody  += insertLength;
    self->part1Length += insertLength;
    self->gapLength   -= insertLength;
}

/* ── DeleteRange ──────────────────────────────────────────────────────
 *
 * C++ original:
 *   void DeleteRange(ptrdiff_t position, ptrdiff_t deleteLength) {
 *       if ((position == 0) && (deleteLength == lengthBody)) {
 *           Init();          // full deletion: release storage
 *       } else if (deleteLength > 0) {
 *           GapTo(position);
 *           lengthBody -= deleteLength;
 *           gapLength  += deleteLength;
 *       }
 *   }
 *
 * Same logic; Init() → re-call our init function (frees body).
 */
void SV_FN(DeleteRange)(SV_STRUCT *self, ptrdiff_t position,
                          ptrdiff_t deleteLength) {
    assert(position >= 0 && (position + deleteLength) <= self->lengthBody);
    if (position < 0 || (position + deleteLength) > self->lengthBody) return;
    if (position == 0 && deleteLength == self->lengthBody) {
        SV_FN(destroy)(self);
        SV_FN(init)(self);
    } else if (deleteLength > 0) {
        SV_FN(GapTo)(self, position);
        self->lengthBody -= deleteLength;
        self->gapLength  += deleteLength;
    }
}

/* ── Insert ───────────────────────────────────────────────────────────
 *
 * C++ original: void Insert(ptrdiff_t position, T v)
 * Inserts a single element — shorthand for InsertValue(..., 1, v).
 */
void SV_FN(Insert)(SV_STRUCT *self, ptrdiff_t position, SV_T v) {
    assert(position >= 0 && position <= self->lengthBody);
    if (position < 0 || position > self->lengthBody) return;
    SV_FN(RoomFor)(self, 1);
    SV_FN(GapTo)(self, position);
    self->body[self->part1Length] = v;
    self->lengthBody++;
    self->part1Length++;
    self->gapLength--;
}

/* ── InsertEmpty ──────────────────────────────────────────────────────
 *
 * C++ original: T *InsertEmpty(ptrdiff_t position, ptrdiff_t insertLength)
 * Inserts zero-initialised elements; returns pointer to inserted region
 * (at position in logical space — caller may write through it).
 */
SV_T *SV_FN(InsertEmpty)(SV_STRUCT *self, ptrdiff_t position,
                           ptrdiff_t insertLength) {
    assert(position >= 0 && position <= self->lengthBody);
    if (insertLength <= 0) return (SV_T *)SV_FN(ElementPointer)(self, position);
    if (position < 0 || position > self->lengthBody) return NULL;
    SV_FN(RoomFor)(self, insertLength);
    SV_FN(GapTo)(self, position);
    memset(self->body + self->part1Length, 0, (size_t)insertLength * sizeof(SV_T));
    self->lengthBody  += insertLength;
    self->part1Length += insertLength;
    self->gapLength   -= insertLength;
    /* gap has moved; the inserted elements start at position in logical space */
    return (SV_T *)SV_FN(ElementPointer)(self, position);
}

/* ── Delete ───────────────────────────────────────────────────────────
 *
 * C++ original: void Delete(ptrdiff_t position)
 * Deletes one element — delegates to DeleteRange.
 */
void SV_FN(Delete)(SV_STRUCT *self, ptrdiff_t position) {
    assert(position >= 0 && position < self->lengthBody);
    SV_FN(DeleteRange)(self, position, 1);
}

/* ── DeleteAll ─────────────────────────────────────────────────────── */
void SV_FN(DeleteAll)(SV_STRUCT *self) {
    SV_FN(DeleteRange)(self, 0, self->lengthBody);
}

/* ── GetRange ─────────────────────────────────────────────────────────
 *
 * C++ used two std::copy_n calls for the two segments around the gap.
 * → two memcpy calls (non-overlapping by construction).
 */
void SV_FN(GetRange)(const SV_STRUCT *self, SV_T *buffer,
                      ptrdiff_t position, ptrdiff_t retrieveLength) {
    ptrdiff_t range1Length = 0;
    if (position < self->part1Length) {
        ptrdiff_t part1After = self->part1Length - position;
        range1Length = (retrieveLength < part1After) ? retrieveLength : part1After;
    }
    memcpy(buffer, self->body + position,
           (size_t)range1Length * sizeof(SV_T));
    buffer   += range1Length;
    position  = position + range1Length + self->gapLength;
    ptrdiff_t range2Length = retrieveLength - range1Length;
    memcpy(buffer, self->body + position,
           (size_t)range2Length * sizeof(SV_T));
}

/* ── BufferPointer ───────────────────────────────────────────────────
 *
 * C++ moved the gap to the end (GapTo(lengthBody)), guaranteed room for
 * one extra element for a NUL terminator, and returned body.data().
 * Same in C; we just use realloc via RoomFor.
 */
SV_T *SV_FN(BufferPointer)(SV_STRUCT *self) {
    SV_FN(RoomFor)(self, 1);
    SV_FN(GapTo)(self, self->lengthBody);
    self->body[self->lengthBody] = (SV_T)SV_ZERO;
    return self->body;
}

/* ── RangePointer ───────────────────────────────────────────────────
 *
 * Returns a pointer to a contiguous range [position, position+rangeLength).
 * May move the gap to make the range contiguous.
 */
SV_T *SV_FN(RangePointer)(SV_STRUCT *self, ptrdiff_t position,
                             ptrdiff_t rangeLength) {
    if (position < self->part1Length) {
        if ((position + rangeLength) > self->part1Length) {
            SV_FN(GapTo)(self, position);
            return self->body + position + self->gapLength;
        } else {
            return self->body + position;
        }
    } else {
        return self->body + position + self->gapLength;
    }
}

/* ── ElementPointer ─────────────────────────────────────────────────
 *
 * Does NOT rearrange the buffer.  Returns a pointer valid only within
 * one side of the gap.
 */
const SV_T *SV_FN(ElementPointer)(const SV_STRUCT *self, ptrdiff_t position) {
    if (position < self->part1Length)
        return self->body + position;
    else
        return self->body + position + self->gapLength;
}

/* ── GapPosition ─────────────────────────────────────────────────── */
ptrdiff_t SV_FN(GapPosition)(const SV_STRUCT *self) {
    return self->part1Length;
}

/* Clean up name-mangling macros */
#undef SV_STRUCT
#undef SV_FN
