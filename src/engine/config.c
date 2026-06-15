#include "config.h"

/* Defaults taken from tierra/soup_in (Tierra 6.02 reference run). */
TConfig t_config_default(void) {
    TConfig c = {
        .soup_size       = 60000,
        .min_cell_size   = 12,
        .min_genome_size = 12,
        .mal_mode        = 1,    /* better-fit */
        .mal_tol         = 20.0,

        .slice_size      = 25,
        .size_dep_slice  = 0,
        .slice_pow       = 1.0,
        .slic_fix_frac   = 0.0,
        .slic_ran_frac   = 2.0,

        .reap_rnd_prop   = 0.3,
        .lazy_tol        = 10,

        .search_limit    = 5.0,
        .min_templ_size  = 1,

        .gen_per_bkg_mut        = 32,
        .gen_per_flaw           = 32,
        .gen_per_mov_mut        = 0,
        .gen_per_div_mut        = 32,
        .gen_per_ins_ins        = 32,
        .gen_per_del_ins        = 32,
        .gen_per_cro_ins        = 32,
        .gen_per_ins_seg        = 32,
        .gen_per_del_seg        = 32,
        .gen_per_cro_seg        = 32,
        .gen_per_cro_ins_sam_siz = 32,
        .mut_bit_prop           = 0.2,

        .dist_freq = -0.3,
        .dist_prop = 0.2,

        .mem_mode_free = 0,
        .mem_mode_mine = 0,
        .mem_mode_prot = 2,

        .seed = 0,
    };
    return c;
}
