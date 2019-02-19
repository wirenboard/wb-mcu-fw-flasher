DESTDIR=/
prefix=usr
BIN_NAME=wb-mcu-fw-flasher


ifeq ($(DEB_BUILD_GNU_TYPE),$(DEB_HOST_GNU_TYPE))
       CC=gcc
else
       CC=$(DEB_HOST_GNU_TYPE)-gcc
endif

CC_FLAGS=-Wall -std=c99 `pkg-config --libs --cflags libmodbus`

$(BIN_NAME): flasher.c
	$(CC)  flasher.c  $(CC_FLAGS)  -o $(BIN_NAME)


install: $(BIN_NAME)
	install -m 0755 $(BIN_NAME) $(DESTDIR)/$(prefix)/bin/$(BIN_NAME)

clean:
	-@rm -f $(BIN_NAME)
.PHONY: install clean all
