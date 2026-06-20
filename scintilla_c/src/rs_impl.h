/* rs_impl.h — RunStyles implementation template
 *
 * Include with RS_D, RS_S, RS_DS, RS_SS, RS_S_ZERO defined.
 * RS_STRUCT / RS_FN / RS_P / RS_P_FN / RS_SV / RS_SV_FN / RS_FR
 * must already be defined by RunStyles.c before each inclusion.
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   Private helpers (RunFromPosition, SplitRun, RemoveRun,
 *   RemoveRunIfEmpty, RemoveRunIfSameAsPrevious) → static functions
 *   prefixed with the same RS_FN() macro.
 *
 *   starts.PartitionFromPosition(p)  → RS_P_FN(PartitionFromPosition)(&self->starts, p)
 *   starts.PositionFromPartition(r)  → RS_P_FN(PositionFromPartition)(&self->starts, r)
 *   starts.Partitions()              → RS_P_FN(Partitions)(&self->starts)
 *   starts.InsertPartition(r,p)      → RS_P_FN(InsertPartition)(&self->starts, r, p)
 *   starts.InsertPartitions(r,arr,n) → RS_P_FN(InsertPartitions)(&self->starts, r, arr, n)
 *   starts.RemovePartition(r)        → RS_P_FN(RemovePartition)(&self->starts, r)
 *   starts.InsertText(r,d)           → RS_P_FN(InsertText)(&self->starts, r, d)
 *   starts.DeleteAll()               → RS_P_FN(DeleteAll)(&self->starts)
 *
 *   styles.ValueAt(r)                → RS_SV_FN(ValueAt)(&self->styles, r)
 *   styles.SetValueAt(r,v)           → RS_SV_FN(SetValueAt)(&self->styles, r, v)
 *   styles.InsertValue(r,n,v)        → RS_SV_FN(InsertValue)(&self->styles, r, n, v)
 *   styles.Insert(r,v)               → RS_SV_FN(Insert)(&self->styles, r, v)
 *   styles.DeleteRange(r,1)          → RS_SV_FN(DeleteRange)(&self->styles, r, 1)
 *   styles.Length()                  → RS_SV_FN(Length)(&self->styles)
 *
 *   starts = Partitioning<D>()       → RS_P_FN(destroy)+RS_P_FN(init)
 *   styles = SplitVector<S>()        → RS_SV_FN(destroy)+RS_SV_FN(init)
 *
 *   STYLE()  (value-init)            → (RS_S)(RS_S_ZERO)
 *
 *   throw std::runtime_error(…)      → assert(0); abort()
 *   bool return                      → int (0/1)
 */

#ifndef RS_D
#  error "rs_impl.h requires RS_D defined"
#endif

#include <stdlib.h>
#include <assert.h>

/* ── Private: RunFromPosition ────────────────────────────────────────
 *
 * Find the first run at or before position.
 *
 * C++ original:
 *   DISTANCE RunFromPosition(DISTANCE position) const noexcept {
 *       DISTANCE run = starts.PartitionFromPosition(position);
 *       while ((run > 0) && (position == starts.PositionFromPartition(run-1)))
 *           run--;
 *       return run;
 *   }
 */
static RS_D RS_FN(RunFromPosition)(const RS_STRUCT *self, RS_D position) {
    RS_D run = RS_P_FN(PartitionFromPosition)(&self->starts, position);
    while ((run > 0) &&
           (position == RS_P_FN(PositionFromPartition)(&self->starts, run - 1)))
        run--;
    return run;
}

/* ── Private: SplitRun ───────────────────────────────────────────────
 *
 * If no run boundary exists at position, insert one.
 *
 * C++ original:
 *   DISTANCE SplitRun(DISTANCE position) {
 *       DISTANCE run = RunFromPosition(position);
 *       const DISTANCE posRun = starts.PositionFromPartition(run);
 *       if (posRun < position) {
 *           STYLE runStyle = ValueAt(position);
 *           run++;
 *           starts.InsertPartition(run, position);
 *           styles.InsertValue(run, 1, runStyle);
 *       }
 *       return run;
 *   }
 */
