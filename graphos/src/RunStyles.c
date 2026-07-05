/* Scintilla source code edit control
 * RunStyles.c — C translation of scintilla/src/RunStyles.cxx
 *
 * Includes rs_impl.h four times (once per DISTANCE×STYLE combination)
 * to instantiate the full set of RunStyles concrete types.
 *
 * ── Template expansion order ──────────────────────────────────────────
 *
 *   Instantiation  DISTANCE    STYLE   struct name
 *   ─────────────  ──────────  ──────  ───────────────────
 *   1              int         int     RunStyles_Int_Int
 *   2              int         char    RunStyles_Int_Char
 *   3              ptrdiff_t   int     RunStyles_PD_Int
 *   4              ptrdiff_t   char    RunStyles_PD_Char
 *
 * Each inclusion of rs_impl.h emits static helper functions (prefixed with
 * the concrete struct name) plus all public RunStyles_*_* functions.
 * The RS_* driver macros are re-defined before each inclusion and must
 * match the definitions used for the corresponding rs_tpl.h inclusion in
 * RunStyles.h.
 */

#include "RunStyles.h"   /* FillResult_* structs + rs_tpl.h 4×, sets macro aliases */

/* ── Name-mangling helpers (must shadow those in RunStyles.h) ─────────
 *
 * RunStyles.h defines them, then undefines them at the end.  Re-define
 * them here before including rs_impl.h so the implementation file can
 * use the same RS_* derived macros.
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

/* ── Instantiation 1: RunStyles_Int_Int ─────────────────────────────── */
#define RS_D      int
#define RS_S      int
#define RS_DS     Int
#define RS_SS     Int
#define RS_S_ZERO 0
#include "rs_impl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO

/* ── Instantiation 2: RunStyles_Int_Char ────────────────────────────── */
#define RS_D      int
#define RS_S      char
#define RS_DS     Int
#define RS_SS     Char
#define RS_S_ZERO '\0'
#include "rs_impl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO

/* ── Instantiation 3: RunStyles_PD_Int ──────────────────────────────── */
#define RS_D      ptrdiff_t
#define RS_S      int
#define RS_DS     PD
#define RS_SS     Int
#define RS_S_ZERO 0
#include "rs_impl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO

/* ── Instantiation 4: RunStyles_PD_Char ─────────────────────────────── */
#define RS_D      ptrdiff_t
#define RS_S      char
#define RS_DS     PD
#define RS_SS     Char
#define RS_S_ZERO '\0'
#include "rs_impl.h"
#undef RS_D
#undef RS_S
#undef RS_DS
#undef RS_SS
#undef RS_S_ZERO
