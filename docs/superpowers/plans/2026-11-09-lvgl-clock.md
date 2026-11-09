# LVGL Clock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an LVGL 9.3 analog clock demo under `linux-app/lvgl-clock` that draws hour/minute/second hands, digital time, and date from the system clock directly to `/dev/fb0`.

**Architecture:** A platform file (`main.c`) initializes LVGL, creates the fbdev display, isolates the console VT, and runs the LVGL main loop. A UI module (`clock_ui.c`) owns all widgets and a 50 ms timer. A pure-math module (`clock_math.c`) converts time to hand angles and is covered by a host unit test. LVGL v9.3.0 is downloaded by a build script into a gitignored `third_party/` directory and built as a static library for either the host or aarch64.

**Tech Stack:** C11, LVGL 9.3.0 (`lv_linux_fbdev`, built-in Montserrat fonts), Linux framebuffer/VT ioctls, gcc / aarch64-linux-gnu-gcc, GNU make, bash.

## Global Constraints

- LVGL version is exactly `9.3.0`, downloaded from `https://github.com/lvgl/lvgl/archive/refs/tags/v9.3.0.tar.gz`.
- Display backend is framebuffer only: no X11, Wayland, SDL, or DRM backend.
- No input devices; the demo is display-only.
- Date uses a fixed English weekday table; no CJK fonts.
- `linux-app/Makefile`'s existing `gui` target must not download LVGL or build the clock.
- The user has not requested git commits: do not run `git commit` in any task. Leave changes staged-free for user review.

---

### Task 1: Clock angle math with unit test

**Files:**
- Create: `linux-app/lvgl-clock/clock_math.h`
- Create: `linux-app/lvgl-clock/clock_math.c`
- Create: `linux-app/lvgl-clock/test_clock_math.c`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `typedef struct { float hour; float minute; float second; } clock_angles_t;`
  - `void clock_math_angles(int hour, int minute, int second, long nanosecond, clock_angles_t *angles);`
  - `void clock_math_endpoint(float center_x, float center_y, float angle_deg, float length, float *x, float *y);`
  - Angles are degrees clockwise from 12 o'clock. `clock_math_endpoint` places `angle_deg == 0` above the center; a negative `length` returns the opposite direction (used for hand tails).

- [ ] **Step 1: Write the failing test and the header it includes**

Create `linux-app/lvgl-clock/clock_math.h`:

```c
#ifndef CLOCK_MATH_H
#define CLOCK_MATH_H

typedef struct {
    float hour;
    float minute;
    float second;
} clock_angles_t;

void clock_math_angles(int hour, int minute, int second, long nanosecond, clock_angles_t *angles);
void clock_math_endpoint(float center_x, float center_y, float angle_deg, float length, float *x, float *y);

#endif
```

Create `linux-app/lvgl-clock/test_clock_math.c`:

```c
#include "clock_math.h"

#include <math.h>
#include <stdio.h>

static int failures;

static void expect_near(const char *name, float got, float want, float tol)
{
    if(fabsf(got - want) > tol) {
        printf("FAIL %-24s got %.6f want %.6f\n", name, got, want);
        failures++;
    }
}

static void expect_point(const char *name, float x, float y, float want_x, float want_y)
{
    if(fabsf(x - want_x) > 0.001f || fabsf(y - want_y) > 0.001f) {
        printf("FAIL %-24s got (%.3f, %.3f) want (%.3f, %.3f)\n", name, x, y, want_x, want_y);
        failures++;
    }
}

int main(void)
{
    clock_angles_t a;

    clock_math_angles(0, 0, 0, 0, &a);
    expect_near("00:00:00 hour", a.hour, 0.0f, 0.001f);
    expect_near("00:00:00 minute", a.minute, 0.0f, 0.001f);
    expect_near("00:00:00 second", a.second, 0.0f, 0.001f);

    clock_math_angles(3, 0, 0, 0, &a);
    expect_near("03:00:00 hour", a.hour, 90.0f, 0.001f);
    expect_near("03:00:00 minute", a.minute, 0.0f, 0.001f);

    clock_math_angles(6, 30, 0, 0, &a);
    expect_near("06:30:00 hour", a.hour, 195.0f, 0.001f);
    expect_near("06:30:00 minute", a.minute, 180.0f, 0.001f);

    clock_math_angles(12, 0, 0, 0, &a);
    expect_near("12:00:00 hour", a.hour, 0.0f, 0.001f);

    clock_math_angles(23, 59, 59, 500000000L, &a);
    expect_near("23:59:59.5 second", a.second, 357.0f, 0.001f);
    expect_near("23:59:59.5 minute", a.minute, 359.95f, 0.01f);
    expect_near("23:59:59.5 hour", a.hour, 359.99583f, 0.01f);

    float x, y;
    clock_math_endpoint(100.0f, 100.0f, 0.0f, 50.0f, &x, &y);
    expect_point("endpoint 0 deg", x, y, 100.0f, 50.0f);
    clock_math_endpoint(100.0f, 100.0f, 90.0f, 50.0f, &x, &y);
    expect_point("endpoint 90 deg", x, y, 150.0f, 100.0f);
    clock_math_endpoint(100.0f, 100.0f, 180.0f, 50.0f, &x, &y);
    expect_point("endpoint 180 deg", x, y, 100.0f, 150.0f);
    clock_math_endpoint(100.0f, 100.0f, 270.0f, 50.0f, &x, &y);
    expect_point("endpoint 270 deg", x, y, 50.0f, 100.0f);

    if(failures) {
        printf("FAIL clock_math (%d checks)\n", failures);
        return 1;
    }
    printf("PASS clock_math\n");
    return 0;
}
```

