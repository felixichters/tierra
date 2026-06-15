#ifndef TIERRA_SLICER_H
#define TIERRA_SLICER_H

#include "world.h"

/* Splice cell `idx` into the circular slicer queue immediately before
 * `before` (legacy EntBotSlicer). `idx` must not already be in the queue. */
void slicer_insert_before(TWorld *w, int32_t idx, int32_t before);

/* Remove cell `idx` from the circular slicer queue (legacy RmvFrmSlicer).
 * If `idx == w->ce`, advances w->ce to idx's successor, or to -1 if `idx`
 * was the only cell in the queue. */
void slicer_remove(TWorld *w, int32_t idx);

/* Advance w->ce to the next cell in the circular queue (IncrSliceQueue). */
void slicer_advance(TWorld *w);

/* Random per-turn instruction budget for cell `c`, combining SliceSize/
 * SizDepSlice/SlicePow/SlicFixFrac/SlicRanFrac (legacy RanSlicerQueue, the
 * default SliceStyle=2). With the stock config this is
 * uniform(0, 2*SliceSize). */
int32_t slicer_slice_size(TWorld *w, const TCell *c);

#endif /* TIERRA_SLICER_H */
