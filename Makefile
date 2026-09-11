# Global Makefile for WinBridge
# Builds and installs native C++ executables (winbridge, winbridge-backend, winbridge-manager)

# Auto-detect PREFIX: default to /usr/local if root or writable, else fallback to ~/.local
ifeq ($(filter command line environment% override,$(origin PREFIX)),)
    ifeq ($(shell test -w /usr/local/bin 2>/dev/null && echo 1 || echo 0),1)
        PREFIX := /usr/local
    else ifeq ($(shell id -u),0)
        PREFIX := /usr/local
    else
        PREFIX := $(HOME)/.local
    endif
endif

PREFIX ?= /usr/local
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
APPLICATIONSDIR ?= $(DATADIR)/applications
PIXMAPSDIR ?= $(DATADIR)/pixmaps
DOCDIR ?= $(DATADIR)/doc/winbridge
BUILD_DIR ?= build
CMAKE ?= cmake
CTEST ?= ctest

.PHONY: all build test install install-user uninstall clean help

all: build

build:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD_DIR) --parallel

test:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
	$(CMAKE) --build $(BUILD_DIR) --parallel
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

install: build
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_DIR)/winbridge $(DESTDIR)$(BINDIR)/winbridge
	install -m 755 $(BUILD_DIR)/winbridge-backend $(DESTDIR)$(BINDIR)/winbridge-backend
	install -m 755 $(BUILD_DIR)/manager/winbridge-manager $(DESTDIR)$(BINDIR)/winbridge-manager
	install -d $(DESTDIR)$(APPLICATIONSDIR)
	install -m 644 packaging/winbridge.desktop $(DESTDIR)$(APPLICATIONSDIR)/winbridge.desktop
	install -m 644 packaging/winbridge-manager.desktop $(DESTDIR)$(APPLICATIONSDIR)/winbridge-manager.desktop
	install -d $(DESTDIR)$(PIXMAPSDIR)
	install -m 644 assets/winbridge.png $(DESTDIR)$(PIXMAPSDIR)/winbridge.png
	install -m 644 assets/winbridge.png $(DESTDIR)$(PIXMAPSDIR)/winbridge-manager.png
	@for size in 16 22 24 32 48 64 128 256 512; do \
		install -d $(DESTDIR)$(DATADIR)/icons/hicolor/$${size}x$${size}/apps; \
		if [ -f assets/icons/$${size}x$${size}/winbridge.png ]; then \
			install -m 644 assets/icons/$${size}x$${size}/winbridge.png $(DESTDIR)$(DATADIR)/icons/hicolor/$${size}x$${size}/apps/winbridge.png; \
			install -m 644 assets/icons/$${size}x$${size}/winbridge.png $(DESTDIR)$(DATADIR)/icons/hicolor/$${size}x$${size}/apps/winbridge-manager.png; \
		fi; \
	done
	install -d $(DESTDIR)$(DOCDIR)
	install -m 644 README.md $(DESTDIR)$(DOCDIR)/README.md
	install -m 644 INSTALL.md $(DESTDIR)$(DOCDIR)/INSTALL.md
	@# Overwrite and upgrade legacy installation in ~/.local/share/winbridge if present
	@if [ -z "$(DESTDIR)" ] && [ -d "$(HOME)/.local/share/winbridge" ]; then \
		echo "Overwriting existing installed WinBridge files in $(HOME)/.local/share/winbridge..."; \
		install -m 755 $(BUILD_DIR)/winbridge $(HOME)/.local/share/winbridge/winbridge; \
		install -m 755 $(BUILD_DIR)/winbridge-backend $(HOME)/.local/share/winbridge/winbridge-backend; \
		install -m 755 $(BUILD_DIR)/manager/winbridge-manager $(HOME)/.local/share/winbridge/winbridge-manager; \
		install -m 644 assets/winbridge.png $(HOME)/.local/share/winbridge/winbridge.png; \
		rm -f $(HOME)/.local/share/winbridge/manager_backend.py $(HOME)/.local/share/winbridge/shortcuts.py $(HOME)/.local/share/winbridge/shared_space.py; \
		rm -rf $(HOME)/.local/share/winbridge/__pycache__; \
		printf '#!/usr/bin/env python3\nimport sys, os\nexe = os.path.expanduser("~/.local/share/winbridge/winbridge")\nos.execv(exe, [exe] + sys.argv[1:])\n' > $(HOME)/.local/share/winbridge/winbridge.py; \
		chmod 755 $(HOME)/.local/share/winbridge/winbridge.py; \
		printf '#!/usr/bin/env python3\nimport sys, os\nexe = os.path.expanduser("~/.local/share/winbridge/winbridge-manager")\nos.execv(exe, [exe] + sys.argv[1:])\n' > $(HOME)/.local/share/winbridge/manager.py; \
		chmod 755 $(HOME)/.local/share/winbridge/manager.py; \
	fi
	@# Ensure user desktop menu entries point to the new native installation
	@if [ -z "$(DESTDIR)" ] && [ -d "$(HOME)/.local/share/applications" ]; then \
		install -m 644 packaging/winbridge.desktop $(HOME)/.local/share/applications/winbridge.desktop; \
		install -m 644 packaging/winbridge-manager.desktop $(HOME)/.local/share/applications/winbridge-manager.desktop; \
		if command -v update-desktop-database >/dev/null 2>&1; then \
			update-desktop-database $(HOME)/.local/share/applications || true; \
		fi; \
	fi
	@if [ -z "$(DESTDIR)" ] && command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database $(DESTDIR)$(APPLICATIONSDIR) || true; \
	fi
	@if [ -z "$(DESTDIR)" ] && command -v gtk-update-icon-cache >/dev/null 2>&1; then \
		gtk-update-icon-cache -f -t $(DATADIR)/icons/hicolor 2>/dev/null || true; \
	fi
	@if [ -z "$(DESTDIR)" ]; then \
		sh packaging/register-file-associations.sh "$(APPLICATIONSDIR)/winbridge.desktop" "$(abspath $(BINDIR))/winbridge"; \
	fi
	@echo "WinBridge successfully installed and updated (PREFIX=$(PREFIX))"

