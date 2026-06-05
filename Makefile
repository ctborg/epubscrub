CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?= -lz
PREFIX ?= $(shell if [ -d /opt/homebrew ]; then printf /opt/homebrew; else printf /usr/local; fi)
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
INSTALL ?= install

TARGET := epubscrub
MANPAGE := man/epubscrub.1
SRCS := src/main.c src/sanitize.c src/zip.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean test man install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS)

man: $(MANPAGE)
	@printf '%s\n' "$(MANPAGE)"

install: $(TARGET) $(MANPAGE)
	$(INSTALL) -d "$(BINDIR)" "$(MANDIR)/man1"
	$(INSTALL) -m 755 "$(TARGET)" "$(BINDIR)/$(TARGET)"
	$(INSTALL) -m 644 "$(MANPAGE)" "$(MANDIR)/man1/$(TARGET).1"

uninstall:
	rm -f "$(BINDIR)/$(TARGET)" "$(MANDIR)/man1/$(TARGET).1"

test: $(TARGET)
	./tests/test_epubscrub.sh
