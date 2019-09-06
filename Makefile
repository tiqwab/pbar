CC := gcc
EXECUTABLE := pbar
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: default clean

default: $(EXECUTABLE)

$(EXECUTABLE): pbar.c
	$(CC) -o $@ -Wall $<

install: $(EXECUTABLE)
	install -pm0755 $(EXECUTABLE) $(BINDIR)/$(EXECUTABLE)

clean:
	rm -rf $(EXECUTABLE)
	rm -rf $(BINDIR)/$(EXECUTABLE)
