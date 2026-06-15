#ifndef TIERRA_REAPER_H
#define TIERRA_REAPER_H

#include "world.h"

/* Append cell `idx` at the bottom (safest) end of the reaper queue
 * (legacy EntBotReaper). `idx` must not already be in the queue. */
void reaper_insert_bottom(TWorld *w, int32_t idx);

/* Remove cell `idx` from the reaper queue (legacy RmvFrmReaper). */
void reaper_remove(TWorld *w, int32_t idx);

/* Bubble `idx` one slot toward the top (worst) end if its error count is
 * now >= its upward neighbour's (legacy UpRprIf). Call after d.flags++. */
void reaper_up_if(TWorld *w, int32_t idx);

/* Bubble `idx` one slot toward the bottom (safest) end if its error count
 * is <= its downward neighbour's (legacy DownReperIf). Call after a
 * successful mal() or divide(). */
void reaper_down_if(TWorld *w, int32_t idx);

/* Free a cell's memory back to the soup, drop it from the genebank's
 * population count, remove it from both queues, and return its slot to
 * the free pool. */
void reap_cell(TWorld *w, int32_t idx);

/* Pick a cell from the top ReapRndProp fraction of the reaper queue and
 * reap it (legacy reaper(), the REAP_SOUP_FULL / lazy / disturb path,
 * minus the locality search). `ex` is nonzero if called while cell
 * `ce_idx` is mid-instruction; in that case `ce_idx` itself is never
 * picked (as long as more than one cell is alive). Returns 0 if a cell was
 * reaped, nonzero if the population is at its floor and nothing was
 * reaped. */
int reaper_reap(TWorld *w, int ex, int32_t ce_idx);

/* If w->ce looks like it has stopped reproducing (repinst > avg_repinst *
 * LazyTol), reap it and repeat for the new w->ce (legacy's post-slice lazy
 * cull loop in RanSlicerQueue). No-op until at least one divide has
 * happened (avg_repinst is otherwise meaningless). */
void reaper_lazy_cull(TWorld *w);

#endif /* TIERRA_REAPER_H */
