PREFIX=/usr
BIN_NAME=wb-mcu-fw-flasher
W32_CROSS=i686-w64-mingw32

VERSION := $(shell head -n 1 debian/changelog  | grep -oh -P "\(\K.*(?=\))")
W32_BIN_NAME=wb-mcu-fw-flasher_$(VERSION).exe

ifeq ($(DEB_BUILD_GNU_TYPE),$(DEB_HOST_GNU_TYPE))
       CC=gcc
else
       CC=$(DEB_HOST_GNU_TYPE)-gcc
endif

CC_FLAGS=-Wall -std=c99 -DVERSION=$(VERSION)

$(BIN_NAME): flasher.c libmodbus-$(DEB_HOST_GNU_TYPE)/src/.libs/libmodbus.a
	$(CC)  flasher.c  $(CC_FLAGS) -Ilibmodbus/src -Ilibmodbus-$(DEB_HOST_GNU_TYPE)/src -Llibmodbus-$(DEB_HOST_GNU_TYPE)/src/.libs -static -lmodbus -o $(BIN_NAME)

libmodbus/configure:
	cd libmodbus && ./autogen.sh

libmodbus-$(DEB_HOST_GNU_TYPE)/src/.libs/libmodbus.a: libmodbus/configure
	mkdir -p libmodbus-$(DEB_HOST_GNU_TYPE)
	cd libmodbus-$(DEB_HOST_GNU_TYPE) && ../libmodbus/configure --host $(DEB_HOST_GNU_TYPE) --enable-static=yes --without-documentation --disable-tests
	make -C libmodbus-$(DEB_HOST_GNU_TYPE)

libmodbus-$(W32_CROSS)/src/.libs/libmodbus.a: libmodbus/configure
	mkdir -p libmodbus-$(W32_CROSS)
	cd libmodbus-$(W32_CROSS) && ../libmodbus/configure --host $(W32_CROSS) --enable-static=yes --without-documentation --disable-tests
	make -C libmodbus-$(W32_CROSS)

$(W32_BIN_NAME): flasher.c libmodbus-$(W32_CROSS)/src/.libs/libmodbus.a
	$(W32_CROSS)-gcc flasher.c $(CC_FLAGS) -Ilibmodbus/src -Ilibmodbus-$(W32_CROSS)/src  -mconsole -static  -L libmodbus-$(W32_CROSS)/src/.libs/  -lmodbus -l ws2_32 -o $(W32_BIN_NAME)
	$(W32_CROSS)-strip --strip-unneeded $(W32_BIN_NAME)

win32: $(W32_BIN_NAME)

install: $(BIN_NAME)
	install -Dm0755 $(BIN_NAME) -t $(DESTDIR)$(PREFIX)/bin

clean:
	-@rm -f $(BIN_NAME)

	-@rm -f $(W32_BIN_NAME)
	-@rm -rf libmodbus-*
.PHONY: install clean all win32
