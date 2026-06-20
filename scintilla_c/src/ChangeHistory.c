/* ChangeHistory.c — stub implementation
 *
 * Full C translation deferred. ChangeHistory.cxx depends on RunStyles
 * and SparseVector (both C++ templates), which need to be translated first.
 *
 * All functions abort() so that linker errors become runtime errors during
 * development.
 */

#include <stdlib.h>
#include <assert.h>
#include "ChangeHistory.h"

#define STUB()  assert(!"ChangeHistory stub not yet implemented"); abort()

ChangeHistory *ChangeHistory_create(SciPosition length)
    { (void)length; STUB(); return NULL; }
void ChangeHistory_destroy(ChangeHistory *self) { (void)self; STUB(); }

void ChangeHistory_Insert(ChangeHistory *s, SciPosition pos, SciPosition len,
                           int cu, int bs)
    { (void)s;(void)pos;(void)len;(void)cu;(void)bs; STUB(); }
void ChangeHistory_DeleteRange(ChangeHistory *s, SciPosition pos,
                                SciPosition len, int rev)
    { (void)s;(void)pos;(void)len;(void)rev; STUB(); }
void ChangeHistory_DeleteRangeSavingHistory(ChangeHistory *s, SciPosition pos,
    SciPosition len, int bs, int det)
    { (void)s;(void)pos;(void)len;(void)bs;(void)det; STUB(); }

void ChangeHistory_StartReversion(ChangeHistory *s) { (void)s; STUB(); }
void ChangeHistory_EndReversion(ChangeHistory *s)   { (void)s; STUB(); }
void ChangeHistory_SetSavePoint(ChangeHistory *s)   { (void)s; STUB(); }
void ChangeHistory_UndoDeleteStep(ChangeHistory *s, SciPosition pos,
    SciPosition len, int det)
    { (void)s;(void)pos;(void)len;(void)det; STUB(); }

SciPosition  ChangeHistory_Length(const ChangeHistory *s)       {(void)s;STUB();return 0;}
int          ChangeHistory_EditionAt(const ChangeHistory *s, SciPosition p) {(void)s;(void)p;STUB();return 0;}
SciPosition  ChangeHistory_EditionEndRun(const ChangeHistory *s, SciPosition p){(void)s;(void)p;STUB();return 0;}
unsigned int ChangeHistory_EditionDeletesAt(const ChangeHistory *s, SciPosition p){(void)s;(void)p;STUB();return 0;}
SciPosition  ChangeHistory_EditionNextDelete(const ChangeHistory *s, SciPosition p){(void)s;(void)p;STUB();return 0;}
void         ChangeHistory_Check(ChangeHistory *s) { (void)s; STUB(); }