static RS_D RS_FN(SplitRun)(RS_STRUCT *self, RS_D position) {
    RS_D run = RS_FN(RunFromPosition)(self, position);
    const RS_D posRun = RS_P_FN(PositionFromPartition)(&self->starts, run);
    if (posRun < position) {
        RS_S runStyle = RS_FN(ValueAt)(self, position);
        run++;
        RS_P_FN(InsertPartition)(&self->starts, run, position);
        RS_SV_FN(InsertValue)(&self->styles, (ptrdiff_t)run, 1, runStyle);
    }
    return run;
}

/* ── Private: RemoveRun ──────────────────────────────────────────────
 *
 * C++ original:
 *   void RemoveRun(DISTANCE run) {
 *       starts.RemovePartition(run);
 *       styles.DeleteRange(run, 1);
 *   }
 */
static void RS_FN(RemoveRun)(RS_STRUCT *self, RS_D run) {
    RS_P_FN(RemovePartition)(&self->starts, run);
    RS_SV_FN(DeleteRange)(&self->styles, (ptrdiff_t)run, 1);
}

/* ── Private: RemoveRunIfEmpty ───────────────────────────────────────
 *
 * C++ original:
 *   void RemoveRunIfEmpty(DISTANCE run) {
 *       if ((run < starts.Partitions()) && (starts.Partitions() > 1)) {
 *           if (starts.PositionFromPartition(run) ==
 *               starts.PositionFromPartition(run+1))
 *               RemoveRun(run);
 *       }
 *   }
 */
static void RS_FN(RemoveRunIfEmpty)(RS_STRUCT *self, RS_D run) {
    const RS_D parts = RS_P_FN(Partitions)(&self->starts);
    if ((run < parts) && (parts > 1)) {
        if (RS_P_FN(PositionFromPartition)(&self->starts, run) ==
            RS_P_FN(PositionFromPartition)(&self->starts, run + 1))
            RS_FN(RemoveRun)(self, run);
    }
}

/* ── Private: RemoveRunIfSameAsPrevious ──────────────────────────────
 *
 * C++ original:
 *   void RemoveRunIfSameAsPrevious(DISTANCE run) {
 *       if ((run > 0) && (run < starts.Partitions())) {
 *           const DISTANCE runBefore = run - 1;
 *           if (styles.ValueAt(runBefore) == styles.ValueAt(run))
 *               RemoveRun(run);
 *       }
 *   }
 */
static void RS_FN(RemoveRunIfSameAsPrevious)(RS_STRUCT *self, RS_D run) {
    if ((run > 0) && (run < RS_P_FN(Partitions)(&self->starts))) {
        const RS_D runBefore = run - 1;
        if (RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)runBefore) ==
            RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)run))
            RS_FN(RemoveRun)(self, run);
    }
}

/* ── init ─────────────────────────────────────────────────────────────
 *
 * C++ original constructor:
 *   RunStyles() { styles.InsertValue(0, 2, 0); }
 *
 * Partitioning is default-constructed: already has 2 partitions from
 * Partitioning_*_init() (which calls Insert(0,0) + Insert(1,0)).
 * styles gets two zero entries: one for the run, one sentinel at end.
 */
void RS_FN(init)(RS_STRUCT *self) {
    RS_P_FN(init)(&self->starts);
    RS_SV_FN(init)(&self->styles);
    RS_SV_FN(InsertValue)(&self->styles, 0, 2, (RS_S)RS_S_ZERO);
}

/* ── destroy ──────────────────────────────────────────────────────── */
void RS_FN(destroy)(RS_STRUCT *self) {
    RS_P_FN(destroy)(&self->starts);
    RS_SV_FN(destroy)(&self->styles);
}

/* ── Length ───────────────────────────────────────────────────────── */
RS_D RS_FN(Length)(const RS_STRUCT *self) {
    return RS_P_FN(PositionFromPartition)(&self->starts,
               RS_P_FN(Partitions)(&self->starts));
}

/* ── ValueAt ──────────────────────────────────────────────────────── */
RS_S RS_FN(ValueAt)(const RS_STRUCT *self, RS_D position) {
    return RS_SV_FN(ValueAt)(&self->styles,
               (ptrdiff_t)RS_P_FN(PartitionFromPosition)(&self->starts, position));
}

