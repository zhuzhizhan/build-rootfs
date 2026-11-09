# LVGL clock for linux-app

Date: 2026-11-09
Status: Approved design

## Goal

Add an LVGL demo under `linux-app/lvgl-clock` that draws an analog clock with
hour, minute, and second hands plus a digital `HH:MM:SS` readout and date, using
the system clock. It renders straight to the Linux framebuffer (`/dev/fb0`), so
it runs in the QEMU guest without Xorg.

## Non-goals

- No input devices (mouse, keyboard, touch); the demo is display-only.
- No X11, Wayland, SDL, or DRM backends.
- No Xorg/openbox integration or window management.
- No CJK fonts or localized weekday names in this iteration.
- No timezone management; `localtime_r()` and the `TZ` environment variable
  determine the displayed time.

## Decisions

- Use LVGL v9.3.0, fetched at build time with `fetch_lvgl.sh` into
  `third_party/lvgl` (gitignored). Only `src/`, `lvgl.h`, `lv_version.h`,
  `lvgl_private.h`, and license/readme files are extracted.
- Use LVGL's `lv_linux_fbdev` display driver with `LV_LINUX_FBDEV_RENDER_MODE`
  partial and unspecified buffer count (driver default).
- Cross-compile on the host with `aarch64-linux-gnu-gcc`; default build is the
  native host compiler. Guest-side native builds are not required.
- Split pointer-angle math into a platform-independent module with a host unit
  test, matching the repository's self-checking convention.
- Build integration: `linux-app/Makefile` gains an `lvgl` target (not part of
  `gui`, to avoid an implicit 84 MB download); `.gitignore` and both READMEs are
  updated.

## Layout

```text
linux-app/lvgl-clock/
├── main.c              # LVGL init, fbdev setup, VT control, main loop, signals
├── clock_ui.c          # clock face widgets and refresh timer
├── clock_ui.h
├── clock_math.c        # time -> hand angles, angle -> endpoint
├── clock_math.h
├── test_clock_math.c   # host self-check for clock_math
├── lv_conf.h           # minimal LVGL configuration
├── fetch_lvgl.sh       # download and trim LVGL v9.3.0
├── build.sh            # thin wrapper around make
├── start-clock.sh      # guest entry point, checks /dev/fb0 and binary arch
├── Makefile
└── README.md
```

`third_party/lvgl/` and `build/` are gitignored.

## Platform layer (`main.c`)

- Install `SIGINT`/`SIGTERM` handlers that clear a `volatile sig_atomic_t`
  running flag.
- `lv_init()`, then `lv_tick_set_cb()` returning monotonic milliseconds from
  `clock_gettime(CLOCK_MONOTONIC)`.
- Query the framebuffer resolution through `lv_display_get_horizontal/vertical_resolution`
  after `lv_linux_fbdev_create()` + `lv_linux_fbdev_set_file(disp, "/dev/fb0")`.
- Console isolation: open `/dev/tty1`, best-effort `VT_ACTIVATE`/
  `VT_WAITACTIVE` through `/dev/tty0`, then `KDSETMODE` `KD_GRAPHICS` on
  `/dev/tty1`. On exit, restore `KD_TEXT`. Failures are logged but not fatal
  (for example when run from a plain console without permissions).
- Main loop: `lv_timer_handler()` then `usleep(5 * 1000)` while running. On exit,
  restore console mode and `lv_deinit()`.
- Optional `argv` is not used; no CLI flags.

## UI layer (`clock_ui.c`)

`clock_ui_create(int width, int height)` builds the screen and returns nothing;
it registers a 50 ms `lv_timer` internally.

Layout adapts to the framebuffer size:

- `r = min(width, height) * 0.36`, dial center `cx = width / 2`,
  `cy = min(height * 0.40, height - (r + 120))`.
- Screen background: dark vertical gradient.
- Dial: circle object (`LV_RADIUS_CIRCLE`) with dark fill, subtle border, and
  centered at `(cx, cy)`.
- Ticks: 60 `lv_line` objects; every fifth tick is longer and thicker.
- Hour numerals `1..12` as `lv_label` objects (Montserrat 28) at radius
  `0.78 * r`, positioned with `lv_obj_align(LV_ALIGN_CENTER, dx, dy)`.
