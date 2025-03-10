SYSROOT=/home/batman/src/xcomp-rpiz-env/mnt/
XCOMPILE=\
	 -target arm-linux-gnueabihf \
	 -mcpu=arm1176jzf-s \
	 --sysroot $(SYSROOT)

# Uncomment to build to local target:
#XCOMPILE=

INCLUDES= \
	-I. \
	-I./libeink \
	-I./libwwwslide \

# TODO lgGpio doesn't build with these
#	-Wformat=2 \
#	-Wextra \
#	-Wpedantic  \

CFLAGS= \
	$(XCOMPILE) \
	$(INCLUDES) \
	-fdiagnostics-color=always \
	-ffunction-sections -fdata-sections \
	-ggdb -O3 \
	-std=gnu99 \
	-Wall -Werror \
	-Wendif-labels \
	-Wfloat-equal \
	-Wimplicit-fallthrough \
	-Winit-self \
	-Winvalid-pch \
	-Wmissing-field-initializers \
	-Wmissing-include-dirs \
	-Wno-strict-prototypes \
	-Wno-unused-function \
	-Wno-unused-parameter \
	-Woverflow \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wstrict-aliasing=2 \
	-Wundef \
	-Wuninitialized \


LDFLAGS=-Wl,--gc-sections -lcurl -lcairo -ljson-c

ambiencesvc: \
		build/libwwwslide/wwwslider.o \
		build/libeink/liblgpio/lgCtx.o \
		build/libeink/liblgpio/lgDbg.o \
		build/libeink/liblgpio/lgGpio.o \
		build/libeink/liblgpio/lgHdl.o \
		build/libeink/liblgpio/lgPthAlerts.o \
		build/libeink/liblgpio/lgPthTx.o \
		build/libeink/liblgpio/lgSPI.o \
		build/libeink/libeink/eink.o \
		build/config_base.o \
		build/config.o \
		build/proc_utils.o \
		build/shm.o \
		build/main.o
	clang $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -rf build
	rm ambiencesvc

build/%.o: %.c
	mkdir -p $(shell dirname $@)
	clang $(CFLAGS) -c $^ -o $@
build/%.o: src/%.c
	mkdir -p $(shell dirname $@)
	clang $(CFLAGS) -c $^ -o $@

.PHONY: xcompile-start xcompile-end xcompile-rebuild-sysrootdeps

xcompile-start:
	./rpiz-xcompile/mount_rpy_root.sh ~/src/xcomp-rpiz-env

xcompile-end:
	./rpiz-xcompile/umount_rpy_root.sh ~/src/xcomp-rpiz-env

install_sysroot_deps:
	./rpiz-xcompile/add_sysroot_pkg.sh ~/src/xcomp-rpiz-env http://archive.raspberrypi.com/debian/pool/main/c/cairo/libcairo2-dev_1.16.0-7+rpt1_armhf.deb
	./rpiz-xcompile/add_sysroot_pkg.sh ~/src/xcomp-rpiz-env http://raspbian.raspberrypi.com/raspbian/pool/main/j/json-c/libjson-c-dev_0.16-2_armhf.deb
	./rpiz-xcompile/add_sysroot_pkg.sh ~/src/xcomp-rpiz-env http://raspbian.raspberrypi.com/raspbian/pool/main/c/curl/libcurl4-openssl-dev_7.88.1-10+rpi1+deb12u8_armhf.deb

.PHONY: deploy run
deploy: ambiencesvc
	scp ambiencesvc StoneBakedMargheritaHomeboard:/home/batman/example
run: deploy
	ssh StoneBakedMargheritaHomeboard /home/batman/example