- [ ] **Step 2: Run the test and confirm it fails to link**

Run:

```bash
cd linux-app/lvgl-clock
mkdir -p build
cc -O2 -Wall -Wextra -o build/test_clock_math test_clock_math.c -lm
```

Expected: link failure reporting `undefined reference to 'clock_math_angles'` and `'clock_math_endpoint'`.

- [ ] **Step 3: Write the implementation**

Create `linux-app/lvgl-clock/clock_math.c`:

```c
#include "clock_math.h"

#include <math.h>

#define CLOCK_PI 3.14159265358979323846

void clock_math_angles(int hour, int minute, int second, long nanosecond, clock_angles_t *angles)
{
    double sec = (double)second + (double)nanosecond / 1e9;
    double min = (double)minute + sec / 60.0;
    double hr = (double)(hour % 12) + min / 60.0;

    angles->second = (float)(sec * 6.0);
    angles->minute = (float)(min * 6.0);
    angles->hour = (float)(hr * 30.0);
}

void clock_math_endpoint(float center_x, float center_y, float angle_deg, float length, float *x, float *y)
{
    double rad = (double)angle_deg * CLOCK_PI / 180.0;

    *x = center_x + (float)(sin(rad) * (double)length);
    *y = center_y - (float)(cos(rad) * (double)length);
}
```

- [ ] **Step 4: Run the test and confirm it passes**

Run:

```bash
cd linux-app/lvgl-clock
cc -O2 -Wall -Wextra -o build/test_clock_math clock_math.c test_clock_math.c -lm
./build/test_clock_math
```

Expected:

```text
PASS clock_math
```

---

### Task 2: LVGL fetch script, config, and build system

**Files:**
- Create: `linux-app/lvgl-clock/fetch_lvgl.sh`
- Create: `linux-app/lvgl-clock/lv_conf.h`
- Create: `linux-app/lvgl-clock/Makefile`
- Create: `linux-app/lvgl-clock/build.sh`

**Interfaces:**
- Consumes: `clock_math.h`/`clock_math.c` from Task 1.
- Produces:
  - `fetch_lvgl.sh` leaves LVGL at `third_party/lvgl/lvgl.h` and `third_party/lvgl/src/`.
  - `make` auto-fetches, then builds `build/lvgl_clock`.
  - `make ARCH=arm64` builds `build/lvgl_clock_arm64`.
  - `make liblvgl` builds `build/obj-$(ARCH)/liblvgl.a`.
  - `make test` runs the Task 1 unit test.

- [ ] **Step 1: Write the fetch script**

Create `linux-app/lvgl-clock/fetch_lvgl.sh`:

