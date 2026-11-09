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
