# SPDX-License-Identifier: GPL-3.0-only

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
CFLAGS += -Wunused-result
LDFLAGS = -lpcap -lncurses
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
TARGET = $(BIN_DIR)/trafix
VERSION := $(shell cat VERSION)
TAG := v$(VERSION)
TARBALL := trafix-$(VERSION).tar.gz
PREFIX := trafix-$(VERSION)
SOURCEDIR := $(HOME)/rpmbuild/SOURCES
SPECDIR := $(HOME)/rpmbuild/SPECS

# Default build
all: $(BIN_DIR) $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

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

# Create tarball from Git tag
tarball:
	git archive --format=tar.gz --prefix=$(PREFIX)/ $(TAG) > $(SOURCEDIR)/$(TARBALL)

# Copy spec to rpmbuild
copy-spec:
	./scripts/copy-spec.sh $(SPECDIR)/

# Build RPM
rpm: tarball copy-spec
	rpmbuild -ba $(SPECDIR)/trafix.spec
	rpmbuild -bs $(SPECDIR)/trafix.spec

# Full release process
release: tag rpm

# Installation
install: install-bin

install-bin:
	install -D -m 0755 $(TARGET) $(DESTDIR)/usr/bin/trafix

uninstall:
	rm -f $(DESTDIR)/usr/bin/trafix
	rm -rf $(DESTDIR)/usr/share/doc/trafix

# Clean build files
clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean install install-bin install-doc uninstall bump tag tarball copy-spec rpm release