- Hands: three `lv_line` objects with rounded line ends, hour `0.52 * r` wide
  style, minute `0.75 * r`, second `0.85 * r` plus a `0.18 * r` tail. Colors:
  white hour/minute, red second. Draw order ends with a small center-cap circle.
- Digital block below the dial: `HH:MM:SS` label (Montserrat 48) centered at
  `cy + r + 34`; date `YYYY-MM-DD Www` (Montserrat 20) at `cy + r + 82`.

Time handling:

- Read `clock_gettime(CLOCK_REALTIME)` and convert with `localtime_r`.
- `clock_math_angles(hour, minute, second, nanosecond)` returns hour, minute,
  and second angles in degrees measured clockwise from 12 o'clock. Hour and
  minute include fractional contributions; the second hand uses the nanoseconds
  for a smooth sweep.
- Every timer tick: recompute hand endpoints and call `lv_line_set_points()`.
  Update the digital labels only when the formatted second changes.
- Date uses `%04d-%02d-%02d %s` with a fixed English weekday table; the
  built-in Montserrat fonts have no CJK glyphs and `%a` would follow the guest
  locale.

## LVGL configuration (`lv_conf.h`)

Minimal override file relying on `lv_conf_internal.h` defaults:

- `LV_COLOR_DEPTH 32`, `LV_MEM_SIZE` raised to 128 KB.
- `LV_USE_LINUX_FBDEV 1`, `LV_LINUX_FBDEV_RENDER_MODE` partial,
  `LV_LINUX_FBDEV_MMAP 1`.
- Built-in fonts 14 (default), 20, 28, and 48 enabled; no external fonts.
- `LV_USE_OS` none, logging disabled, unused optional features left off.
- Compiled with `-DLV_CONF_INCLUDE_SIMPLE` so `lv_conf_internal.h` picks up the
  local `lv_conf.h`.

## Build

`Makefile` targets:

| Target | Behavior |
|--------|----------|
| `all` / `make` | Fetch LVGL if missing, build native `build/lvgl_clock` |
| `ARCH=arm64` | Same but `CC=aarch64-linux-gnu-gcc`, output `build/lvgl_clock_arm64` |
| `fetch` | Run `fetch_lvgl.sh` only |
| `test` | Build and run `test_clock_math` with the native compiler |
| `clean` | Remove `build/` |
| `distclean` | Remove `build/` and `third_party/` |

LVGL sources are compiled into `build/lvgl/lib/liblvgl.a`; the demo links it plus
`clock_math.c` and `clock_ui.c` with `-lm`. The fetch script verifies the
download with a size check and reuses an existing `third_party/lvgl`.

Failure modes:

- Download failure or missing `curl`/`tar`: clear error and non-zero exit.
- Missing `aarch64-linux-gnu-gcc` for `ARCH=arm64`: clear error.
- Missing `/dev/fb0` at runtime: error message suggesting a QEMU `--gui` or
  `--vnc` boot.
- `KD_GRAPHICS`/`VT_ACTIVATE` permission failures: warn and continue.

## Guest usage

```bash
/mnt/linux-app/lvgl-clock/start-clock.sh
```

The script checks `/dev/fb0` and that `build/lvgl_clock_arm64` exists and matches
`uname -m` (arm64). Missing or stale binaries print the host build command
(`make ARCH=arm64`) and exit; the guest does not compile LVGL. Then it execs the
clock. Exit with Ctrl+C from the serial console; the app restores the text
console. `TZ=Asia/Shanghai` can be exported before launch.

## Verification

- `make -C linux-app/lvgl-clock test` passes: angles for 00:00:00, 03:00:00,
  06:30:00, 23:59:59.5, and endpoint directions at 0/90/180/270 degrees.
- `make -C linux-app/lvgl-clock` builds natively.
- `make -C linux-app/lvgl-clock ARCH=arm64` builds an aarch64 binary (checked
  with `file`).
- Boot the arm64 guest with `--gui` or `--vnc` and confirm: dial and ticks are
  drawn, hands track system time (compare with `date`), second hand sweeps
  smoothly, digital time/date update, Ctrl+C restores the console.
- Confirm no Xorg is required and `run-all.sh` remains unaffected.
