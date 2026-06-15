/* tierra-viz: raylib window showing the soup map and a live telemetry /
 * parameter dashboard. Each frame advances the engine by a speed-scaled
 * instruction budget and renders from the read-only TWorld views. */
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tierra/tierra.h"
#include "soupview.h"
#include "dashboard.h"

#define WIN_W 1400
#define WIN_H 900

/* Instructions executed per frame at each speed setting. */
static const uint64_t SPEED_BUDGET[] = {0, 1000, 5000, 25000, 100000, 400000};
#define NUM_SPEEDS (int)(sizeof(SPEED_BUDGET) / sizeof(SPEED_BUDGET[0]))
#define SPEED_LABELS "0x;1x;5x;25x;100x;400x"

int main(int argc, char **argv) {
    const char *tie_path = argc > 1 ? argv[1] : "assets/0080aaa.tie";
    uint64_t base_seed = argc > 2 ? strtoull(argv[2], NULL, 10) : 0;

    TConfig cfg = t_config_default();
    cfg.seed = base_seed;

    TWorld *w = t_create(&cfg);
    if (t_seed_file(w, tie_path) != 0) {
        fprintf(stderr, "tierra-viz: failed to load '%s'\n", tie_path);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "tierra-viz");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    SoupView sv;
    soupview_init(&sv, cfg.soup_size);

    Dashboard dash;
    dashboard_init(&dash, w);

    int paused = 0;
    int speed_idx = 2; /* 5x */
    int colour_mode = SOUP_COLOUR_GENOTYPE;
    bool show_ip = false;
    int reseed_count = 0;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE))
            paused = !paused;

        /* Dashboard keeps a fixed width (clamped on tiny windows); the soup
         * map fills whatever space is left, growing/shrinking with the
         * window. */
        int win_w = GetScreenWidth();
        int win_h = GetScreenHeight();
        int dash_w = 470;
        if (dash_w > win_w - 300) dash_w = win_w - 300;
        Rectangle soup_rect = {10, 46, (float)(win_w - dash_w - 20), (float)(win_h - 56)};
        Rectangle dash_rect = {(float)(win_w - dash_w - 10), 46, (float)dash_w, (float)(win_h - 56)};

        /* ---- step the engine ----------------------------------------- */
        int step_once = 0;
        uint64_t budget = paused ? 0 : SPEED_BUDGET[speed_idx];

        /* ---- top control bar (drawn after stepping, but state read now) */
        BeginDrawing();
        ClearBackground((Color){18, 18, 22, 255});

        DrawRectangle(0, 0, win_w, 40, (Color){32, 32, 38, 255});

        if (GuiButton((Rectangle){10, 6, 80, 28}, paused ? "Resume" : "Pause"))
            paused = !paused;

        if (GuiButton((Rectangle){96, 6, 60, 28}, "Step"))
            step_once = 1;

        DrawText("speed", 168, 14, 14, LIGHTGRAY);
        GuiToggleGroup((Rectangle){216, 6, 50, 28}, SPEED_LABELS, &speed_idx);

        DrawText("colour", 540, 14, 14, LIGHTGRAY);
        GuiToggleGroup((Rectangle){590, 6, 80, 28}, "Genotype;Size", &colour_mode);

        GuiCheckBox((Rectangle){770, 10, 20, 20}, "IP markers", &show_ip);

        if (GuiButton((Rectangle){920, 6, 90, 28}, "Reseed")) {
            t_destroy(w);
            cfg = t_config_default();
            cfg.seed = base_seed != 0
                ? base_seed + (uint64_t)(++reseed_count)
                : (uint64_t)time(NULL) * 2654435761u + (uint64_t)(++reseed_count);
            w = t_create(&cfg);
            t_seed_file(w, tie_path);
            dashboard_init(&dash, w);
        }

        char title[128];
        snprintf(title, sizeof(title), "tierra-viz -- %s%s", tie_path,
                 paused ? "  [paused]" : "");
        DrawText(title, win_w - 380, 14, 14, LIGHTGRAY);

        /* ---- advance simulation ---------------------------------------- */
        if (step_once)
            t_step(w, SPEED_BUDGET[speed_idx] ? SPEED_BUDGET[speed_idx] : 1000);
        else if (budget > 0)
            t_step(w, budget);

        const TStats *st = t_stats(w);
        dashboard_push(&dash, st);
        soupview_update(&sv, w, (SoupColourMode)colour_mode, show_ip);

        /* ---- render ------------------------------------------------------ */
        soupview_draw(&sv, soup_rect);
        dashboard_draw(&dash, w, st, dash_rect);

        EndDrawing();
    }

    soupview_destroy(&sv);
    CloseWindow();
    t_destroy(w);
    return 0;
}
