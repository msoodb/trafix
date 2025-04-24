CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lpcap -lncurses
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
TARGET = $(BIN_DIR)/trafix
VERSION := $(shell cat VERSION)

all: $(BIN_DIR) $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

install: install-bin install-doc

install-bin:
	install -D -m 0755 $(TARGET) $(DESTDIR)/usr/bin/trafix

install-doc:
	install -D -m 0644 LICENSE $(DESTDIR)/usr/share/doc/trafix/LICENSE
	install -D -m 0644 README.md $(DESTDIR)/usr/share/doc/trafix/README.md

uninstall:
	rm -f $(DESTDIR)/usr/bin/trafix
	rm -rf $(DESTDIR)/usr/share/doc/trafix

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR) $(BIN_DIR)

tarball:
	git archive --format=tar.gz --prefix=trafix-$(VERSION)/ HEAD > $(HOME)/rpmbuild/SOURCES/trafix-$(VERSION).tar.gz

copy-spec:
	cp trafix.spec $(HOME)/rpmbuild/SPECS/

rpm: tarball copy-spec
	rpmbuild -ba $(HOME)/rpmbuild/SPECS/trafix.spec
	rpmbuild -bs $(HOME)/rpmbuild/SPECS/trafix.spec

.PHONY: all clean install install-bin install-doc uninstall tarball copy-spec rpm
