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
- guest：无需额外软件包，只需要内核 framebuffer 和 `/dev/fb0`；但二进制为动态链接，需要 glibc >= 2.34（提供的 Ubuntu 24.04 guest 为 2.39），其他 rootfs 镜像需使用兼容的 glibc 或重新编译。
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
