/* rs_tpl.h — RunStyles struct + function declaration template
 *
 * Include with RS_D, RS_S, RS_DS, RS_SS, RS_S_ZERO defined.
 * RS_STRUCT, RS_FN, RS_P, RS_P_FN, RS_SV, RS_SV_FN, RS_FR are
 * defined in RunStyles.h before each inclusion.
 *
 * Required macros:
 *   RS_D      — DISTANCE type      (int or ptrdiff_t)
 *   RS_S      — STYLE type         (int or char)
 *   RS_DS     — DISTANCE suffix    (Int or PD)
 *   RS_SS     — STYLE suffix       (Int or Char)
 *   RS_S_ZERO — zero STYLE value   (0 or '\0')
 *
 * Derived macros (defined in RunStyles.h):
 *   RS_STRUCT  → RunStyles_DS_SS
 *   RS_FN(fn)  → RunStyles_DS_SS_fn
 *   RS_P       → Partitioning_DS      (body type for starts)
 *   RS_P_FN    → Partitioning_DS_fn
 *   RS_SV      → SplitVector_SS       (body type for styles)
 *   RS_SV_FN   → SplitVector_SS_fn
 *   RS_FR      → FillResult_DS
 */

#ifndef RS_D
#  error "rs_tpl.h requires RS_D defined"
#endif

typedef struct RS_STRUCT {
    RS_P  starts;
    RS_SV styles;
} RS_STRUCT;

void      RS_FN(init)       (RS_STRUCT *self);
void      RS_FN(destroy)    (RS_STRUCT *self);
RS_D      RS_FN(Length)     (const RS_STRUCT *self);
RS_S      RS_FN(ValueAt)    (const RS_STRUCT *self, RS_D position);
RS_D      RS_FN(FindNextChange)(const RS_STRUCT *self, RS_D position, RS_D end);
RS_D      RS_FN(StartRun)   (const RS_STRUCT *self, RS_D position);
RS_D      RS_FN(EndRun)     (const RS_STRUCT *self, RS_D position);
RS_FR     RS_FN(FillRange)  (RS_STRUCT *self, RS_D position, RS_S value, RS_D fillLength);
void      RS_FN(SetValueAt) (RS_STRUCT *self, RS_D position, RS_S value);
void      RS_FN(InsertSpace)(RS_STRUCT *self, RS_D position, RS_D insertLength);
void      RS_FN(DeleteAll)  (RS_STRUCT *self);
void      RS_FN(DeleteRange)(RS_STRUCT *self, RS_D position, RS_D deleteLength);
RS_D      RS_FN(Runs)       (const RS_STRUCT *self);
int       RS_FN(AllSame)    (const RS_STRUCT *self);
int       RS_FN(AllSameAs)  (const RS_STRUCT *self, RS_S value);
RS_D      RS_FN(Find)       (const RS_STRUCT *self, RS_S value, RS_D start);
void      RS_FN(Check)      (const RS_STRUCT *self);
