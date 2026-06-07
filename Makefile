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
SRCS := src/main.c src/markup.c src/sanitize.c src/zip.c
OBJS := $(SRCS:.c=.o)
FUZZ_TARGET := fuzz_epub
FUZZ_SRCS := fuzz/fuzz_epub.c src/markup.c src/sanitize.c src/zip.c
FUZZ_CORPUS := fuzz/corpus
FUZZ_RUNS ?= 1000

.PHONY: all clean test man install uninstall fuzz-smoke fuzz-libfuzzer

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS) $(FUZZ_TARGET)
	rm -rf $(FUZZ_TARGET).dSYM

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
	./tests/test_markup_sanitize.sh

$(FUZZ_TARGET): $(FUZZ_SRCS)
	$(CC) -g -O1 -fsanitize=address,undefined -DEPUBSCRUB_STANDALONE_FUZZ $(CPPFLAGS) -o $@ $(FUZZ_SRCS) $(LDLIBS)

fuzz-smoke: $(FUZZ_TARGET)
	./$(FUZZ_TARGET) -runs=$(FUZZ_RUNS) $(FUZZ_CORPUS)

fuzz-libfuzzer: $(FUZZ_SRCS)
	$(CC) -g -O1 -fsanitize=fuzzer,address,undefined $(CPPFLAGS) -o $(FUZZ_TARGET) $(FUZZ_SRCS) $(LDLIBS)
