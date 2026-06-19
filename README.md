# tierra

A modern reimplementation of Tom Ray's [Tierra](https://tomray.me/tierra/)

Same core algorithm (ISA, template addressing, slicer/reaper, mutation),
rebuilt as a global-free `TWorld` + small C API. Added a live raylib soup map and dashboard with parameter sliders, replacing the old X11 frontend. Dropped networking/Beagle/XDR, diploidy, multithreaded cells, disk genebank/phylogeny tools, audio.

## Build

```
make        # libtierra.a, tierra-cli, tierra-viz
make debug  # -O0 -g build
```

## Run

```
./tierra-cli assets/0080aaa.tie                    # headless, default params
./tierra-cli --no-mutation -n 5000000 assets/0080aaa.tie
./tierra-cli -h                                    # all options

./tierra-viz assets/0080aaa.tie [seed]
```

`tierra-viz` controls: Pause/Step, speed multiplier, soup colouring
(genotype/size), IP markers, reseed, and live sliders for slice size,
mutation/flaw rates, reap fraction and allocator mode.
