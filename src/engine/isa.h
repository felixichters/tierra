#ifndef TIERRA_ISA_H
#define TIERRA_ISA_H

#include <stdint.h>
#include "cpu.h"

#define TIERRA_NUM_OPS 32

/* Canonical opcode values, matching tierra/gb0/opcode.map (index = opcode)
 * and the hex annotations in gb0/0080aaa.tie / gb0/0021aaa.tie. */
enum {
    OP_NOP0   = 0x00,
    OP_NOP1   = 0x01,
    OP_NOT0   = 0x02,
    OP_SHL    = 0x03,
    OP_ZERO   = 0x04,
    OP_IFZ    = 0x05,
    OP_SUBCAB = 0x06,
    OP_SUBAAC = 0x07,
    OP_INCA   = 0x08,
    OP_INCB   = 0x09,
    OP_DECC   = 0x0A,
    OP_INCC   = 0x0B,
    OP_PUSHA  = 0x0C,
    OP_PUSHB  = 0x0D,
    OP_PUSHC  = 0x0E,
    OP_PUSHD  = 0x0F,
    OP_POPA   = 0x10,
    OP_POPB   = 0x11,
    OP_POPC   = 0x12,
    OP_POPD   = 0x13,
    OP_JMPO   = 0x14,
    OP_JMPB   = 0x15,
    OP_CALL   = 0x16,
    OP_RET    = 0x17,
    OP_MOVDC  = 0x18,
    OP_MOVBA  = 0x19,
    OP_MOVII  = 0x1A,
    OP_ADRO   = 0x1B,
    OP_ADRB   = 0x1C,
    OP_ADRF   = 0x1D,
    OP_MAL    = 0x1E,
    OP_DIVIDE = 0x1F,
};

typedef struct TInst {
    const char *mnemonic;
    /* Execute the instruction. Returns the amount to advance cpu->ip by
     * (wrapped via ad()), or 0 if the instruction already set cpu->ip
     * itself (jumps, calls, ret). */
    int32_t (*exec)(TCpu *cpu, TExecCtx *ctx);
} TInst;

extern const TInst tierra_isa[TIERRA_NUM_OPS];

#endif /* TIERRA_ISA_H */
