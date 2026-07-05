/* Scintilla source code edit control
 * SplitVector.h — C translation of scintilla/src/SplitVector.h
 *
 * Instantiates three concrete types by including sv_tpl.h three times:
 *   SplitVector_Char   (char)      — used by CellBuffer for text and style
 *   SplitVector_Int    (int)       — used by Partitioning<int>  (32-bit docs)
 *   SplitVector_PD     (ptrdiff_t) — used by Partitioning<ptrdiff_t> (64-bit)
 *
 * ── Pattern explained ────────────────────────────────────────────────
 *
 * C++ template instantiation:
 *   SplitVector<char>      — compiler produces one copy of all methods with T=char
 *   SplitVector<int>       — compiler produces one copy with T=int
 *   SplitVector<ptrdiff_t> — compiler produces one copy with T=ptrdiff_t
 *
 * C single-include-multiple-times:
 *   #define SV_T char
 *   #define SV_SUFFIX Char
 *   #define SV_ZERO '\0'
 *   #include "sv_tpl.h"        ← preprocessor produces all declarations with T=char
 *   #undef SV_T
 *   ...
 *
 * The three include blocks below expand sv_tpl.h into three distinct sets of
 * struct typedefs and function declarations, exactly as C++ template
 * instantiation would.  The implementations are in SplitVector.c using
 * the same trick with sv_impl.h.
 */

#ifndef SPLITVECTOR_C_H
#define SPLITVECTOR_C_H

#include <stddef.h>     /* ptrdiff_t, size_t */
#include <string.h>     /* memmove, memcpy, memset */

/* ── Instantiation 1: SplitVector_Ptr (T = void *) ─────────────────────
 * Used by SparseVector_Ptr (C translation of SparseVector<EditionSetOwned>).
 * Values are opaque owned pointers; lifecycle (malloc/free) is managed by
 * the caller — SparseVector_Ptr never frees stored pointers on its own.
 */
#define SV_T       void *
#define SV_SUFFIX  Ptr
#define SV_ZERO    NULL
#include "sv_tpl.h"
#undef SV_T
#undef SV_SUFFIX
#undef SV_ZERO

/* ── Instantiation 2: SplitVector_Char (T = char) ───────────────────── */
#define SV_T       char
#define SV_SUFFIX  Char
#define SV_ZERO    '\0'
#include "sv_tpl.h"
#undef SV_T
#undef SV_SUFFIX
#undef SV_ZERO

/* ── Instantiation 2: SplitVector_Int (T = int) ─────────────────────── */
#define SV_T       int
#define SV_SUFFIX  Int
#define SV_ZERO    0
#include "sv_tpl.h"
#undef SV_T
#undef SV_SUFFIX
#undef SV_ZERO

/* ── Instantiation 3: SplitVector_PD (T = ptrdiff_t) ───────────────── */
#define SV_T       ptrdiff_t
#define SV_SUFFIX  PD
#define SV_ZERO    0
#include "sv_tpl.h"
#undef SV_T
#undef SV_SUFFIX
#undef SV_ZERO

#endif /* SPLITVECTOR_C_H */