/* ── FindNextChange ───────────────────────────────────────────────── */
RS_D RS_FN(FindNextChange)(const RS_STRUCT *self, RS_D position, RS_D end) {
    const RS_D run = RS_P_FN(PartitionFromPosition)(&self->starts, position);
    if (run < RS_P_FN(Partitions)(&self->starts)) {
        const RS_D runChange = RS_P_FN(PositionFromPartition)(&self->starts, run);
        if (runChange > position)
            return runChange;
        const RS_D nextChange = RS_P_FN(PositionFromPartition)(&self->starts, run + 1);
        if (nextChange > position)
            return nextChange;
        else if (position < end)
            return end;
        else
            return end + 1;
    }
    return end + 1;
}

/* ── StartRun ─────────────────────────────────────────────────────── */
RS_D RS_FN(StartRun)(const RS_STRUCT *self, RS_D position) {
    return RS_P_FN(PositionFromPartition)(&self->starts,
               RS_P_FN(PartitionFromPosition)(&self->starts, position));
}

/* ── EndRun ───────────────────────────────────────────────────────── */
RS_D RS_FN(EndRun)(const RS_STRUCT *self, RS_D position) {
    return RS_P_FN(PositionFromPartition)(&self->starts,
               RS_P_FN(PartitionFromPosition)(&self->starts, position) + 1);
}

/* ── FillRange ────────────────────────────────────────────────────────
 *
 * Fill [position, position+fillLength) with value.  Merges or splits
 * runs as needed.  Returns the actual changed range (may be smaller
 * than requested if some of it already had the value).
 *
 * C++ translation notes:
 *   const DISTANCE range[] { position, end }
 *   → RS_D range[2] = { position, end };
 *
 *   styles.Insert(runEndIndex + 1, value)
 *   → RS_SV_FN(Insert)(&self->styles, (ptrdiff_t)(runEndIndex + 1), value)
 *
 *   { true, position, fillLength }  (aggregate init)
 *   → (RS_FR){ 1, position, fillLength }
 */
RS_FR RS_FN(FillRange)(RS_STRUCT *self, RS_D position, RS_S value, RS_D fillLength) {
    const RS_FR resultNoChange = { 0, position, fillLength };
    if (fillLength <= 0)
        return resultNoChange;
    RS_D end = position + fillLength;
    if (end > RS_FN(Length)(self))
        return resultNoChange;

    RS_D runEnd = RS_FN(RunFromPosition)(self, end);
    const RS_S valueCurrent = RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)runEnd);

    if (valueCurrent == value) {
        /* End already has value — trim range */
        end = RS_P_FN(PositionFromPartition)(&self->starts, runEnd);
        if (position >= end)
            return resultNoChange;
        fillLength = end - position;
    } else {
        const RS_D startRun = RS_P_FN(PositionFromPartition)(&self->starts, runEnd);
        if (position > startRun) {
            const RS_D runNext = runEnd + 1;
            const RS_D endRun = RS_P_FN(PositionFromPartition)(&self->starts, runNext);
            if (end < endRun) {
                /* New piece completely inside a run — insert two boundaries */
                RS_D range[2] = { position, end };
                RS_P_FN(InsertPartitions)(&self->starts, runEnd + 1, range, 2);
                const ptrdiff_t runEndIndex = (ptrdiff_t)runEnd;
                RS_SV_FN(Insert)(&self->styles, runEndIndex + 1, value);
                RS_SV_FN(Insert)(&self->styles, runEndIndex + 2, valueCurrent);
                RS_FR r = { 1, position, fillLength };
                return r;
            }
        }
        runEnd = RS_FN(SplitRun)(self, end);
    }

    RS_D runStart = RS_FN(RunFromPosition)(self, position);
    if (RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)runStart) == value) {
        /* Start already has value — trim range */
        runStart++;
        position   = RS_P_FN(PositionFromPartition)(&self->starts, runStart);
        fillLength = end - position;
    } else {
        if (RS_P_FN(PositionFromPartition)(&self->starts, runStart) < position) {
            runStart = RS_FN(SplitRun)(self, position);
            runEnd++;
        }
    }

    if (runStart < runEnd) {
        RS_FR result = { 1, position, fillLength };
        RS_SV_FN(SetValueAt)(&self->styles, (ptrdiff_t)runStart, value);
        /* Remove each old run over the range */
        for (RS_D run = runStart + 1; run < runEnd; run++)
            RS_FN(RemoveRun)(self, runStart + 1);
        runEnd = RS_FN(RunFromPosition)(self, end);
        RS_FN(RemoveRunIfSameAsPrevious)(self, runEnd);
        RS_FN(RemoveRunIfSameAsPrevious)(self, runStart);
        runEnd = RS_FN(RunFromPosition)(self, end);
        RS_FN(RemoveRunIfEmpty)(self, runEnd);
        return result;
    }
    return resultNoChange;
}

