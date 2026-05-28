# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Masoud Bolhassani


CC ?= gcc
CPPFLAGS ?= -I./include
CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -Wunused-result
LDFLAGS ?=
LDLIBS = -lncurses
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
TEST_BIN = $(BUILD_DIR)/test_trafix
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
TARGET = $(BIN_DIR)/trafix
VERSION := $(shell grep -v '^#' VERSION | head -n 1)
CPPFLAGS += -DTRFX_VERSION=\"$(VERSION)\"
TAG := v$(VERSION)
TARBALL := trafix-$(VERSION).tar.gz
PREFIX ?= /usr
SOURCEDIR := $(HOME)/rpmbuild/SOURCES
SPECDIR := $(HOME)/rpmbuild/SPECS

# Default build
all: $(BIN_DIR) $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $(TARGET) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_BIN): tests/test_trafix.c src/trfx_utils.c src/trfx_config.c src/trfx_cli.c src/trfx_version.c src/trfx_connections.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

test: $(TEST_BIN)
	$(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Version bumping script
bump:
	@read -p "Enter version bump type (patch, minor, major): " bump_type; \
	./scripts/bump-version.sh $$bump_type

# Git tag from version file
tag:
	@if git rev-parse $(TAG) >/dev/null 2>&1; then \
		echo "Tag $(TAG) already exists."; \
	else \
		git tag -a $(TAG) -m "Release $(TAG)"; \
		git push origin $(TAG); \
	fi

# Note: Tarball creation is no longer needed - GitHub auto-generates archives

# Copy spec to rpmbuild
copy-spec:
	./scripts/copy-spec.sh $(SPECDIR)/

# Build RPM
rpm: copy-spec
	spectool -g -R $(SPECDIR)/trafix.spec
	rpmbuild -ba $(SPECDIR)/trafix.spec

# Installation
install: install-bin

install-bin:
	install -D -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/trafix

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/trafix
	rm -rf $(DESTDIR)/usr/share/doc/trafix

# Clean build files
clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all test clean install install-bin install-doc uninstall bump tag tarball copy-spec rpm