```bash
#!/bin/bash
set -euo pipefail

VERSION="9.3.0"
ROOT="$(cd "$(dirname "$0")" >/dev/null 2>&1 && pwd -P)"
DEST="$ROOT/third_party"
TARGET="$DEST/lvgl"
URL="https://github.com/lvgl/lvgl/archive/refs/tags/v${VERSION}.tar.gz"

if [[ -f "$TARGET/lvgl.h" && -d "$TARGET/src" ]]; then
    echo "LVGL v${VERSION} already present in $TARGET"
    exit 0
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "curl is required to download LVGL." >&2
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading LVGL v${VERSION} ..."
curl -fL --retry 3 -o "$TMP/lvgl.tar.gz" "$URL"

SIZE="$(stat -c %s "$TMP/lvgl.tar.gz")"
if [[ "$SIZE" -lt 50000000 ]]; then
    echo "Downloaded archive is too small ($SIZE bytes); refusing to continue." >&2
    exit 1
fi

tar -tzf "$TMP/lvgl.tar.gz" >/dev/null

tar -xzf "$TMP/lvgl.tar.gz" -C "$TMP" \
    "lvgl-${VERSION}/src" \
    "lvgl-${VERSION}/lvgl.h" \
    "lvgl-${VERSION}/lv_version.h" \
    "lvgl-${VERSION}/lvgl_private.h" \
    "lvgl-${VERSION}/COPYRIGHTS.md" \
    "lvgl-${VERSION}/README.md"

rm -rf "$TARGET"
mkdir -p "$TARGET"
mv "$TMP/lvgl-${VERSION}"/* "$TARGET"/

echo "LVGL v${VERSION} ready in $TARGET"
```

- [ ] **Step 2: Write the minimal LVGL configuration**

Create `linux-app/lvgl-clock/lv_conf.h`:

```c
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (128U * 1024U)
#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 0
#define LV_DEF_REFR_PERIOD 33
#define LV_USE_FLOAT 0

#define LV_USE_LINUX_FBDEV 1
#define LV_LINUX_FBDEV_BSD 0
#define LV_LINUX_FBDEV_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_LINUX_FBDEV_BUFFER_COUNT 0
#define LV_LINUX_FBDEV_BUFFER_SIZE 60
#define LV_LINUX_FBDEV_MMAP 1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif
```

- [ ] **Step 3: Write the Makefile**

Create `linux-app/lvgl-clock/Makefile`:

```make
LVGL_DIR := third_party/lvgl
BUILD_DIR := build

ARCH ?= $(shell uname -m)
ifneq ($(filter arm64 aarch64,$(ARCH)),)
ARCH := arm64
endif

ifeq ($(ARCH),arm64)
CROSS_COMPILE ?= aarch64-linux-gnu-
BIN_NAME := lvgl_clock_arm64
else
CROSS_COMPILE ?=
BIN_NAME := lvgl_clock
endif

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
OBJ_DIR := $(BUILD_DIR)/obj-$(ARCH)
BIN := $(BUILD_DIR)/$(BIN_NAME)

ifeq ($(ARCH),arm64)
ifeq ($(shell command -v $(CC) >/dev/null 2>&1 && echo yes),)
$(error $(CC) not found; install gcc-aarch64-linux-gnu)
endif
endif

CFLAGS ?= -O2 -Wall -Wextra
CPPFLAGS += -DLV_CONF_INCLUDE_SIMPLE -I. -I$(LVGL_DIR)
LDLIBS := -lm

LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name '*.c' 2>/dev/null)
LVGL_OBJS := $(patsubst $(LVGL_DIR)/src/%.c,$(OBJ_DIR)/lvgl/%.o,$(LVGL_SRCS))
APP_OBJS := $(OBJ_DIR)/main.o $(OBJ_DIR)/clock_ui.o $(OBJ_DIR)/clock_math.o
LIBLVGL := $(OBJ_DIR)/liblvgl.a

.PHONY: all fetch liblvgl test clean distclean

all:
	@test -f $(LVGL_DIR)/lvgl.h || ./fetch_lvgl.sh
	@$(MAKE) --no-print-directory $(BIN)

fetch:
	./fetch_lvgl.sh

liblvgl: $(LIBLVGL)

$(OBJ_DIR)/lvgl/%.o: $(LVGL_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIBLVGL): $(LVGL_OBJS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: %.c clock_ui.h clock_math.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN): $(APP_OBJS) $(LIBLVGL)
	$(CC) $(CFLAGS) -o $@ $(APP_OBJS) $(LIBLVGL) $(LDLIBS)

test:
	@mkdir -p $(BUILD_DIR)
	cc -O2 -Wall -Wextra -o $(BUILD_DIR)/test_clock_math clock_math.c test_clock_math.c -lm
	./$(BUILD_DIR)/test_clock_math

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf third_party
```

