/* The 32-instruction Tierra ISA.
 *
 * Grounded in tierra/gb0/opcode.map (opcode -> mnemonic -> decode/exec) and
 * cross-checked against the hex annotations in gb0/0080aaa.tie and
 * gb0/0021aaa.tie. Register letters a/b/c/d map to AX/BX/CX/DX (confirmed
 * via decode.c's movii/movdd argument strings).
 *
 * Each exec_* function fuses legacy decode+exec into one step and returns
 * the amount to advance cpu->ip by (0 if it set cpu->ip itself).
 *
 * Template addressing (adr/adrb/adrf/jmp/jmpb/call) follows ctemplate()'s
 * algorithm: a "template" is the run of nop0/nop1 immediately after the
 * instruction; a match is a same-length run whose opcodes are the bitwise
 * complement (nop0<->nop1) of the template, found by scanning outward,
 * forward-only, or backward-only from just past/before the template. The
 * resulting address is one past the end of the matched run
 * (ad(match_start + template_size), per ctemplate()'s `adrt = ad(*f + tz)`).
 * The legacy two-tier skip/match loop is collapsed into a single
 * position-indexed scan bounded by ctx->search_limit candidate positions
 * per direction -- equivalent in spirit and exact for the densely-templated
 * ancestor genomes.
 */

#include "isa.h"
#include "util.h"

static int32_t flaw(TExecCtx *ctx) {
    return ctx->flaw ? ctx->flaw(ctx->flaw_user) : 0;
}

static void do_flags(TCpu *cpu, int32_t val) {
    cpu->fl.e = 0;
    cpu->fl.s = (val < 0);
    cpu->fl.z = (val == 0);
}

static void clear_flags(TCpu *cpu) {
    cpu->fl.e = cpu->fl.s = cpu->fl.z = 0;
}

static void fail_flags(TCpu *cpu) {
    cpu->fl.e = 1;
    cpu->fl.s = cpu->fl.z = 0;
}

static void cpu_push(TCpu *cpu, int32_t val) {
    cpu->sp = (cpu->sp + 1) % TCPU_STACK_SIZE;
    cpu->stack[cpu->sp] = val;
}

static int32_t cpu_pop(TCpu *cpu) {
    int32_t val = cpu->stack[cpu->sp];
    cpu->sp = (cpu->sp == 0) ? TCPU_STACK_SIZE - 1 : cpu->sp - 1;
    return val;
}

static int is_nop(uint8_t b) {
    return b == OP_NOP0 || b == OP_NOP1;
}

/* Size of the nop0/nop1 run starting at soup[start], capped at soup->size
 * (a creature cannot be made entirely of one unbroken template). */
static int32_t template_size(const TSoup *soup, int32_t start) {
    int32_t n = 0;
    while (n < soup->size && is_nop(soup->mem[tierra_ad(soup->size, start + n)]))
        n++;
    return n;
}

/* Does the tz-length run starting at `pos` complement the tz-length run
 * starting at `origin`, byte-for-byte (nop0+nop1 == 1)? */
static int complement_at(const TSoup *soup, int32_t origin, int32_t pos, int32_t tz) {
    if (!is_nop(soup->mem[pos]))
        return 0;
    for (int32_t i = 0; i < tz; i++) {
        uint8_t o = soup->mem[tierra_ad(soup->size, origin + i)];
        uint8_t p = soup->mem[tierra_ad(soup->size, pos + i)];
        if (!is_nop(p) || (int)o + (int)p != 1)
            return 0;
    }
    return 1;
}

/* Scan outward/forward/backward for a complement template of size tz.
 * On success returns ad(match_start + tz); on failure returns -1.
 * Forward is checked before backward each step, matching the legacy
 * preference for forward matches on simultaneous hits. */
static int32_t find_complement(const TSoup *soup, int32_t origin, int32_t tz,
                                int32_t fwd_start, int32_t bwd_start,
                                int want_fwd, int want_bwd, int32_t limit) {
    int32_t f = fwd_start, b = bwd_start;
    for (int32_t step = 0; step <= limit; step++) {
        if (want_fwd && complement_at(soup, origin, f, tz))
            return tierra_ad(soup->size, f + tz);
        if (want_bwd && complement_at(soup, origin, b, tz))
            return tierra_ad(soup->size, b + tz);
        f = tierra_ad(soup->size, f + 1);
        b = tierra_ad(soup->size, b - 1);
    }
    return -1;
}

