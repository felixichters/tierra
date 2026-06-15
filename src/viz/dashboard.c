#include "dashboard.h"

#include "raygui.h"

#include <stdio.h>
#include <string.h>

#define TOP_N 8

void dashboard_init(Dashboard *d, const TWorld *w) {
    memset(d->pop_hist, 0, sizeof(d->pop_hist));
    memset(d->geno_hist, 0, sizeof(d->geno_hist));

    d->slice_size      = (float)t_get_param(w, T_PARAM_SLICE_SIZE);
    d->gen_per_bkg_mut = (float)t_get_param(w, T_PARAM_GEN_PER_BKG_MUT);
    d->gen_per_flaw    = (float)t_get_param(w, T_PARAM_GEN_PER_FLAW);
    d->gen_per_mov_mut = (float)t_get_param(w, T_PARAM_GEN_PER_MOV_MUT);
    d->gen_per_div_mut = (float)t_get_param(w, T_PARAM_GEN_PER_DIV_MUT);
    d->reap_rnd_prop   = (float)t_get_param(w, T_PARAM_REAP_RND_PROP);
    d->mal_mode        = (float)t_get_param(w, T_PARAM_MAL_MODE);
}

void dashboard_push(Dashboard *d, const TStats *st) {
    memmove(d->pop_hist, d->pop_hist + 1, (DASH_HISTORY - 1) * sizeof(float));
    memmove(d->geno_hist, d->geno_hist + 1, (DASH_HISTORY - 1) * sizeof(float));
    d->pop_hist[DASH_HISTORY - 1] = (float)st->num_cells;
    d->geno_hist[DASH_HISTORY - 1] = (float)st->num_genotypes;
}

