/* UndoHistory.c — stub implementation
 *
 * Full C translation is deferred. Translating UndoHistory.cxx requires
 * first translating:
 *   • ScaledVector (uses std::vector<uint8_t>)
 *   • ScrapStack   (uses std::string)
 *   • UndoActions  (uses std::vector<UndoActionType> + ScaledVector)
 *
 * All functions abort() so that linker errors become runtime errors during
 * development.  Replace with real implementations after translating the
 * above dependencies.
 */

#include <stdlib.h>
#include <assert.h>
#include "UndoHistory.h"

#define STUB()  assert(!"UndoHistory stub not yet implemented"); abort()

UndoHistory *UndoHistory_create(void)            { STUB(); return NULL; }
void         UndoHistory_destroy(UndoHistory *s) { (void)s; STUB(); }

const char *UndoHistory_AppendAction(UndoHistory *s, ActionType at,
    SciPosition position, const char *data, SciPosition lengthData,
    int *startSequence, int mayCoalesce)
{
    (void)s; (void)at; (void)position; (void)data; (void)lengthData;
    (void)startSequence; (void)mayCoalesce;
    STUB(); return NULL;
}

void UndoHistory_BeginUndoAction(UndoHistory *s, int mc) { (void)s;(void)mc; STUB(); }
void UndoHistory_EndUndoAction(UndoHistory *s)            { (void)s; STUB(); }
int  UndoHistory_UndoSequenceDepth(const UndoHistory *s)  { (void)s; STUB(); return 0; }
int  UndoHistory_AfterUndoSequenceStart(const UndoHistory *s){(void)s;STUB();return 0;}
void UndoHistory_DropUndoSequence(UndoHistory *s)         { (void)s; STUB(); }
void UndoHistory_DeleteUndoHistory(UndoHistory *s)        { (void)s; STUB(); }
int  UndoHistory_Actions(const UndoHistory *s)            { (void)s; STUB(); return 0; }

void UndoHistory_SetSavePointAction(UndoHistory *s, int a){(void)s;(void)a;STUB();}
int  UndoHistory_SavePoint(const UndoHistory *s)          { (void)s; STUB(); return 0; }
void UndoHistory_SetSavePoint(UndoHistory *s)             { (void)s; STUB(); }
int  UndoHistory_IsSavePoint(const UndoHistory *s)        { (void)s; STUB(); return 0; }
int  UndoHistory_BeforeSavePoint(const UndoHistory *s)    { (void)s; STUB(); return 0; }
int  UndoHistory_PreviousBeforeSavePoint(const UndoHistory *s){(void)s;STUB();return 0;}
int  UndoHistory_BeforeReachableSavePoint(const UndoHistory *s){(void)s;STUB();return 0;}
int  UndoHistory_AfterSavePoint(const UndoHistory *s)     { (void)s; STUB(); return 0; }

void     UndoHistory_SetDetachPoint(UndoHistory *s, int a){(void)s;(void)a;STUB();}
int      UndoHistory_DetachPoint(const UndoHistory *s)    { (void)s; STUB(); return 0; }
int      UndoHistory_AfterDetachPoint(const UndoHistory *s){(void)s;STUB();return 0;}
int      UndoHistory_AfterOrAtDetachPoint(const UndoHistory *s){(void)s;STUB();return 0;}

intptr_t UndoHistory_Delta(const UndoHistory *s, int a)   { (void)s;(void)a;STUB();return 0;}
int      UndoHistory_Validate(const UndoHistory *s, intptr_t l){(void)s;(void)l;STUB();return 0;}
void     UndoHistory_SetCurrent(UndoHistory *s, int a, intptr_t l){(void)s;(void)a;(void)l;STUB();}
int      UndoHistory_Current(const UndoHistory *s)        { (void)s; STUB(); return 0; }

int          UndoHistory_Type(const UndoHistory *s, int a)    {(void)s;(void)a;STUB();return 0;}
SciPosition  UndoHistory_Position(const UndoHistory *s, int a){(void)s;(void)a;STUB();return 0;}
SciPosition  UndoHistory_Length(const UndoHistory *s, int a)  {(void)s;(void)a;STUB();return 0;}
SciStringView UndoHistory_Text(UndoHistory *s, int a)
    { (void)s;(void)a;STUB(); SciStringView sv={NULL,0};return sv; }
void UndoHistory_PushUndoActionType(UndoHistory *s, int t, SciPosition p)
    { (void)s;(void)t;(void)p;STUB(); }
void UndoHistory_ChangeLastUndoActionText(UndoHistory *s, size_t l, const char *t)
    { (void)s;(void)l;(void)t;STUB(); }

void UndoHistory_SetTentative(UndoHistory *s, int a){(void)s;(void)a;STUB();}
int  UndoHistory_TentativePoint(const UndoHistory *s) {(void)s;STUB();return 0;}
void UndoHistory_TentativeStart(UndoHistory *s)       {(void)s;STUB();}
void UndoHistory_TentativeCommit(UndoHistory *s)      {(void)s;STUB();}
int  UndoHistory_TentativeActive(const UndoHistory *s){(void)s;STUB();return 0;}
int  UndoHistory_TentativeSteps(const UndoHistory *s) {(void)s;STUB();return 0;}

int    UndoHistory_CanUndo(const UndoHistory *s)            {(void)s;STUB();return 0;}
int    UndoHistory_StartUndo(const UndoHistory *s)          {(void)s;STUB();return 0;}
Action UndoHistory_GetUndoStep(const UndoHistory *s)
    { (void)s;STUB(); Action a={0,0,0,NULL,0};return a; }
void   UndoHistory_CompletedUndoStep(UndoHistory *s)        {(void)s;STUB();}
int    UndoHistory_CanRedo(const UndoHistory *s)            {(void)s;STUB();return 0;}
int    UndoHistory_StartRedo(const UndoHistory *s)          {(void)s;STUB();return 0;}
Action UndoHistory_GetRedoStep(const UndoHistory *s)
    { (void)s;STUB(); Action a={0,0,0,NULL,0};return a; }
void   UndoHistory_CompletedRedoStep(UndoHistory *s)        {(void)s;STUB();}
