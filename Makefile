PREFIX?= /usr/local
CFLAGS:= -std=gnu99 -O2 -DPREFIX=$(PREFIX)
BIN?= $(PREFIX)/bin/ush
MAN?= $(PREFIX)/share/man/man3/ush.3

all: ush

install: ush
	cp $< $(BIN)
	cp ush.3 $(MAN)

uninstall:
	rm -f $(BIN) $(MAN)

clean:
	rm -f ush
