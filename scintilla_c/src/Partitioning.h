/* Scintilla source code edit control
 * Partitioning.h — C translation of scintilla/src/Partitioning.h
 *
 * Instantiates two concrete types by including pt_tpl.h twice:
 *   Partitioning_Int  (int)       — used for 32-bit documents
 *   Partitioning_PD   (ptrdiff_t) — used for 64-bit documents
 *
 * ── C++ → C translation map ──────────────────────────────────────────
 *
 *   template <typename T> class Partitioning
 *   → struct Partitioning_PT_SUFFIX  (two instantiations below)
 *
 *   SplitVector<T> body
 *   → SplitVector_Int body  /  SplitVector_PD body
 *   (the body member type is different per instantiation)
 *
 *   T stepPartition, T stepLength
 *   → PT_T stepPartition, PT_T stepLength  (concrete fields)
 *
 *   body[middle]   (operator[])
 *   → SplitVector_Int_ValueAt(&self->body, middle)
 *     — returns value; Partitioning only reads through operator[] once
 *       (PartitionFromPosition) and reads/writes via other helpers.
 *
 *   body.Insert(position, pos)
 *   → SplitVector_Int_Insert(&self->body, position, pos)
 *
 *   PLATFORM_ASSERT → assert()
 *   throw std::runtime_error → assert(0)  (Check() only in debug builds)
 */

#ifndef PARTITIONING_C_H
#define PARTITIONING_C_H

#include <stddef.h>
#include <assert.h>
#include "SplitVector.h"   /* SplitVector_Int, SplitVector_PD */

/* ── Name-mangling helpers (same pattern as sv_tpl.h) ───────────────── */
#define _PT_CAT2(a,b) a##b
#define _PT_CAT(a,b)  _PT_CAT2(a,b)
#define PT_STRUCT  _PT_CAT(Partitioning_, PT_SUFFIX)
#define PT_FN(fn)  _PT_CAT(_PT_CAT(Partitioning_, PT_SUFFIX), _PT_CAT(_, fn))

/* ── Instantiation 1: Partitioning_Int (T = int) ───────────────────── */
#define PT_T      int
#define PT_SUFFIX Int
#define PT_SV     SplitVector_Int
#include "pt_tpl.h"
#undef PT_T
#undef PT_SUFFIX
#undef PT_SV

/* ── Instantiation 2: Partitioning_PD (T = ptrdiff_t) ──────────────── */
#define PT_T      ptrdiff_t
#define PT_SUFFIX PD
#define PT_SV     SplitVector_PD
#include "pt_tpl.h"
#undef PT_T
#undef PT_SUFFIX
#undef PT_SV

#undef _PT_CAT2
#undef _PT_CAT
#undef PT_STRUCT
#undef PT_FN

#endif /* PARTITIONING_C_H */
