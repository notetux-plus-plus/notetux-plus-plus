/* Scintilla source code edit control
 * RunStyles.h — C translation of scintilla/src/RunStyles.h
 *
 * RunStyles is a run-length-encoded sparse style store.  It divides the
 * document into contiguous "runs" where all bytes share the same STYLE value.
 * Only run *boundaries* are stored (in a Partitioning), plus one STYLE value
 * per run (in a SplitVector).  This is far more space-efficient than one
 * style byte per character when large regions share the same style.
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   template <typename DISTANCE, typename STYLE>
 *   class RunStyles { Partitioning<DISTANCE> starts; SplitVector<STYLE> styles; }
 *
 *   Two-parameter template → four concrete structs via X-macro:
 *     RunStyles_Int_Int   (DISTANCE=int,       STYLE=int)
 *     RunStyles_Int_Char  (DISTANCE=int,       STYLE=char)
 *     RunStyles_PD_Int    (DISTANCE=ptrdiff_t, STYLE=int)
 *     RunStyles_PD_Char   (DISTANCE=ptrdiff_t, STYLE=char)
 *
 *   FillResult<DISTANCE>  — return type for FillRange.
 *   Two variants: FillResult_Int, FillResult_PD.
 *
 *   bool AllSame() / bool AllSameAs()  → int (0/1)
 *   throw std::runtime_error(…)        → assert(0); abort()
 */

#ifndef RUNSTYLES_C_H
#define RUNSTYLES_C_H

#include <stddef.h>
#include "SplitVector.h"
#include "Partitioning.h"

/* ── FillResult (per DISTANCE type) ─────────────────────────────────
 *
 * C++ original:
 *   template <typename DISTANCE>
 *   struct FillResult { bool changed; DISTANCE position; DISTANCE fillLength; };
 */
typedef struct { int changed; int       position; int       fillLength; } FillResult_Int;
typedef struct { int changed; ptrdiff_t position; ptrdiff_t fillLength; } FillResult_PD;

/* ── Name-mangling helpers ───────────────────────────────────────────
 *
 * Two-level concatenation (RS_DS and RS_SS are already token sequences,
 * not macro calls, so one _CAT level suffices for the outer join):
 *
 *   RS_STRUCT  → RunStyles_##RS_DS##_##RS_SS
 *   RS_FN(fn)  → RunStyles_##RS_DS##_##RS_SS##_##fn
 *   RS_P       → Partitioning_##RS_DS
 *   RS_P_FN(fn)→ Partitioning_##RS_DS##_##fn
 *   RS_SV      → SplitVector_##RS_SS
 *   RS_SV_FN(fn)→ SplitVector_##RS_SS##_##fn
 *   RS_FR      → FillResult_##RS_DS
 */
#define _RS_CAT2(a,b)    a##b
#define _RS_CAT(a,b)     _RS_CAT2(a,b)
#define _RS_CAT3(a,b,c)  _RS_CAT(_RS_CAT(a,b),c)

#define RS_STRUCT        _RS_CAT3(RunStyles_, RS_DS, _RS_CAT(_,RS_SS))
#define RS_FN(fn)        _RS_CAT(RS_STRUCT, _RS_CAT(_,fn))
#define RS_P             _RS_CAT(Partitioning_, RS_DS)
#define RS_P_FN(fn)      _RS_CAT(RS_P, _RS_CAT(_,fn))
#define RS_SV            _RS_CAT(SplitVector_, RS_SS)
#define RS_SV_FN(fn)     _RS_CAT(RS_SV, _RS_CAT(_,fn))
#define RS_FR            _RS_CAT(FillResult_, RS_DS)

/* ── Instantiation 1: RunStyles_Int_Int ─────────────────────────── */
#define RS_D      int
#define RS_S      int
#define RS_DS     Int
#define RS_SS     Int
#define RS_S_ZERO 0
#include "rs_tpl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO

/* ── Instantiation 2: RunStyles_Int_Char ────────────────────────── */
#define RS_D      int
#define RS_S      char
#define RS_DS     Int
#define RS_SS     Char
#define RS_S_ZERO '\0'
#include "rs_tpl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO

/* ── Instantiation 3: RunStyles_PD_Int ──────────────────────────── */
#define RS_D      ptrdiff_t
#define RS_S      int
#define RS_DS     PD
#define RS_SS     Int
#define RS_S_ZERO 0
#include "rs_tpl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO

/* ── Instantiation 4: RunStyles_PD_Char ─────────────────────────── */
#define RS_D      ptrdiff_t
#define RS_S      char
#define RS_DS     PD
#define RS_SS     Char
#define RS_S_ZERO '\0'
#include "rs_tpl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO

#undef _RS_CAT2
#undef _RS_CAT
#undef _RS_CAT3
#undef RS_STRUCT
#undef RS_FN
#undef RS_P
#undef RS_P_FN
#undef RS_SV
#undef RS_SV_FN
#undef RS_FR

#endif /* RUNSTYLES_C_H */