/* ---- nop0 / nop1 -------------------------------------------------------- */

static int32_t exec_nop(TCpu *cpu, TExecCtx *ctx) {
    (void)ctx;
    clear_flags(cpu);
    return 1;
}

/* ---- not0 / shl / zero ---------------------------------------------------
 * All operate on CX (opcode.map decode arg "c").
 */

static int32_t exec_not0(TCpu *cpu, TExecCtx *ctx) {
    cpu->cx ^= (1 + flaw(ctx));
    do_flags(cpu, cpu->cx);
    return 1;
}

static int32_t exec_shl(TCpu *cpu, TExecCtx *ctx) {
    cpu->cx <<= (1 + flaw(ctx));
    do_flags(cpu, cpu->cx);
    return 1;
}

static int32_t exec_zero(TCpu *cpu, TExecCtx *ctx) {
    cpu->cx = 0 + flaw(ctx);
    do_flags(cpu, cpu->cx);
    return 1;
}

/* ---- ifz ------------------------------------------------------------------
 * decode arg "cc", mn[2]='z': is.sval = (CX==0); skip() advances IP by 1 if
 * true (execute next instruction), by 2 if false (skip it).
 */

static int32_t exec_ifz(TCpu *cpu, TExecCtx *ctx) {
    (void)ctx;
    clear_flags(cpu);
    return (cpu->cx == 0) ? 1 : 2;
}

/* ---- subCAB / subAAC / incA / incB / decC / incC -------------------------
 * dec1d1s/dec1d2s + the generic add()-style executor: *(dreg) = sval + sval2.
 */

static int32_t exec_subcab(TCpu *cpu, TExecCtx *ctx) { /* CX = AX - BX */
    cpu->cx = cpu->ax + (-cpu->bx) + flaw(ctx);
    do_flags(cpu, cpu->cx);
    return 1;
}

static int32_t exec_subaac(TCpu *cpu, TExecCtx *ctx) { /* AX = AX - CX */
    cpu->ax = cpu->ax + (-cpu->cx) + flaw(ctx);
    do_flags(cpu, cpu->ax);
    return 1;
}

static int32_t exec_inca(TCpu *cpu, TExecCtx *ctx) { /* AX++ */
    cpu->ax = cpu->ax + 1 + flaw(ctx);
    do_flags(cpu, cpu->ax);
    return 1;
}

static int32_t exec_incb(TCpu *cpu, TExecCtx *ctx) { /* BX++ */
    cpu->bx = cpu->bx + 1 + flaw(ctx);
    do_flags(cpu, cpu->bx);
    return 1;
}

static int32_t exec_decc(TCpu *cpu, TExecCtx *ctx) { /* CX-- */
    cpu->cx = cpu->cx - 1 + flaw(ctx);
    do_flags(cpu, cpu->cx);
    return 1;
}

static int32_t exec_incc(TCpu *cpu, TExecCtx *ctx) { /* CX++ */
    cpu->cx = cpu->cx + 1 + flaw(ctx);
    do_flags(cpu, cpu->cx);
    return 1;
}

/* ---- push / pop ------------------------------------------------------------
 * push(): sp = (sp+1) % STACK_SIZE; st[sp] = val. pop(): val = st[sp];
 * sp = (sp-1), wrapping to STACK_SIZE-1.
 */

static int32_t exec_pusha(TCpu *cpu, TExecCtx *ctx) { cpu_push(cpu, cpu->ax + flaw(ctx)); clear_flags(cpu); return 1; }
static int32_t exec_pushb(TCpu *cpu, TExecCtx *ctx) { cpu_push(cpu, cpu->bx + flaw(ctx)); clear_flags(cpu); return 1; }
static int32_t exec_pushc(TCpu *cpu, TExecCtx *ctx) { cpu_push(cpu, cpu->cx + flaw(ctx)); clear_flags(cpu); return 1; }
static int32_t exec_pushd(TCpu *cpu, TExecCtx *ctx) { cpu_push(cpu, cpu->dx + flaw(ctx)); clear_flags(cpu); return 1; }

