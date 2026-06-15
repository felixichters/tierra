#include "assemble.h"
#include "isa.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int assemble_mnemonic_to_op(const char *token) {
    for (int op = 0; op < TIERRA_NUM_OPS; op++)
        if (strcmp(tierra_isa[op].mnemonic, token) == 0)
            return op;
    return -1;
}

int32_t assemble_load_tie(TSoup *soup, const char *path, int32_t addr) {
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char line[256];
    int in_code = 0;
    int32_t n = 0;

    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        if (!in_code) {
            if (strncmp(line, "CODE", 4) == 0)
                in_code = 1;
            continue;
        }

        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == ';' || strncmp(p, "track", 5) == 0)
            continue;

        char tok[16];
        size_t i = 0;
        while (p[i] && !isspace((unsigned char)p[i]) && i < sizeof(tok) - 1) {
            tok[i] = p[i];
            i++;
        }
        tok[i] = '\0';
        if (i == 0)
            continue;

        int op = assemble_mnemonic_to_op(tok);
        if (op < 0 || n >= soup->size) {
            fclose(f);
            return -1;
        }
        soup->mem[tierra_ad(soup->size, addr + n)] = (uint8_t)op;
        n++;
    }
    fclose(f);
    return n > 0 ? n : -1;
}
