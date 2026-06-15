#ifndef TIERRA_VIZ_SOUPVIEW_H
#define TIERRA_VIZ_SOUPVIEW_H

#include "raylib.h"
#include "tierra/tierra.h"

/* The soup drawn as a wrapped linear grid: address 0 is the top-left
 * pixel, addresses increase left-to-right then top-to-bottom. */
typedef struct SoupView {
    int32_t width, height;
    Image     image;
    Texture2D texture;
} SoupView;

typedef enum SoupColourMode {
    SOUP_COLOUR_GENOTYPE,
    SOUP_COLOUR_SIZE,
} SoupColourMode;

void soupview_init(SoupView *sv, int32_t soup_size);
void soupview_destroy(SoupView *sv);

/* Recolour the backing image from the current world state: free soup is
 * dark grey, each live cell's memory span is painted with its genotype's
 * (or size class's) stable colour, and optionally its IP is marked white. */
void soupview_update(SoupView *sv, const TWorld *w, SoupColourMode mode, int show_ip);

/* Draw the soup map scaled to fit `dest`. */
void soupview_draw(const SoupView *sv, Rectangle dest);

#endif /* TIERRA_VIZ_SOUPVIEW_H */