- [ ] **Step 4: Write the build wrapper**

Create `linux-app/lvgl-clock/build.sh`:

```bash
#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"
exec make "$@"
```

Then run:

```bash
chmod +x linux-app/lvgl-clock/fetch_lvgl.sh linux-app/lvgl-clock/build.sh
```

- [ ] **Step 5: Fetch LVGL and verify the trimmed tree**

Run:

```bash
cd linux-app/lvgl-clock
./fetch_lvgl.sh
test -f third_party/lvgl/lvgl.h
test -d third_party/lvgl/src
test ! -d third_party/lvgl/examples
find third_party/lvgl -maxdepth 1 -type d
```

Expected: the last command prints exactly `third_party/lvgl` and `third_party/lvgl/src`; the download is about 84 MB.

- [ ] **Step 6: Build the native LVGL archive**

Run:

```bash
cd linux-app/lvgl-clock
make liblvgl
find build -name liblvgl.a -exec ls -l {} +
```

Expected: `liblvgl.a` exists and is several MB.

- [ ] **Step 7: Build the aarch64 LVGL archive**

Run:

```bash
cd linux-app/lvgl-clock
make ARCH=arm64 liblvgl
file build/obj-arm64/lvgl/drivers/display/fb/lv_linux_fbdev.o
```

Expected: `ELF 64-bit LSB relocatable, ARM aarch64`.

---

### Task 3: Clock UI and platform main

**Files:**
- Create: `linux-app/lvgl-clock/clock_ui.h`
- Create: `linux-app/lvgl-clock/clock_ui.c`
- Create: `linux-app/lvgl-clock/main.c`

**Interfaces:**
- Consumes:
  - `clock_angles_t`, `clock_math_angles()`, `clock_math_endpoint()` from Task 1.
  - Build rules and `lv_conf.h` from Task 2.
- Produces:
  - `void clock_ui_create(int width, int height);` — creates all widgets on the active screen and starts the 50 ms refresh timer.
  - Executable `build/lvgl_clock` (native) and `build/lvgl_clock_arm64`.

- [ ] **Step 1: Write the UI header**

Create `linux-app/lvgl-clock/clock_ui.h`:

```c
#ifndef CLOCK_UI_H
#define CLOCK_UI_H

void clock_ui_create(int width, int height);

#endif
```

- [ ] **Step 2: Write the UI module**

Create `linux-app/lvgl-clock/clock_ui.c`:

