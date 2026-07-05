/* pt_tpl.h — Partitioning struct + function declaration template
 *
 * Include with PT_T, PT_SUFFIX, PT_SV defined.
 *
 *   PT_T      — element type          (int or ptrdiff_t)
 *   PT_SUFFIX — name suffix           (Int or PD)
 *   PT_SV     — body SplitVector type (SplitVector_Int or SplitVector_PD)
 *
 * The PT_STRUCT and PT_FN macros are defined in Partitioning.h before
 * this file is included so they work across both inclusions.
 */

#ifndef PT_T
#  error "pt_tpl.h requires PT_T defined"
#endif

typedef struct PT_STRUCT {
    PT_T   stepPartition;
    PT_T   stepLength;
    PT_SV  body;
} PT_STRUCT;

void   PT_FN(init)                      (PT_STRUCT *self);
void   PT_FN(destroy)                   (PT_STRUCT *self);
PT_T   PT_FN(Partitions)                (const PT_STRUCT *self);
void   PT_FN(ReAllocate)                (PT_STRUCT *self, ptrdiff_t newSize);
PT_T   PT_FN(Length)                    (const PT_STRUCT *self);
void   PT_FN(InsertPartition)           (PT_STRUCT *self, PT_T partition, PT_T pos);
void   PT_FN(InsertPartitions)          (PT_STRUCT *self, PT_T partition,
                                          const PT_T *positions, size_t length);
void   PT_FN(InsertPartitionsWithCast)  (PT_STRUCT *self, PT_T partition,
                                          const ptrdiff_t *positions, size_t length);
void   PT_FN(SetPartitionStartPosition) (PT_STRUCT *self, PT_T partition, PT_T pos);
void   PT_FN(InsertText)                (PT_STRUCT *self, PT_T partitionInsert,
                                          PT_T delta);
void   PT_FN(RemovePartition)           (PT_STRUCT *self, PT_T partition);
PT_T   PT_FN(PositionFromPartition)     (const PT_STRUCT *self, PT_T partition);
PT_T   PT_FN(PartitionFromPosition)     (const PT_STRUCT *self, PT_T pos);
void   PT_FN(DeleteAll)                 (PT_STRUCT *self);
