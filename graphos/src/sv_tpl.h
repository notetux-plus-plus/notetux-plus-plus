/* sv_tpl.h — SplitVector implementation template
 *
 * This file is NOT a normal header — it is a C template in the style of
 * Linux's list.h / OpenBSD's queue.h: it is #include-d multiple times
 * with different pre-processor defines to generate concrete types.
 *
 * Required macros before inclusion:
 *   SV_T      — element type  (e.g. char, int, ptrdiff_t)
 *   SV_SUFFIX — name suffix   (e.g. Char, Int, PD)
 *   SV_ZERO   — zero value for empty / fill (e.g. '\0', 0)
 *   SV_MEMCPY — 1 if memmove/memcpy/memset are valid for SV_T (always true
 *               here since T is always a primitive integer type)
 *
 * After inclusion, you get:
 *   typedef struct SplitVector_SV_SUFFIX { ... } SplitVector_SV_SUFFIX;
 *   void  SV_FN(init)(SplitVector_SV_SUFFIX *self);
 *   void  SV_FN(destroy)(SplitVector_SV_SUFFIX *self);
 *   ...
 *
 * Repeat the include block for each needed concrete type.
 *
 * ── Why this pattern? ─────────────────────────────────────────────────
 *
 * C has no generics.  The three practical options are:
 *
 *   (a) Void-pointer container + runtime size  — loses type safety and
 *       forces casts everywhere.
 *   (b) Code generation (m4, Jinja, etc.)  — external tool dependency.
 *   (c) Include-multiple-times template  — pure C, no tools, type-safe,
 *       inlineable, debuggable.  Used by Linux kernel (list.h), glibc
 *       (sorthelper.h), OpenBSD (queue.h), many embedded projects.
 *
 * We use (c).  The generated code is identical to what a C++ compiler
 * produces for template instantiation, just made explicit.
 *
 * ── C++ → C translation map for SplitVector<T> ───────────────────────
 *
 *   class SplitVector<T>          → struct SplitVector_SV_SUFFIX
 *   std::vector<T> body           → T *body  (raw heap array)
 *   body.size()                   → self->bodySize
 *   body.data()                   → self->body
 *   body.reserve(n)/resize(n)     → SV_FN(grow_body)(self, n)  [internal]
 *   std::move_backward(a,b,c)     → memmove(c-(b-a), a, (b-a)*sizeof(T))
 *   std::move(a,b,c)              → memmove(c, a, (b-a)*sizeof(T))
 *   std::fill_n(ptr,n,v)          → memset(ptr, v, n)  [for byte types]
 *   std::copy_n(src,n,dst)        → memcpy(dst, src, n*sizeof(T))
 *   T emptyOne = {}               → SV_ZERO constant
 *   try { ... } catch(...) {}     → removed (memmove cannot fail)
 *   noexcept                      → removed
 *   protected: / public:          → all fields in the struct; convention
 *                                   only internal functions touch body/gap
 */

#ifndef SV_T
#  error "sv_tpl.h requires SV_T to be defined before inclusion"
#endif
#ifndef SV_SUFFIX
#  error "sv_tpl.h requires SV_SUFFIX to be defined before inclusion"
#endif
#ifndef SV_ZERO
#  error "sv_tpl.h requires SV_ZERO to be defined before inclusion"
#endif

/* ── Name-mangling helpers ───────────────────────────────────────────── */
#define _SV_CAT2(a,b)  a##b
#define _SV_CAT(a,b)   _SV_CAT2(a,b)
#define SV_STRUCT  _SV_CAT(SplitVector_, SV_SUFFIX)
#define SV_FN(fn)  _SV_CAT(_SV_CAT(SplitVector_, SV_SUFFIX), _SV_CAT(_, fn))

/* ── Struct declaration ──────────────────────────────────────────────── */
typedef struct SV_STRUCT {
    SV_T      *body;          /* raw heap array, size == bodySize          */
    size_t     bodySize;      /* allocated element count (was body.size()) */
    SV_T       empty;         /* returned for out-of-range access           */
    ptrdiff_t  lengthBody;    /* logical length (body + gap = bodySize)    */
    ptrdiff_t  part1Length;   /* length of part before the gap             */
    ptrdiff_t  gapLength;     /* length of the gap (bodySize-lengthBody)   */
    size_t     growSize;      /* minimum growth quantum                    */
} SV_STRUCT;

/* ── Function declarations ───────────────────────────────────────────── */
void      SV_FN(init)          (SV_STRUCT *self);
void      SV_FN(destroy)       (SV_STRUCT *self);
void      SV_FN(ReAllocate)    (SV_STRUCT *self, size_t newSize);
SV_T      SV_FN(ValueAt)       (const SV_STRUCT *self, ptrdiff_t position);
void      SV_FN(SetValueAt)    (SV_STRUCT *self, ptrdiff_t position, SV_T v);
ptrdiff_t SV_FN(Length)        (const SV_STRUCT *self);
void      SV_FN(InsertValue)   (SV_STRUCT *self, ptrdiff_t position,
                                 ptrdiff_t insertLength, SV_T v);
void      SV_FN(InsertFromArray)(SV_STRUCT *self, ptrdiff_t positionToInsert,
                                  const SV_T s[], ptrdiff_t positionFrom,
                                  ptrdiff_t insertLength);
void      SV_FN(DeleteRange)   (SV_STRUCT *self, ptrdiff_t position,
                                 ptrdiff_t deleteLength);
void      SV_FN(Insert)        (SV_STRUCT *self, ptrdiff_t position, SV_T v);
SV_T     *SV_FN(InsertEmpty)   (SV_STRUCT *self, ptrdiff_t position,
                                 ptrdiff_t insertLength);
void      SV_FN(Delete)        (SV_STRUCT *self, ptrdiff_t position);
void      SV_FN(DeleteAll)     (SV_STRUCT *self);
void      SV_FN(GetRange)      (const SV_STRUCT *self, SV_T *buffer,
                                 ptrdiff_t position, ptrdiff_t retrieveLength);
SV_T     *SV_FN(BufferPointer) (SV_STRUCT *self);
SV_T     *SV_FN(RangePointer)  (SV_STRUCT *self, ptrdiff_t position,
                                 ptrdiff_t rangeLength);
const SV_T *SV_FN(ElementPointer)(const SV_STRUCT *self, ptrdiff_t position);
ptrdiff_t SV_FN(GapPosition)   (const SV_STRUCT *self);

/* ── Undefine name helpers (clean up for next inclusion) ─────────────── */
#undef SV_STRUCT
#undef SV_FN
/* NOTE: SV_T, SV_SUFFIX, SV_ZERO are NOT undef-d here — the caller does it. */
