#include "soupview.h"

#include <math.h>
#include <stdlib.h>

#define SOUP_GRID_WIDTH 300

static Color unpack_colour(uint32_t c) {
    return (Color){
        .r = (unsigned char)(c >> 24),
        .g = (unsigned char)(c >> 16),
        .b = (unsigned char)(c >> 8),
        .a = (unsigned char)(c & 0xFF),
    };
}

/* Stable per-size-class colour, independent of the genebank. */
static Color size_colour(int32_t size) {
    float hue = fmodf((float)size * 47.0f, 360.0f);
    return ColorFromHSV(hue, 0.55f, 0.85f);
}

void soupview_init(SoupView *sv, int32_t soup_size) {
    sv->width = SOUP_GRID_WIDTH;
    sv->height = (soup_size + sv->width - 1) / sv->width;

    sv->image = GenImageColor(sv->width, sv->height, (Color){25, 25, 30, 255});
    sv->texture = LoadTextureFromImage(sv->image);
    SetTextureFilter(sv->texture, TEXTURE_FILTER_POINT);
}

void soupview_destroy(SoupView *sv) {
    UnloadTexture(sv->texture);
    UnloadImage(sv->image);
}

void soupview_update(SoupView *sv, const TWorld *w, SoupColourMode mode, int show_ip) {
    Color *px = (Color *)sv->image.data;
    int total = sv->width * sv->height;
    Color free_colour = (Color){25, 25, 30, 255};
    for (int i = 0; i < total; i++)
        px[i] = free_colour;

    int soup_size = 0;
    t_soup(w, &soup_size);

    int ngeno = 0;
    const TGenoView *gv = t_genotypes(w, &ngeno);

    /* Genotype ids are small dense indices into the genebank; build a
     * direct lookup table sized to the largest id seen this frame. */
    int32_t max_id = -1;
    for (int i = 0; i < ngeno; i++)
        if (gv[i].id > max_id)
            max_id = gv[i].id;

    Color unknown_colour = (Color){120, 120, 120, 255};
    Color *geno_colour = NULL;
    if (max_id >= 0) {
        geno_colour = malloc((size_t)(max_id + 1) * sizeof(Color));
        for (int32_t i = 0; i <= max_id; i++)
            geno_colour[i] = unknown_colour;
        for (int i = 0; i < ngeno; i++)
            geno_colour[gv[i].id] = unpack_colour(gv[i].colour);
    }

    int ncells = 0;
    const TCellView *cv = t_cells(w, &ncells);

    for (int i = 0; i < ncells; i++) {
        Color col;
        if (mode == SOUP_COLOUR_SIZE) {
            col = size_colour(cv[i].size);
        } else if (cv[i].genotype_id >= 0 && cv[i].genotype_id <= max_id) {
            col = geno_colour[cv[i].genotype_id];
        } else {
            col = unknown_colour;
        }

        for (int32_t j = 0; j < cv[i].size; j++) {
            int32_t addr = cv[i].addr + j;
            if (addr >= soup_size)
                addr -= soup_size;
            px[addr] = col;
        }

        if (show_ip && cv[i].ip >= 0 && cv[i].ip < soup_size)
            px[cv[i].ip] = WHITE;
    }

    free(geno_colour);

    UpdateTexture(sv->texture, sv->image.data);
}

void soupview_draw(const SoupView *sv, Rectangle dest) {
    Rectangle src = {0, 0, (float)sv->width, (float)sv->height};
    DrawTexturePro(sv->texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
    DrawRectangleLinesEx(dest, 1.0f, GRAY);
}