static int32_t exec_popa(TCpu *cpu, TExecCtx *ctx) { cpu->ax = cpu_pop(cpu) + flaw(ctx); do_flags(cpu, cpu->ax); return 1; }
static int32_t exec_popb(TCpu *cpu, TExecCtx *ctx) { cpu->bx = cpu_pop(cpu) + flaw(ctx); do_flags(cpu, cpu->bx); return 1; }
static int32_t exec_popc(TCpu *cpu, TExecCtx *ctx) { cpu->cx = cpu_pop(cpu) + flaw(ctx); do_flags(cpu, cpu->cx); return 1; }
static int32_t exec_popd(TCpu *cpu, TExecCtx *ctx) { cpu->dx = cpu_pop(cpu) + flaw(ctx); do_flags(cpu, cpu->dx); return 1; }

/* ---- jmpo / jmpb -----------------------------------------------------------
 * decjmp: template = nop0/nop1 run at ip+1, size s.
 *   s == 0: IP = ad(BX)  (indirect jump, no error -- decjmp sets is.sval
 *           from BX precisely for this case)
 *   s != 0: search for complement template (jmpo: outward, jmpb: backward
 *           only); on match IP = ad(match_start + s); on failure E=1 and
 *           IP advances past the template.
 */

static int32_t exec_jmp(TCpu *cpu, TExecCtx *ctx, int want_fwd, int want_bwd) {
    TSoup *soup = ctx->soup;
    int32_t origin = tierra_ad(soup->size, cpu->ip + 1);
    int32_t tz = template_size(soup, origin);

    if (tz == 0) {
        cpu->ip = tierra_ad(soup->size, cpu->bx);
        return 0;
    }
    if (tz < ctx->min_templ_size) {
        fail_flags(cpu);
        return tz + 1;
    }

    int32_t fwd = tierra_ad(soup->size, origin + tz + 1);
    int32_t bwd = tierra_ad(soup->size, origin - tz - 1);
    int32_t found = find_complement(soup, origin, tz, fwd, bwd, want_fwd, want_bwd, ctx->search_limit);
    if (found < 0) {
        fail_flags(cpu);
        return tz + 1;
    }
    cpu->ip = found;
    clear_flags(cpu);
    return 0;
}

static int32_t exec_jmpo(TCpu *cpu, TExecCtx *ctx) { return exec_jmp(cpu, ctx, 1, 1); }
static int32_t exec_jmpb(TCpu *cpu, TExecCtx *ctx) { return exec_jmp(cpu, ctx, 0, 1); }

/* ---- call ------------------------------------------------------------------
 * ptcall + tcall/adrfindtmp: like jmpo, but additionally pushes the address
 * just past the call's own template (the return address). s == 0 jumps to
 * (and pushes) that same address -- effectively a no-op call.
 */

static int32_t exec_call(TCpu *cpu, TExecCtx *ctx) {
    TSoup *soup = ctx->soup;
    int32_t origin = tierra_ad(soup->size, cpu->ip + 1);
    int32_t tz = template_size(soup, origin);
    int32_t ret_addr = tierra_ad(soup->size, cpu->ip + tz + 1);

    if (tz == 0) {
        cpu->ip = ret_addr;
        cpu_push(cpu, ret_addr);
        return 0;
    }
    if (tz < ctx->min_templ_size) {
        fail_flags(cpu);
        return tz + 1;
    }

    int32_t fwd = tierra_ad(soup->size, origin + tz + 1);
    int32_t bwd = tierra_ad(soup->size, origin - tz - 1);
    int32_t found = find_complement(soup, origin, tz, fwd, bwd, 1, 1, ctx->search_limit);
    if (found < 0) {
        fail_flags(cpu);
        return tz + 1;
    }
    cpu->ip = found;
    cpu_push(cpu, ret_addr);
    clear_flags(cpu);
    return 0;
}