```c
#define _POSIX_C_SOURCE 200809L

#include "clock_ui.h"

#include <math.h>
#include <time.h>

#include "clock_math.h"
#include "lvgl.h"

typedef struct {
    int32_t r;
    lv_obj_t *hour_hand;
    lv_obj_t *minute_hand;
    lv_obj_t *second_hand;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_point_precise_t hour_pts[2];
    lv_point_precise_t minute_pts[2];
    lv_point_precise_t second_pts[2];
    int32_t last_second_of_day;
} clock_ui_t;

static clock_ui_t ui;

static const char *const weekday_names[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static lv_obj_t *create_hand(lv_obj_t *dial, uint32_t color, int32_t width)
{
    lv_obj_t *hand = lv_line_create(dial);
    lv_obj_set_style_line_color(hand, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(hand, width, 0);
    lv_obj_set_style_line_rounded(hand, true, 0);
    return hand;
}

static void set_hand(lv_obj_t *hand, lv_point_precise_t pts[2], int32_t cx, int32_t cy,
                     float angle, float length, float tail)
{
    float x0, y0, x1, y1;
    lv_value_precise_t nx0, ny0, nx1, ny1;

    clock_math_endpoint((float)cx, (float)cy, angle, -tail, &x0, &y0);
    clock_math_endpoint((float)cx, (float)cy, angle, length, &x1, &y1);

    nx0 = (lv_value_precise_t)lroundf(x0);
    ny0 = (lv_value_precise_t)lroundf(y0);
    nx1 = (lv_value_precise_t)lroundf(x1);
    ny1 = (lv_value_precise_t)lroundf(y1);

    if(pts[0].x == nx0 && pts[0].y == ny0 && pts[1].x == nx1 && pts[1].y == ny1) {
        return;
    }

    pts[0].x = nx0;
    pts[0].y = ny0;
    pts[1].x = nx1;
    pts[1].y = ny1;
    lv_line_set_points(hand, pts, 2);
}

static void create_ticks(lv_obj_t *dial, int32_t r)
{
    static lv_point_precise_t points[60][2];

    for(int i = 0; i < 60; i++) {
        bool major = (i % 5) == 0;
        float angle = (float)i * 6.0f;
        float outer = (float)(r - 6);
        float inner = outer - (major ? 18.0f : 10.0f);
        float x1, y1, x2, y2;

        clock_math_endpoint((float)r, (float)r, angle, inner, &x1, &y1);
        clock_math_endpoint((float)r, (float)r, angle, outer, &x2, &y2);

        points[i][0].x = (lv_value_precise_t)lroundf(x1);
        points[i][0].y = (lv_value_precise_t)lroundf(y1);
        points[i][1].x = (lv_value_precise_t)lroundf(x2);
        points[i][1].y = (lv_value_precise_t)lroundf(y2);

        lv_obj_t *line = lv_line_create(dial);
        lv_line_set_points(line, points[i], 2);
        lv_obj_set_style_line_color(line, lv_color_hex(major ? 0xdfe9f3 : 0x5d6d7e), 0);
        lv_obj_set_style_line_width(line, major ? 4 : 2, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
    }
}

static void create_numerals(lv_obj_t *dial, int32_t r)
{
    for(int i = 1; i <= 12; i++) {
        float x, y;

        clock_math_endpoint(0.0f, 0.0f, (float)i * 30.0f, (float)r * 0.78f, &x, &y);

        lv_obj_t *label = lv_label_create(dial);
        lv_label_set_text_fmt(label, "%d", i);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xdfe9f3), 0);
        lv_obj_align(label, LV_ALIGN_CENTER, (int32_t)lroundf(x), (int32_t)lroundf(y));
    }
}

static void update_clock(void)
{
    struct timespec ts;
    struct tm now;
    clock_angles_t angles;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &now);
    clock_math_angles(now.tm_hour, now.tm_min, now.tm_sec, ts.tv_nsec, &angles);

    set_hand(ui.hour_hand, ui.hour_pts, ui.r, ui.r, angles.hour,
             (float)ui.r * 0.52f, (float)ui.r * 0.12f);
    set_hand(ui.minute_hand, ui.minute_pts, ui.r, ui.r, angles.minute,
             (float)ui.r * 0.75f, (float)ui.r * 0.12f);
    set_hand(ui.second_hand, ui.second_pts, ui.r, ui.r, angles.second,
             (float)ui.r * 0.85f, (float)ui.r * 0.18f);

    int32_t second_of_day = now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec;
    if(second_of_day != ui.last_second_of_day) {
        lv_label_set_text_fmt(ui.time_label, "%02d:%02d:%02d",
                              now.tm_hour, now.tm_min, now.tm_sec);
        lv_label_set_text_fmt(ui.date_label, "%04d-%02d-%02d %s",
                              now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
                              weekday_names[now.tm_wday % 7]);
        ui.last_second_of_day = second_of_day;
    }
}

static void timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_clock();
}

void clock_ui_create(int width, int height)
{
    lv_obj_t *screen = lv_screen_active();

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x1c2a3a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    int32_t r = (int32_t)(fminf((float)width, (float)height) * 0.36f);
    int32_t cx = width / 2;
    int32_t cy = (int32_t)fminf((float)height * 0.40f, (float)(height - (r + 120)));
    if(cy - r < 8) {
        cy = r + 8;
    }

    ui.r = r;
    ui.last_second_of_day = -1;

    lv_obj_t *dial = lv_obj_create(screen);
    lv_obj_remove_style_all(dial);
    lv_obj_set_size(dial, r * 2, r * 2);
    lv_obj_set_pos(dial, cx - r, cy - r);
    lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dial, lv_color_hex(0x111a26), 0);
    lv_obj_set_style_bg_grad_color(dial, lv_color_hex(0x1a2636), 0);
    lv_obj_set_style_bg_grad_dir(dial, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(dial, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dial, 3, 0);
    lv_obj_set_style_border_color(dial, lv_color_hex(0x2f8f9d), 0);
    lv_obj_set_style_shadow_width(dial, 24, 0);
    lv_obj_set_style_shadow_color(dial, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(dial, LV_OPA_50, 0);

    create_ticks(dial, r);
    create_numerals(dial, r);

    ui.hour_hand = create_hand(dial, 0xdfe9f3, 8);
    ui.minute_hand = create_hand(dial, 0xdfe9f3, 5);
    ui.second_hand = create_hand(dial, 0xe74c3c, 2);

    lv_obj_t *cap = lv_obj_create(dial);
    lv_obj_remove_style_all(cap);
    lv_obj_set_size(cap, 16, 16);
    lv_obj_set_pos(cap, r - 8, r - 8);
    lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cap, lv_color_hex(0xe74c3c), 0);
    lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cap, 2, 0);
    lv_obj_set_style_border_color(cap, lv_color_hex(0xffffff), 0);

    int32_t x_ofs = cx - width / 2;
    int32_t y_ofs = cy - height / 2;

    ui.time_label = lv_label_create(screen);
    lv_obj_set_style_text_font(ui.time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui.time_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(ui.time_label, LV_ALIGN_CENTER, x_ofs, y_ofs + r + 34);

    ui.date_label = lv_label_create(screen);
    lv_obj_set_style_text_font(ui.date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.date_label, lv_color_hex(0x8fa3b8), 0);
    lv_obj_align(ui.date_label, LV_ALIGN_CENTER, x_ofs, y_ofs + r + 82);

    update_clock();
    lv_timer_create(timer_cb, 50, NULL);
}
```

