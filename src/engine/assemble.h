#ifndef TIERRA_ASSEMBLE_H
#define TIERRA_ASSEMBLE_H

#include <stdint.h>
#include "soup.h"

/* Look up the opcode (0-31) for a .tie mnemonic token, e.g. "movDC",
 * "adrb", "divide". Returns -1 if the token is not a recognised mnemonic. */
int assemble_mnemonic_to_op(const char *token);

/* Parse a tierra/gb0 .tie-format genome file and write its instruction
 * bytes into soup starting at `addr` (addresses wrap via ad()).
 *
 * Format: everything before a line beginning with "CODE" is metadata and
 * ignored; after that, blank lines, lines starting with "track" and "; ..."
 * comment lines are skipped, and the first whitespace-delimited token of
 * every other line is an instruction mnemonic.
 *
 * Returns the genome length in instructions, or -1 on error (file not
 * found, no CODE section, unknown mnemonic, or genome longer than the
 * soup). */
int32_t assemble_load_tie(TSoup *soup, const char *path, int32_t addr);

#endif /* TIERRA_ASSEMBLE_H */