/* ── SetValueAt ───────────────────────────────────────────────────── */
void RS_FN(SetValueAt)(RS_STRUCT *self, RS_D position, RS_S value) {
    RS_FN(FillRange)(self, position, value, 1);
}

/* ── InsertSpace ──────────────────────────────────────────────────────
 *
 * C++ original:
 *   void InsertSpace(DISTANCE position, DISTANCE insertLength) {
 *       DISTANCE runStart = RunFromPosition(position);
 *       if (starts.PositionFromPartition(runStart) == position) {
 *           STYLE runStyle = ValueAt(position);
 *           if (runStart == 0) {
 *               if (runStyle) {
 *                   styles.SetValueAt(0, STYLE());
 *                   starts.InsertPartition(1, 0);
 *                   styles.InsertValue(1, 1, runStyle);
 *                   starts.InsertText(0, insertLength);
 *               } else {
 *                   starts.InsertText(runStart, insertLength);
 *               }
 *           } else {
 *               if (runStyle) {
 *                   starts.InsertText(runStart-1, insertLength);
 *               } else {
 *                   starts.InsertText(runStart, insertLength);
 *               }
 *           }
 *       } else {
 *           starts.InsertText(runStart, insertLength);
 *       }
 *   }
 *
 *   STYLE() → (RS_S)(RS_S_ZERO)
 */
void RS_FN(InsertSpace)(RS_STRUCT *self, RS_D position, RS_D insertLength) {
    RS_D runStart = RS_FN(RunFromPosition)(self, position);
    if (RS_P_FN(PositionFromPartition)(&self->starts, runStart) == position) {
        RS_S runStyle = RS_FN(ValueAt)(self, position);
        if (runStart == 0) {
            if (runStyle) {
                RS_SV_FN(SetValueAt)(&self->styles, 0, (RS_S)RS_S_ZERO);
                RS_P_FN(InsertPartition)(&self->starts, 1, 0);
                RS_SV_FN(InsertValue)(&self->styles, 1, 1, runStyle);
                RS_P_FN(InsertText)(&self->starts, 0, insertLength);
            } else {
                RS_P_FN(InsertText)(&self->starts, runStart, insertLength);
            }
        } else {
            if (runStyle) {
                RS_P_FN(InsertText)(&self->starts, runStart - 1, insertLength);
            } else {
                RS_P_FN(InsertText)(&self->starts, runStart, insertLength);
            }
        }
    } else {
        RS_P_FN(InsertText)(&self->starts, runStart, insertLength);
    }
}

/* ── DeleteAll ────────────────────────────────────────────────────────
 *
 * C++ original:
 *   void DeleteAll() {
 *       starts = Partitioning<DISTANCE>();   // re-construct
 *       styles = SplitVector<STYLE>();       // re-construct
 *       styles.InsertValue(0, 2, 0);
 *   }
 */
void RS_FN(DeleteAll)(RS_STRUCT *self) {
    RS_P_FN(destroy)(&self->starts);
    RS_P_FN(init)(&self->starts);
    RS_SV_FN(destroy)(&self->styles);
    RS_SV_FN(init)(&self->styles);
    RS_SV_FN(InsertValue)(&self->styles, 0, 2, (RS_S)RS_S_ZERO);
}

/* ── DeleteRange ──────────────────────────────────────────────────────
 *
 * C++ original:
 *   void DeleteRange(DISTANCE position, DISTANCE deleteLength) {
 *       DISTANCE end = position + deleteLength;
 *       DISTANCE runStart = RunFromPosition(position);
 *       DISTANCE runEnd   = RunFromPosition(end);
 *       if (runStart == runEnd) {
 *           starts.InsertText(runStart, -deleteLength);
 *           RemoveRunIfEmpty(runStart);
 *       } else {
 *           runStart = SplitRun(position);
 *           runEnd   = SplitRun(end);
 *           starts.InsertText(runStart, -deleteLength);
 *           for (DISTANCE run = runStart; run < runEnd; run++)
 *               RemoveRun(runStart);
 *           RemoveRunIfEmpty(runStart);
 *           RemoveRunIfSameAsPrevious(runStart);
 *       }
 *   }
 */