- [ ] **Step 3: Write the platform main**

Create `linux-app/lvgl-clock/main.c`:

```c
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "clock_ui.h"
#include "lvgl.h"

static volatile sig_atomic_t running = 1;

static void on_signal(int signo)
{
    (void)signo;
    running = 0;
}

static uint32_t tick_cb(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static int console_graphics(void)
{
    int fd = open("/dev/tty1", O_RDWR | O_CLOEXEC);
    if(fd < 0) {
        fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    }
    if(fd < 0) {
        fprintf(stderr, "lvgl-clock: cannot open console tty: %s\n", strerror(errno));
        return -1;
    }

    int vt = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if(vt >= 0) {
        if(ioctl(vt, VT_ACTIVATE, 1) != 0 || ioctl(vt, VT_WAITACTIVE, 1) != 0) {
            fprintf(stderr, "lvgl-clock: VT activate failed: %s (continuing)\n", strerror(errno));
        }
        close(vt);
    }

    if(ioctl(fd, KDSETMODE, KD_GRAPHICS) != 0) {
        fprintf(stderr, "lvgl-clock: KD_GRAPHICS failed: %s (continuing)\n", strerror(errno));
    }

    return fd;
}

static void console_restore(int fd)
{
    if(fd < 0) {
        return;
    }

    if(ioctl(fd, KDSETMODE, KD_TEXT) != 0) {
        fprintf(stderr, "lvgl-clock: KD_TEXT restore failed: %s\n", strerror(errno));
    }
    close(fd);
}

int main(void)
{
    struct sigaction sa;
    struct timespec delay = { 0, 5 * 1000 * 1000L };

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if(access("/dev/fb0", R_OK | W_OK) != 0) {
        fprintf(stderr, "lvgl-clock: /dev/fb0 is not usable: %s\n", strerror(errno));
        fprintf(stderr, "Boot QEMU with --gui or --vnc so virtio-gpu provides a framebuffer.\n");
        return 1;
    }

    int tty_fd = console_graphics();

    lv_init();
    lv_tick_set_cb(tick_cb);

    lv_display_t *disp = lv_linux_fbdev_create();
    if(disp == NULL) {
        fprintf(stderr, "lvgl-clock: failed to create the fbdev display\n");
        lv_deinit();
        console_restore(tty_fd);
        return 1;
    }
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    int width = lv_display_get_horizontal_resolution(disp);
    int height = lv_display_get_vertical_resolution(disp);
    if(width <= 0 || height <= 0) {
        fprintf(stderr, "lvgl-clock: framebuffer resolution is unavailable\n");
        lv_deinit();
        console_restore(tty_fd);
        return 1;
    }

    clock_ui_create(width, height);

    while(running) {
        lv_timer_handler();
        nanosleep(&delay, NULL);
    }

    lv_deinit();
    console_restore(tty_fd);
    return 0;
}
```

- [ ] **Step 4: Build the native binary**

Run:

```bash
cd linux-app/lvgl-clock
make
file build/lvgl_clock
```

Expected: build succeeds; `file` reports `ELF 64-bit LSB pie executable, x86-64`.

- [ ] **Step 5: Build the arm64 binary**

Run:

