# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Masoud Bolhassani


CC ?= gcc
CPPFLAGS ?= -I./include
CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -Wunused-result
LDFLAGS ?=
LDLIBS = -lncurses
SRC_DIR = src
BUILD_ROOT = build
MODE ?= release
BUILD_DIR = $(BUILD_ROOT)/$(MODE)
BIN_DIR = bin
TEST_BINS = \
	$(BUILD_DIR)/test_utils \
	$(BUILD_DIR)/test_config \
	$(BUILD_DIR)/test_runtime \
	$(BUILD_DIR)/test_globals \
	$(BUILD_DIR)/test_cli \
	$(BUILD_DIR)/test_cli_output \
	$(BUILD_DIR)/test_version \
	$(BUILD_DIR)/test_connections \
	$(BUILD_DIR)/test_netinfo \
	$(BUILD_DIR)/test_bandwidth \
	$(BUILD_DIR)/test_diagnostics \
	$(BUILD_DIR)/test_actions
BENCH_BINS = \
	$(BUILD_DIR)/bench_socket_owners
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
TARGET = $(BIN_DIR)/trafix
VERSION := $(shell grep -v '^#' VERSION | head -n 1)
CPPFLAGS += -DTRFX_VERSION=\"$(VERSION)\"
TAG := v$(VERSION)
TARBALL := trafix-$(VERSION).tar.gz
PREFIX ?= /usr
DATAROOTDIR ?= $(PREFIX)/share
DOCDIR ?= $(DATAROOTDIR)/doc/trafix
MANDIR ?= $(DATAROOTDIR)/man
SYSCONFDIR ?= /etc
SOURCEDIR := $(HOME)/rpmbuild/SOURCES
SPECDIR := $(HOME)/rpmbuild/SPECS

# Default build
all: $(BIN_DIR) $(TARGET)

debug:
	$(MAKE) clean
	$(MAKE) MODE=debug CFLAGS="-O0 -g3 -Wall -Wextra -Wunused-result" all

asan:
	$(MAKE) clean
	$(MAKE) MODE=asan CFLAGS="-O1 -g -Wall -Wextra -Wunused-result -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined" all

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $(TARGET) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_utils: tests/test_utils.c src/trfx_utils.c src/trfx_globals.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_config: tests/test_config.c src/trfx_config.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_runtime: tests/test_runtime.c src/trfx_runtime.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_globals: tests/test_globals.c src/trfx_globals.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_cli: tests/test_cli.c src/trfx_cli.c src/trfx_version.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_cli_output: tests/test_cli_output.c src/trfx_cli_output.c src/trfx_diagnostics.c src/trfx_netinfo.c src/trfx_sysinfo.c src/trfx_cpu.c src/trfx_meminfo.c src/trfx_disk.c src/trfx_procinfo.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_version: tests/test_version.c src/trfx_version.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_connections: tests/test_connections.c src/trfx_connections.c src/trfx_socket_owners.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_netinfo: tests/test_netinfo.c src/trfx_netinfo.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_bandwidth: tests/test_bandwidth.c src/trfx_bandwidth.c src/trfx_netinfo.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_diagnostics: tests/test_diagnostics.c src/trfx_diagnostics.c src/trfx_netinfo.c src/trfx_sysinfo.c src/trfx_cpu.c src/trfx_meminfo.c src/trfx_disk.c src/trfx_procinfo.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/test_actions: tests/test_actions.c src/trfx_actions.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		$$test_bin || exit 1; \
	done

$(BUILD_DIR)/bench_socket_owners: benchmarks/bench_socket_owners.c src/trfx_socket_owners.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

benchmark: $(BENCH_BINS)
	@for bench_bin in $(BENCH_BINS); do \
		$$bench_bin || exit 1; \
	done

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
install: install-bin install-man install-config install-doc

install-bin:
	install -D -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/trafix

install-man:
	install -D -m 644 man/trafix.1 $(DESTDIR)$(MANDIR)/man1/trafix.1

install-config:
	install -D -m 644 config/config.cfg $(DESTDIR)$(SYSCONFDIR)/trafix/config.cfg

install-doc:
	install -D -m 644 README.md $(DESTDIR)$(DOCDIR)/README.md
	install -D -m 644 RELEASE.md $(DESTDIR)$(DOCDIR)/RELEASE.md
	install -D -m 644 docs/cli_contract.md $(DESTDIR)$(DOCDIR)/docs/cli_contract.md
	install -D -m 644 docs/debian_packaging.md $(DESTDIR)$(DOCDIR)/docs/debian_packaging.md
	install -D -m 644 docs/performance_notes.md $(DESTDIR)$(DOCDIR)/docs/performance_notes.md
	install -D -m 644 docs/trafix_features.md $(DESTDIR)$(DOCDIR)/docs/trafix_features.md
	install -D -m 644 docs/trafix_technical_doc.md $(DESTDIR)$(DOCDIR)/docs/trafix_technical_doc.md
	install -D -m 644 docs/tui_manual_test_checklist.md $(DESTDIR)$(DOCDIR)/docs/tui_manual_test_checklist.md

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/trafix
	rm -f $(DESTDIR)$(MANDIR)/man1/trafix.1
	rm -f $(DESTDIR)$(SYSCONFDIR)/trafix/config.cfg
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(SYSCONFDIR)/trafix 2>/dev/null || true
	rm -rf $(DESTDIR)$(DOCDIR)

# Clean build files
clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_ROOT) $(BIN_DIR)

.PHONY: all debug asan test benchmark clean install install-bin install-man install-config install-doc uninstall bump tag tarball copy-spec rpm