/* ---- ret -------------------------------------------------------------------
 * pop() into IP, wrapped via ad(); DoFlags() is then evaluated on IP itself.
 */

static int32_t exec_ret(TCpu *cpu, TExecCtx *ctx) {
    int32_t val = cpu_pop(cpu) + flaw(ctx);
    cpu->ip = tierra_ad(ctx->soup->size, val);
    do_flags(cpu, cpu->ip);
    return 0;
}

/* ---- movDC / movBA --------------------------------------------------------- */

static int32_t exec_movdc(TCpu *cpu, TExecCtx *ctx) { /* DX = CX */
    cpu->dx = cpu->cx + flaw(ctx);
    do_flags(cpu, cpu->dx);
    return 1;
}

static int32_t exec_movba(TCpu *cpu, TExecCtx *ctx) { /* BX = AX */
    cpu->bx = cpu->ax + flaw(ctx);
    do_flags(cpu, cpu->bx);
    return 1;
}

/* ---- movii -----------------------------------------------------------------
 * pmovii + movii: soup[ad(AX)] = soup[ad(BX)], one byte, unless the two
 * addresses coincide (E=1 in that case). On a successful write, ctx->on_mov
 * lets world.c track the daughter-memory copy bookkeeping (ce->d.MovOffMin/
 * Max/mov_daught) that divide() consumes.
 */

static int32_t exec_movii(TCpu *cpu, TExecCtx *ctx) {
    TSoup *soup = ctx->soup;
    int32_t dval = tierra_ad(soup->size, cpu->ax);
    int32_t sval = tierra_ad(soup->size, cpu->bx);

    clear_flags(cpu);
    if (dval != sval) {
        soup->mem[dval] = soup->mem[sval];
        if (ctx->on_mov)
            ctx->on_mov(ctx->user, dval);
    } else {
        cpu->fl.e = 1;
    }
    return 1;
}

/* ---- adro / adrb / adrf ------------------------------------------------------
 * decadr + adrfindtmp: template = nop0/nop1 run at ip+1, size s. s == 0 is
 * treated as a malformed instruction (E=1, advance past it -- legacy leaves
 * the destination register at a stale value in this case, which a
 * from-scratch implementation has no equivalent of). Otherwise search for a
 * complement template (adro: outward, adrb: backward, adrf: forward);
 * on success AX = match address, CX = template size s.
 */

static int32_t exec_adr(TCpu *cpu, TExecCtx *ctx, int want_fwd, int want_bwd) {
    TSoup *soup = ctx->soup;
    int32_t origin = tierra_ad(soup->size, cpu->ip + 1);
    int32_t tz = template_size(soup, origin);

    if (tz == 0 || tz < ctx->min_templ_size) {
        fail_flags(cpu);
        return (tz == 0) ? 1 : tz + 1;
    }

    int32_t fwd = tierra_ad(soup->size, origin + tz + 1);
    int32_t bwd = tierra_ad(soup->size, origin - tz - 1);
    int32_t found = find_complement(soup, origin, tz, fwd, bwd, want_fwd, want_bwd, ctx->search_limit);
    if (found < 0) {
        fail_flags(cpu);
        return tz + 1;
    }
    cpu->ax = found;
    cpu->cx = tz;
    clear_flags(cpu);
    return tz + 1;
}

static int32_t exec_adro(TCpu *cpu, TExecCtx *ctx) { return exec_adr(cpu, ctx, 1, 1); }
static int32_t exec_adrb(TCpu *cpu, TExecCtx *ctx) { return exec_adr(cpu, ctx, 0, 1); }
static int32_t exec_adrf(TCpu *cpu, TExecCtx *ctx) { return exec_adr(cpu, ctx, 1, 0); }

/* ---- mal -------------------------------------------------------------------
 * malchm: CX = requested size. Reject if outside [min_cell_size, soup_size).
 * Otherwise ask the allocator (AX as an address hint, unused by first/better
 * fit) and store the resulting address in AX. If the soup is full,
 * ctx->reap_for_space frees a cell's memory and the allocation is retried
 * (mirroring mal()'s "while (MemAlloc < 0) reaper(...)" loop) until it
 * succeeds or reap_for_space reports nothing left to reap.
 */