void RS_FN(DeleteRange)(RS_STRUCT *self, RS_D position, RS_D deleteLength) {
    const RS_D end      = position + deleteLength;
    RS_D runStart = RS_FN(RunFromPosition)(self, position);
    RS_D runEnd   = RS_FN(RunFromPosition)(self, end);
    if (runStart == runEnd) {
        RS_P_FN(InsertText)(&self->starts, runStart, -deleteLength);
        RS_FN(RemoveRunIfEmpty)(self, runStart);
    } else {
        runStart = RS_FN(SplitRun)(self, position);
        runEnd   = RS_FN(SplitRun)(self, end);
        RS_P_FN(InsertText)(&self->starts, runStart, -deleteLength);
        for (RS_D run = runStart; run < runEnd; run++)
            RS_FN(RemoveRun)(self, runStart);
        RS_FN(RemoveRunIfEmpty)(self, runStart);
        RS_FN(RemoveRunIfSameAsPrevious)(self, runStart);
    }
}

/* ── Runs ─────────────────────────────────────────────────────────── */
RS_D RS_FN(Runs)(const RS_STRUCT *self) {
    return RS_P_FN(Partitions)(&self->starts);
}

/* ── AllSame ──────────────────────────────────────────────────────── */
int RS_FN(AllSame)(const RS_STRUCT *self) {
    for (RS_D run = 1; run < RS_P_FN(Partitions)(&self->starts); run++) {
        if (RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)run) !=
            RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)(run - 1)))
            return 0;
    }
    return 1;
}

/* ── AllSameAs ────────────────────────────────────────────────────── */
int RS_FN(AllSameAs)(const RS_STRUCT *self, RS_S value) {
    return RS_FN(AllSame)(self) &&
           (RS_SV_FN(ValueAt)(&self->styles, 0) == value);
}

/* ── Find ─────────────────────────────────────────────────────────── */
RS_D RS_FN(Find)(const RS_STRUCT *self, RS_S value, RS_D start) {
    if (start < RS_FN(Length)(self)) {
        RS_D run = start ? RS_FN(RunFromPosition)(self, start) : 0;
        if (RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)run) == value)
            return start;
        run++;
        while (run < RS_P_FN(Partitions)(&self->starts)) {
            if (RS_SV_FN(ValueAt)(&self->styles, (ptrdiff_t)run) == value)
                return RS_P_FN(PositionFromPartition)(&self->starts, run);
            run++;
        }
    }
    return (RS_D)(-1);
}

/* ── Check ────────────────────────────────────────────────────────────
 *
 * Debug validator.  throw → assert(0); abort()
 */
void RS_FN(Check)(const RS_STRUCT *self) {
    if (RS_FN(Length)(self) < 0) {
        assert(!"RunStyles: Length cannot be negative.");
        abort();
    }
    if (RS_P_FN(Partitions)(&self->starts) < 1) {
        assert(!"RunStyles: Must always have 1 or more partitions.");
        abort();
    }
    if (RS_P_FN(Partitions)(&self->starts) !=
        RS_SV_FN(Length)(&self->styles) - 1) {
        assert(!"RunStyles: Partitions and styles different lengths.");
        abort();
    }
    RS_D start = 0;
    while (start < RS_FN(Length)(self)) {
        const RS_D end = RS_FN(EndRun)(self, start);
        if (start >= end) {
            assert(!"RunStyles: Partition is 0 length.");
            abort();
        }
        start = end;
    }
    if (RS_SV_FN(ValueAt)(&self->styles,
                            RS_SV_FN(Length)(&self->styles) - 1) != (RS_S)RS_S_ZERO) {
        assert(!"RunStyles: Unused style at end changed.");
        abort();
    }
    for (ptrdiff_t j = 1; j < RS_SV_FN(Length)(&self->styles) - 1; j++) {
        if (RS_SV_FN(ValueAt)(&self->styles, j) ==
            RS_SV_FN(ValueAt)(&self->styles, j - 1)) {
            assert(!"RunStyles: Style of a partition same as previous.");
            abort();
        }
    }
}