```bash
cd linux-app/lvgl-clock
make ARCH=arm64
file build/lvgl_clock_arm64
```

Expected: `ELF 64-bit LSB pie executable, ARM aarch64`.

- [ ] **Step 6: Re-run the unit test through the Makefile**

Run:

```bash
cd linux-app/lvgl-clock
make test
```

Expected: `PASS clock_math`.

---

### Task 4: Guest launcher, documentation, and top-level integration

**Files:**
- Create: `linux-app/lvgl-clock/start-clock.sh`
- Create: `linux-app/lvgl-clock/README.md`
- Modify: `linux-app/.gitignore`
- Modify: `linux-app/Makefile`
- Modify: `linux-app/README.md`

**Interfaces:**
- Consumes: `build/lvgl_clock_arm64` from Task 3.
- Produces: guest entry point `/mnt/linux-app/lvgl-clock/start-clock.sh`; `make lvgl` and `make lvgl-arm64` from `linux-app/`.

- [ ] **Step 1: Write the guest launcher**

Create `linux-app/lvgl-clock/start-clock.sh`:

```bash
#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" >/dev/null 2>&1 && pwd -P)"
BIN="$DIR/build/lvgl_clock_arm64"

if [[ ! -e /dev/fb0 ]]; then
    echo "No /dev/fb0. Boot QEMU with --gui or --vnc so virtio-gpu provides a framebuffer." >&2
    exit 1
fi

case "$(uname -m)" in
    aarch64|arm64) ;;
    *)
        echo "lvgl_clock_arm64 targets arm64; current architecture is $(uname -m)." >&2
        exit 1
        ;;
esac

if [[ ! -x "$BIN" ]]; then
    echo "Missing $BIN." >&2
    echo "On the host run: make -C linux-app/lvgl-clock ARCH=arm64" >&2
    exit 1
fi

if find "$DIR" -maxdepth 1 \( -name '*.c' -o -name '*.h' \) -newer "$BIN" -print -quit | grep -q .; then
    echo "$BIN is older than its sources." >&2
    echo "On the host run: make -C linux-app/lvgl-clock ARCH=arm64" >&2
    exit 1
fi

exec "$BIN"
```

Then run:

```bash
chmod +x linux-app/lvgl-clock/start-clock.sh
```

- [ ] **Step 2: Write the demo README**

Create `linux-app/lvgl-clock/README.md`:

```markdown
# lvgl-clock

LVGL 9.3 实现的模拟时钟，显示系统时间：时分秒指针、`HH:MM:SS` 数字时间和
日期。直接写入 `/dev/fb0`，不依赖 Xorg、Wayland 或 SDL。

## 功能

- 60 个刻度（12 个主刻度）和 1–12 数字表盘。
- 时/分/秒三根指针；秒针按 `CLOCK_REALTIME` 的亚秒平滑扫动。
- 表盘下方显示 `HH:MM:SS` 和 `YYYY-MM-DD Www`。
- 分辨率自适应（800×600、1024×768 等）。
- 启动时切到 VT1 并设置 `KD_GRAPHICS` 接管控制台；退出（Ctrl+C）后恢复文本模式。
- 无输入设备依赖，50 ms 刷新一次指针，数字每秒刷新。

## 依赖

- 宿主机：`gcc`、`make`、`curl`、`tar`。
- 交叉编译 arm64：`aarch64-linux-gnu-gcc`（Ubuntu 包
  `gcc-aarch64-linux-gnu`）。
- guest：无需额外软件包，只需要内核 framebuffer 和 `/dev/fb0`。
- 首次构建会下载 LVGL v9.3.0 到 `third_party/`（约 84 MB，已 gitignore）。

## 构建

宿主机原生构建（仅用于编译检查）：

```bash
make
```

为 QEMU arm64 guest 交叉编译：

```bash
make ARCH=arm64
```

产物：

- `build/lvgl_clock`：宿主机 x86_64 版本。
- `build/lvgl_clock_arm64`：guest arm64 版本。

其他目标：

```bash
make test       # 指针角度单元测试
make clean      # 删除 build/
make distclean  # 删除 build/ 和 third_party/
```

## 在 QEMU guest 中运行

带图形设备启动（`--gui` 或 `--vnc`），登录 guest 后：

```bash
/mnt/linux-app/lvgl-clock/start-clock.sh
```

按 Ctrl+C 退出并恢复文本控制台。时钟占用 VT1，运行前不要启动
`stopwatch-qt/start-gui.sh` 等 Xorg 会话；如果 Xorg 已在运行，先退出 Xorg。

时区由系统环境和 `TZ` 决定：

```bash
TZ=Asia/Shanghai /mnt/linux-app/lvgl-clock/start-clock.sh
```

## 常见问题

- 报错 `No /dev/fb0`：QEMU 没有虚拟 GPU 或用了 `-nographic`。改用 `--gui`
  或 `--vnc <display>` 启动。
- 屏幕仍显示登录提示或光标闪烁：`KD_GRAPHICS` 设置失败（通常是没有 root
  权限）。以 root 登录 guest 再运行。
- 退出后登录提示没有立即出现：按一次回车即可让 getty 重新输出提示。
```

