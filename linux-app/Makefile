SUBDIRS := process-fork pthread-sum pipe-ipc shm-sem signal-demo epoll-server mmap-file ebpf kernel-perf kernel-trace
GUI_DIR := stopwatch-qt
LVGL_DIR := lvgl-clock

all:
	@for dir in $(SUBDIRS); do \
		echo "==> $$dir"; \
		$(MAKE) -C $$dir || exit 1; \
	done

test:
	./run-all.sh

test-perf:
	./kernel-perf/run_perf.sh all

test-trace:
	./kernel-trace/trace_demo.sh all

test-ebpf:
	./ebpf/run_ebpf.sh all

gui:
	$(MAKE) -C $(GUI_DIR)

lvgl:
	$(MAKE) -C $(LVGL_DIR)

lvgl-arm64:
	$(MAKE) -C $(LVGL_DIR) ARCH=arm64

modules:
	./kernel-drivers/build.sh

modules-clean:
	./kernel-drivers/build.sh clean

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	$(MAKE) -C $(GUI_DIR) clean
	$(MAKE) -C $(LVGL_DIR) clean

distclean: clean
	$(MAKE) -C $(LVGL_DIR) distclean

.PHONY: all test test-perf test-trace test-ebpf gui lvgl lvgl-arm64 modules modules-clean clean distclean