install-user: build
	$(MAKE) install PREFIX=$(HOME)/.local

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/winbridge
	rm -f $(DESTDIR)$(BINDIR)/winbridge-backend
	rm -f $(DESTDIR)$(BINDIR)/winbridge-manager
	rm -f $(DESTDIR)$(APPLICATIONSDIR)/winbridge.desktop
	rm -f $(DESTDIR)$(APPLICATIONSDIR)/winbridge-manager.desktop
	rm -f $(DESTDIR)$(PIXMAPSDIR)/winbridge.png
	rm -f $(DESTDIR)$(PIXMAPSDIR)/winbridge-manager.png
	@for size in 16 22 24 32 48 64 128 256 512; do \
		rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/$${size}x$${size}/apps/winbridge.png; \
		rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/$${size}x$${size}/apps/winbridge-manager.png; \
	done
	rm -rf $(DESTDIR)$(DOCDIR)
	@if [ -z "$(DESTDIR)" ] && command -v gtk-update-icon-cache >/dev/null 2>&1; then \
		gtk-update-icon-cache -f -t $(DATADIR)/icons/hicolor 2>/dev/null || true; \
	fi
	@if [ -z "$(DESTDIR)" ] && command -v kbuildsycoca6 >/dev/null 2>&1; then \
		kbuildsycoca6 --noincremental 2>/dev/null || true; \
	fi
	@if [ -z "$(DESTDIR)" ] && command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database $(DESTDIR)$(APPLICATIONSDIR) || true; \
	fi
	@echo "WinBridge successfully uninstalled from $(PREFIX)"

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Available Makefile targets:"
	@echo "  make               - Build all native C++ binaries (winbridge, backend, manager)"
	@echo "  make test          - Run native C++ test suite (core-tests, manager-ui)"
	@echo "  make install       - Install system-wide to $(PREFIX) (or use PREFIX=/custom/path)"
	@echo "  make install-user  - Install locally into ~/.local and configure file associations"
	@echo "  make uninstall     - Remove installed files from $(PREFIX)"
	@echo "  make clean         - Remove build directory"