- [ ] **Step 3: Ignore build artifacts and third-party sources**

Append to `linux-app/.gitignore`:

```text
lvgl-clock/build/
lvgl-clock/third_party/
```

- [ ] **Step 4: Add top-level make targets**

Modify `linux-app/Makefile`: add `LVGL_DIR := lvgl-clock` after the
`GUI_DIR := stopwatch-qt` line, add the two targets after `gui:`, add the LVGL
cleanup to `clean:`, and extend `.PHONY`:

```make
GUI_DIR := stopwatch-qt
LVGL_DIR := lvgl-clock
```

```make
gui:
	$(MAKE) -C $(GUI_DIR)

lvgl:
	$(MAKE) -C $(LVGL_DIR)

lvgl-arm64:
	$(MAKE) -C $(LVGL_DIR) ARCH=arm64
```

```make
clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	$(MAKE) -C $(GUI_DIR) clean
	$(MAKE) -C $(LVGL_DIR) clean

distclean: clean
	$(MAKE) -C $(LVGL_DIR) distclean
```

```make
.PHONY: all test test-perf test-trace test-ebpf gui lvgl lvgl-arm64 modules modules-clean clean distclean
```

- [ ] **Step 5: Document the demo in the linux-app README**

Add this row to the table in `linux-app/README.md`, directly after the
`snake-tk` row:

```markdown
| `lvgl-clock` | LVGL 模拟表盘时钟（fbdev 直显） | `lv_linux_fbdev`, `lv_timer`, `lv_line` |
```

Add this section directly before `## eBPF 示例`:

```markdown
## LVGL 表盘时钟

`lvgl-clock/` 使用 LVGL 9.3 绘制模拟表盘、数字时间和日期，直接写入
`/dev/fb0`，不依赖 Xorg。首次构建会下载 LVGL 源码到 `third_party/`（需要网络）。

在宿主机交叉编译 arm64 版本：

```bash
make -C lvgl-clock ARCH=arm64
```

在带图形设备的 QEMU guest 中运行（`--gui` 或 `--vnc`）：

```bash
/mnt/linux-app/lvgl-clock/start-clock.sh
```

按 Ctrl+C 退出并恢复文本控制台。时区通过 `TZ` 指定，例如
`TZ=Asia/Shanghai /mnt/linux-app/lvgl-clock/start-clock.sh`。

详见 [lvgl-clock/README.md](lvgl-clock/README.md)。
```

- [ ] **Step 6: Verify scripts and top-level targets**

Run:

```bash
cd linux-app
bash -n lvgl-clock/fetch_lvgl.sh
bash -n lvgl-clock/build.sh
bash -n lvgl-clock/start-clock.sh
make lvgl-arm64
make -C lvgl-clock test
./run-all.sh
```

Expected: all scripts parse; `make lvgl-arm64` rebuilds without errors and
leaves `lvgl-clock/build/lvgl_clock_arm64`; the unit test prints
`PASS clock_math`; `run-all.sh` finishes without any `[FAIL]` lines and exits 0.

- [ ] **Step 7: Manual guest verification**

Run:

```bash
./run-qemu.sh rootfs_arm64.ext4 --arch arm64 --gui
```

(The default image for `--arch arm64` is `rootfs_arm64.ext4`, so the argument
may be omitted.) In the guest serial console:

```bash
/mnt/linux-app/lvgl-clock/start-clock.sh
date
```

Confirm: the QEMU/VNC window shows the dial and all three hands; the second hand
sweeps smoothly; the digital time and date match `date`; Ctrl+C returns to the
text console. Then run `make test` from the host one more time to confirm nothing
regressed.
