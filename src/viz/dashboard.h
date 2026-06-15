#ifndef TIERRA_VIZ_DASHBOARD_H
#define TIERRA_VIZ_DASHBOARD_H

#include "raylib.h"
#include "tierra/tierra.h"

/* Width of the population / genotype-diversity time-series plots, in
 * samples (one sample per dashboard_push call, i.e. one per frame). */
#define DASH_HISTORY 240

typedef struct Dashboard {
    float pop_hist[DASH_HISTORY];
    float geno_hist[DASH_HISTORY];

    /* live-tunable parameter mirrors, read by raygui sliders */
    float slice_size;
    float gen_per_bkg_mut;
    float gen_per_flaw;
    float gen_per_mov_mut;
    float gen_per_div_mut;
    float reap_rnd_prop;
    float mal_mode;
} Dashboard;

/* Seed the slider mirrors from the world's current parameters and clear
 * the history plots. Call again after a reseed. */
void dashboard_init(Dashboard *d, const TWorld *w);

/* Append one sample to the history plots. */
void dashboard_push(Dashboard *d, const TStats *st);

/* Draw counters, history plots, a size histogram, the genotype table and
 * the parameter sliders into `bounds`. Slider edits are applied to `w`
 * immediately via t_set_param. */
void dashboard_draw(Dashboard *d, TWorld *w, const TStats *st, Rectangle bounds);

#endif /* TIERRA_VIZ_DASHBOARD_H */
