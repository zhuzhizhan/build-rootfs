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