static void draw_plot(Rectangle r, const float *hist, Color col, const char *label) {
    DrawRectangleLinesEx(r, 1.0f, GRAY);
    DrawText(label, (int)r.x + 4, (int)r.y + 2, 10, LIGHTGRAY);

    float max_val = 1.0f;
    for (int i = 0; i < DASH_HISTORY; i++)
        if (hist[i] > max_val)
            max_val = hist[i];

    for (int i = 1; i < DASH_HISTORY; i++) {
        float x0 = r.x + r.width * (float)(i - 1) / (float)(DASH_HISTORY - 1);
        float x1 = r.x + r.width * (float)i / (float)(DASH_HISTORY - 1);
        float y0 = r.y + r.height - (hist[i - 1] / max_val) * (r.height - 4) - 2;
        float y1 = r.y + r.height - (hist[i] / max_val) * (r.height - 4) - 2;
        DrawLine((int)x0, (int)y0, (int)x1, (int)y1, col);
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", max_val);
    DrawText(buf, (int)(r.x + r.width) - 30, (int)r.y + 2, 10, LIGHTGRAY);
}

/* Histogram of live-cell sizes, bucketed into NBINS bins spanning
 * [min_size, max_size]. */
static void draw_size_histogram(Rectangle r, const TWorld *w) {
    DrawRectangleLinesEx(r, 1.0f, GRAY);
    DrawText("size histogram", (int)r.x + 4, (int)r.y + 2, 10, LIGHTGRAY);

    int ncells = 0;
    const TCellView *cv = t_cells(w, &ncells);
    if (ncells == 0)
        return;

    int32_t min_size = cv[0].size, max_size = cv[0].size;
    for (int i = 1; i < ncells; i++) {
        if (cv[i].size < min_size) min_size = cv[i].size;
        if (cv[i].size > max_size) max_size = cv[i].size;
    }

    #define NBINS 24
    int bins[NBINS] = {0};
    int32_t span = max_size - min_size + 1;
    for (int i = 0; i < ncells; i++) {
        int b = (int)(((int64_t)(cv[i].size - min_size) * NBINS) / span);
        if (b >= NBINS) b = NBINS - 1;
        bins[b]++;
    }

    int max_count = 1;
    for (int b = 0; b < NBINS; b++)
        if (bins[b] > max_count)
            max_count = bins[b];

    float plot_top = r.y + 16, plot_h = r.height - 20;
    float bin_w = r.width / NBINS;
    for (int b = 0; b < NBINS; b++) {
        if (bins[b] == 0)
            continue;
        float h = plot_h * (float)bins[b] / (float)max_count;
        DrawRectangle((int)(r.x + b * bin_w) + 1, (int)(plot_top + plot_h - h),
                       (int)bin_w - 1, (int)h, SKYBLUE);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%d .. %d instructions", min_size, max_size);
    DrawText(buf, (int)r.x + 4, (int)(r.y + r.height) - 12, 10, LIGHTGRAY);
    #undef NBINS
}

/* Top-N genotypes by population, kept in a sorted index list built with a
 * single pass insertion -- ngeno is typically small (tens to low hundreds). */
static void draw_genotype_table(Rectangle r, const TWorld *w) {
    DrawRectangleLinesEx(r, 1.0f, GRAY);
    DrawText("top genotypes", (int)r.x + 4, (int)r.y + 2, 10, LIGHTGRAY);

    int ngeno = 0;
    const TGenoView *gv = t_genotypes(w, &ngeno);

    int top_idx[TOP_N];
    int top_n = 0;
    for (int i = 0; i < ngeno; i++) {
        if (top_n == TOP_N && gv[i].pop <= gv[top_idx[TOP_N - 1]].pop)
            continue;
        int pos = (top_n < TOP_N) ? top_n++ : TOP_N - 1;
        while (pos > 0 && gv[top_idx[pos - 1]].pop < gv[i].pop) {
            top_idx[pos] = top_idx[pos - 1];
            pos--;
        }
        top_idx[pos] = i;
    }

    int line_y = (int)r.y + 18;
    DrawText("name      size   pop", (int)r.x + 4, line_y, 10, LIGHTGRAY);
    line_y += 14;
    for (int i = 0; i < top_n; i++) {
        const TGenoView *g = &gv[top_idx[i]];
        Color col = (Color){
            .r = (unsigned char)(g->colour >> 24), .g = (unsigned char)(g->colour >> 16),
            .b = (unsigned char)(g->colour >> 8),  .a = 255,
        };
        DrawRectangle((int)r.x + 4, line_y + 1, 8, 8, col);
        char buf[64];
        snprintf(buf, sizeof(buf), "%-8s %6d %6d", g->name, g->size, g->pop);
        DrawText(buf, (int)r.x + 16, line_y, 10, RAYWHITE);
        line_y += 14;
    }
}

/* One labelled slider; applies the new value via t_set_param immediately. */
static void param_slider(Rectangle r, const char *label, float *mirror,
                          float lo, float hi, TWorld *w, TParam p) {
    char val[16];
    snprintf(val, sizeof(val), "%.0f", *mirror);
    GuiSlider(r, label, val, mirror, lo, hi);
    t_set_param(w, p, (double)*mirror);
}

void dashboard_draw(Dashboard *d, TWorld *w, const TStats *st, Rectangle b) {
    DrawRectangleLinesEx(b, 1.0f, GRAY);

    float x = b.x + 8, y = b.y + 6, lw = b.width - 16;
    int fs = 16, lh = 19;
    char buf[96];

    snprintf(buf, sizeof(buf), "instructions: %llu  (%.0f inst/s)",
             (unsigned long long)st->inst_executed, st->speed);
    DrawText(buf, (int)x, (int)y, fs, RAYWHITE); y += lh;

    snprintf(buf, sizeof(buf), "population: %d   generations: %.2f",
             st->num_cells, st->generations);
    DrawText(buf, (int)x, (int)y, fs, RAYWHITE); y += lh;

    snprintf(buf, sizeof(buf), "genotypes: %d   sizes: %d   avg size: %.1f",
             st->num_genotypes, st->num_sizes, st->avg_size);
    DrawText(buf, (int)x, (int)y, fs, RAYWHITE); y += lh;

    snprintf(buf, sizeof(buf), "occupancy: %.1f%%   avg fecundity: %.2f",
             st->mem_occupancy * 100.0, st->avg_fecundity);
    DrawText(buf, (int)x, (int)y, fs, RAYWHITE); y += lh;

    snprintf(buf, sizeof(buf), "mutations: %llu   flaws: %llu",
             (unsigned long long)st->mutations, (unsigned long long)st->flaws);
    DrawText(buf, (int)x, (int)y, fs, RAYWHITE); y += lh;

    snprintf(buf, sizeof(buf), "births: %llu   deaths: %llu",
             (unsigned long long)st->births, (unsigned long long)st->deaths);
    DrawText(buf, (int)x, (int)y, fs, RAYWHITE); y += lh + 6;

    draw_plot((Rectangle){x, y, lw, 64}, d->pop_hist, LIME, "population");
    y += 70;
    draw_plot((Rectangle){x, y, lw, 64}, d->geno_hist, SKYBLUE, "genotypes");
    y += 70 + 6;

    draw_size_histogram((Rectangle){x, y, lw, 90}, w);
    y += 96 + 6;

    draw_genotype_table((Rectangle){x, y, lw, 18 + 14 * (1 + TOP_N)}, w);
    y += 18 + 14 * (1 + TOP_N) + 12;

    DrawText("parameters (live)", (int)x, (int)y, 12, LIGHTGRAY);
    y += 18;

    float sh = 18, sgap = 26;
    param_slider((Rectangle){x + 110, y, lw - 150, sh}, "SliceSize",
                  &d->slice_size, 1, 200, w, T_PARAM_SLICE_SIZE); y += sgap;
    param_slider((Rectangle){x + 110, y, lw - 150, sh}, "GenPerBkgMut",
                  &d->gen_per_bkg_mut, 0, 200, w, T_PARAM_GEN_PER_BKG_MUT); y += sgap;
    param_slider((Rectangle){x + 110, y, lw - 150, sh}, "GenPerFlaw",
                  &d->gen_per_flaw, 0, 200, w, T_PARAM_GEN_PER_FLAW); y += sgap;
    param_slider((Rectangle){x + 110, y, lw - 150, sh}, "GenPerMovMut",
                  &d->gen_per_mov_mut, 0, 200, w, T_PARAM_GEN_PER_MOV_MUT); y += sgap;
    param_slider((Rectangle){x + 110, y, lw - 150, sh}, "GenPerDivMut",
                  &d->gen_per_div_mut, 0, 200, w, T_PARAM_GEN_PER_DIV_MUT); y += sgap;
    param_slider((Rectangle){x + 110, y, lw - 150, sh}, "ReapRndProp",
                  &d->reap_rnd_prop, 0.0f, 1.0f, w, T_PARAM_REAP_RND_PROP); y += sgap;
    param_slider((Rectangle){x + 110, y, lw - 150, sh}, "MalMode",
                  &d->mal_mode, 0, 2, w, T_PARAM_MAL_MODE); y += sgap;
}
