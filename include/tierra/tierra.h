/* libtierra public API -- clean-room reimplementation of Tom Ray's Tierra. */
#ifndef TIERRA_H
#define TIERRA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Configuration ----------------------------------------------------- */

typedef struct TConfig {
    /* soup & memory allocation */
    int32_t soup_size;        /* SoupSize: total soup memory, in instructions */
    int32_t min_cell_size;    /* MinCellSize: minimum size for a cell */
    int32_t min_genome_size;  /* MinGenMemSiz: minimum genetic memory size */
    int32_t mal_mode;         /* MalMode: 0=first-fit, 1=better-fit, 2=random */
    double  mal_tol;          /* MalTol: free-block search range, x avg size */

    /* slicer (CPU time allocation) */
    int32_t slice_size;       /* SliceSize: base instructions per slice */
    int32_t size_dep_slice;   /* SizDepSlice: scale slice size by creature size */
    double  slice_pow;        /* SlicePow: exponent for size-dependent slicing */
    double  slic_fix_frac;    /* SlicFixFrac: fixed fraction of slice size */
    double  slic_ran_frac;    /* SlicRanFrac: random fraction of slice size */

    /* reaper */
    double  reap_rnd_prop;    /* ReapRndProp: top fraction of reaper queue reaped */
    int32_t lazy_tol;         /* LazyTol: tolerance for non-reproductive cells */

    /* template matching */
    double  search_limit;     /* SearchLimit: template search range, x avg size */
    int32_t min_templ_size;   /* MinTemplSize: minimum template length */

    /* mutation rates, all expressed as "1 event per N generations" */
    int32_t gen_per_bkg_mut;        /* GenPerBkgMut: cosmic-ray bit flips */
    int32_t gen_per_flaw;           /* GenPerFlaw: flaw injection */
    int32_t gen_per_mov_mut;        /* GenPerMovMut: copy mutations during movii */
    int32_t gen_per_div_mut;        /* GenPerDivMut: point mutation at divide */
    int32_t gen_per_ins_ins;        /* GenPerInsIns: single-instruction insertion */
    int32_t gen_per_del_ins;        /* GenPerDelIns: single-instruction deletion */
    int32_t gen_per_cro_ins;        /* GenPerCroIns: single-instruction crossover */
    int32_t gen_per_ins_seg;        /* GenPerInsSeg: segment insertion */
    int32_t gen_per_del_seg;        /* GenPerDelSeg: segment deletion */
    int32_t gen_per_cro_seg;        /* GenPerCroSeg: segment crossover */
    int32_t gen_per_cro_ins_sam_siz; /* GenPerCroInsSamSiz: same-size crossover */
    double  mut_bit_prop;           /* MutBitProp: fraction of mutations that are bit flips */

    /* disturbance */
    double  dist_freq;        /* DistFreq: disturbance frequency, x recovery time */
    double  dist_prop;        /* DistProp: fraction of population disturbed */

    /* memory protection modes (bitmask: 1=execute, 2=write, 4=read) */
    int32_t mem_mode_free;    /* MemModeFree: protection for free memory */
    int32_t mem_mode_mine;    /* MemModeMine: protection for memory a creature owns */
    int32_t mem_mode_prot;    /* MemModeProt: protection for another creature's memory */

    /* rng */
    uint64_t seed;            /* PRNG seed; 0 = derive from system time */
} TConfig;

/* Returns a TConfig populated with the canonical soup_in defaults. */
TConfig t_config_default(void);

/* ---- Live-tunable parameters -------------------------------------------- */

typedef enum TParam {
    T_PARAM_SLICE_SIZE,
    T_PARAM_GEN_PER_BKG_MUT,
    T_PARAM_GEN_PER_FLAW,
    T_PARAM_GEN_PER_MOV_MUT,
    T_PARAM_GEN_PER_DIV_MUT,
    T_PARAM_DIST_FREQ,
    T_PARAM_DIST_PROP,
    T_PARAM_REAP_RND_PROP,
    T_PARAM_MAL_MODE,
    T_PARAM_COUNT
} TParam;

/* ---- Telemetry ------------------------------------------------------------ */

typedef struct TStats {
    uint64_t inst_executed;
    double   generations;
    int32_t  num_cells;
    int32_t  num_genotypes;
    int32_t  num_sizes;
    double   speed;            /* instructions/sec */
    uint64_t mutations;        /* cumulative cosmic-ray + copy mutations */
    uint64_t flaws;            /* cumulative flaws */
    uint64_t births;
    uint64_t deaths;
    int32_t  max_pop_genotype; /* genotype id with the largest population */
    int32_t  max_pop;          /* that genotype's population */
    double   avg_size;
    double   avg_fecundity;
    double   mem_occupancy;    /* fraction of soup occupied, [0,1] */
} TStats;

/* ---- Read-only views for the visualizer ----------------------------------- */

typedef struct TCellView {
    int32_t  addr;
    int32_t  size;
    int32_t  ip;
    int32_t  genotype_id;
    uint64_t age;
    int      alive;
} TCellView;

typedef struct TGenoView {
    int32_t  id;
    char     name[8];   /* "NNNNlll" style, e.g. "0080aaa" */
    int32_t  size;
    int32_t  pop;
    uint32_t colour;    /* packed 0xRRGGBBAA, stable per genotype */
} TGenoView;

/* ---- Engine lifecycle ------------------------------------------------------ */

typedef struct TWorld TWorld;

TWorld  *t_create(const TConfig *cfg);
void     t_destroy(TWorld *w);

/* Inoculate the soup from a .tie ancestor file. Returns 0 on success. */
int      t_seed_file(TWorld *w, const char *tie_path);

/* Execute up to max_inst instructions total; returns instructions executed. */
uint64_t t_step(TWorld *w, uint64_t max_inst);

const TStats *t_stats(const TWorld *w);

int    t_set_param(TWorld *w, TParam p, double v);
double t_get_param(const TWorld *w, TParam p);

const uint8_t   *t_soup(const TWorld *w, int *size);
const TCellView *t_cells(const TWorld *w, int *n);
const TGenoView *t_genotypes(const TWorld *w, int *n);

#ifdef __cplusplus
}
#endif

#endif /* TIERRA_H */
