DESTDIR=/
prefix=usr
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
	$(CC)  flasher.c  $(CC_FLAGS) -Ilibmodbus-$(DEB_HOST_GNU_TYPE)/src -Llibmodbus-$(DEB_HOST_GNU_TYPE)/src/.libs -static -lmodbus -o $(BIN_NAME)

libmodbus-$(DEB_HOST_GNU_TYPE):
	git clone -b feature/stopbits-tolerant https://github.com/wirenboard/libmodbus.git $@

libmodbus-$(DEB_HOST_GNU_TYPE)/src/.libs/libmodbus.a: libmodbus-$(DEB_HOST_GNU_TYPE)
	cd $< && ./autogen.sh && ./configure --host $(subst libmodbus-,,$<) --enable-static=yes --without-documentation --disable-tests
	make -C $<

libmodbus-$(W32_CROSS):
	git clone -b feature/stopbits-tolerant https://github.com/wirenboard/libmodbus.git $@

libmodbus-$(W32_CROSS)/src/.libs/libmodbus.a: libmodbus-$(W32_CROSS)
	cd $< && ./autogen.sh && ./configure --host $(subst libmodbus-,,$<) --enable-static=yes --without-documentation --disable-tests
	make -C $<

$(W32_BIN_NAME): flasher.c libmodbus-$(W32_CROSS)/src/.libs/libmodbus.a
	$(W32_CROSS)-gcc flasher.c $(CC_FLAGS) -Ilibmodbus-$(W32_CROSS)/src  -mconsole -static  -L libmodbus-$(W32_CROSS)/src/.libs/  -lmodbus -l ws2_32 -o $(W32_BIN_NAME)
	$(W32_CROSS)-strip --strip-unneeded $(W32_BIN_NAME)

win32: $(W32_BIN_NAME)

install: $(BIN_NAME)
	install -m 0755 $(BIN_NAME) $(DESTDIR)/$(prefix)/bin/$(BIN_NAME)

clean:
	-@rm -f $(BIN_NAME)

	-@rm -f $(W32_BIN_NAME)
	-@rm -rf libmodbus-*
.PHONY: install clean all win32