static int32_t exec_mal(TCpu *cpu, TExecCtx *ctx) {
    TSoup *soup = ctx->soup;

    /* Legacy mal() rejects outright (before any reaping) a request larger
     * than MaxMalMult * the requesting cell's own mm_size -- otherwise a
     * single creature with a stray large cx could force-reap most of the
     * population to satisfy one oversized mal(). */
    if (cpu->cx <= 0 ||
        (double)cpu->cx > MAX_MAL_MULT * (double)ctx->own_mm_size) {
        fail_flags(cpu);
        return 1;
    }

    int32_t req = cpu->cx + flaw(ctx);

    if (req < soup->min_cell_size || req >= soup->size) {
        fail_flags(cpu);
        return 1;
    }

    int32_t addr;
    for (;;) {
        addr = soup_alloc(soup, req, cpu->ax);
        if (addr >= 0)
            break;
        if (!ctx->reap_for_space || ctx->reap_for_space(ctx->user) != 0) {
            fail_flags(cpu);
            return 1;
        }
    }

    cpu->ax = addr;
    if (ctx->on_mal)
        ctx->on_mal(ctx->user, addr, req);
    clear_flags(cpu);
    return 1;
}

/* ---- divide -----------------------------------------------------------------
 * The canonical decode (idf.C) fixes mode=2, eject=0. The actual creation of
 * a daughter cell requires the slicer/reaper/genebank (step 3), so it is
 * delegated to ctx->divide; with no world attached (ctx->divide == NULL) it
 * always fails, which is the correct behaviour for a standalone CPU.
 */

static int32_t exec_divide(TCpu *cpu, TExecCtx *ctx) {
    int ok = ctx->divide ? ctx->divide(ctx->user, cpu, 2, 0) : -1;
    if (ok == 0)
        clear_flags(cpu);
    else
        fail_flags(cpu);
    return 1;
}

/* ---- dispatch table ---------------------------------------------------- */

const TInst tierra_isa[TIERRA_NUM_OPS] = {
    [OP_NOP0]   = {"nop0",   exec_nop},
    [OP_NOP1]   = {"nop1",   exec_nop},
    [OP_NOT0]   = {"not0",   exec_not0},
    [OP_SHL]    = {"shl",    exec_shl},
    [OP_ZERO]   = {"zero",   exec_zero},
    [OP_IFZ]    = {"ifz",    exec_ifz},
    [OP_SUBCAB] = {"subCAB", exec_subcab},
    [OP_SUBAAC] = {"subAAC", exec_subaac},
    [OP_INCA]   = {"incA",   exec_inca},
    [OP_INCB]   = {"incB",   exec_incb},
    [OP_DECC]   = {"decC",   exec_decc},
    [OP_INCC]   = {"incC",   exec_incc},
    [OP_PUSHA]  = {"pushA",  exec_pusha},
    [OP_PUSHB]  = {"pushB",  exec_pushb},
    [OP_PUSHC]  = {"pushC",  exec_pushc},
    [OP_PUSHD]  = {"pushD",  exec_pushd},
    [OP_POPA]   = {"popA",   exec_popa},
    [OP_POPB]   = {"popB",   exec_popb},
    [OP_POPC]   = {"popC",   exec_popc},
    [OP_POPD]   = {"popD",   exec_popd},
    [OP_JMPO]   = {"jmpo",   exec_jmpo},
    [OP_JMPB]   = {"jmpb",   exec_jmpb},
    [OP_CALL]   = {"call",   exec_call},
    [OP_RET]    = {"ret",    exec_ret},
    [OP_MOVDC]  = {"movDC",  exec_movdc},
    [OP_MOVBA]  = {"movBA",  exec_movba},
    [OP_MOVII]  = {"movii",  exec_movii},
    [OP_ADRO]   = {"adro",   exec_adro},
    [OP_ADRB]   = {"adrb",   exec_adrb},
    [OP_ADRF]   = {"adrf",   exec_adrf},
    [OP_MAL]    = {"mal",    exec_mal},
    [OP_DIVIDE] = {"divide", exec_divide},
};
