DESTDIR=/
prefix=usr
BIN_NAME=wb-mcu-fw-flasher
W32_BIN_NAME=wb-mcu-fw-flasher.exe
W32_CC=i686-w64-mingw32-gcc

VERSION := $(shell head -n 1 debian/changelog  | grep -oh -P "\(\K.*(?=\))")

ifeq ($(DEB_BUILD_GNU_TYPE),$(DEB_HOST_GNU_TYPE))
       CC=gcc
else
       CC=$(DEB_HOST_GNU_TYPE)-gcc
endif

CC_FLAGS=-Wall -std=c99 -DVERSION=$(VERSION)

$(BIN_NAME): flasher.c
	$(CC)  flasher.c  $(CC_FLAGS) `pkg-config --libs --cflags libmodbus` -o $(BIN_NAME)

libmodbus:
	git clone git@github.com:stephane/libmodbus.git

libmodbus/src/.libs/libmodbus.a: libmodbus
	cd libmodbus/ && ./autogen.sh && ./configure --host i686-w64-mingw32 --enable-static=yes --without-documentation --disable-tests
	make -C libmodbus 

$(W32_BIN_NAME): flasher.c libmodbus/src/.libs/libmodbus.a
	$(W32_CC) flasher.c $(CC_FLAGS) -Ilibmodbus/src  -mwindows -static  -L libmodbus/src/.libs/  -lmodbus -l ws2_32 -o $(W32_BIN_NAME)

install: $(BIN_NAME)
	install -m 0755 $(BIN_NAME) $(DESTDIR)/$(prefix)/bin/$(BIN_NAME)

clean:
	-@rm -f $(BIN_NAME)
	-@rm -rf libmodbus
.PHONY: install clean all
