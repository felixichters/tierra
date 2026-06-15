#include "cpu.h"
#include "isa.h"
#include "util.h"

#include <string.h>

void cpu_reset(TCpu *cpu, int32_t ip) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->ip = ip;
}

int cpu_step(TCpu *cpu, TExecCtx *ctx) {
    TSoup *soup = ctx->soup;
    cpu->ip = tierra_ad(soup->size, cpu->ip);
    int op = soup->mem[cpu->ip] & 0x1F;
    int32_t delta = tierra_isa[op].exec(cpu, ctx);
    if (delta)
        cpu->ip = tierra_ad(soup->size, cpu->ip + delta);
    return op;
}
