# Global Makefile for WinBridge
# Builds and installs native C++ executables (winbridge, winbridge-backend, winbridge-manager)

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
	install -d $(DESTDIR)$(DOCDIR)
	install -m 644 README.md $(DESTDIR)$(DOCDIR)/README.md
	@if [ -z "$(DESTDIR)" ] && command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database $(DESTDIR)$(APPLICATIONSDIR) || true; \
	fi
	@echo "WinBridge successfully installed to $(PREFIX)"

install-user: build
	$(MAKE) install PREFIX=$(HOME)/.local
	@if command -v xdg-mime >/dev/null 2>&1; then \
		for mime in application/x-ms-dos-executable application/x-msdownload application/vnd.microsoft.portable-executable; do \
			xdg-mime default winbridge.desktop $$mime || true; \
		done; \
	fi

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/winbridge
	rm -f $(DESTDIR)$(BINDIR)/winbridge-backend
	rm -f $(DESTDIR)$(BINDIR)/winbridge-manager
	rm -f $(DESTDIR)$(APPLICATIONSDIR)/winbridge.desktop
	rm -f $(DESTDIR)$(APPLICATIONSDIR)/winbridge-manager.desktop
	rm -f $(DESTDIR)$(PIXMAPSDIR)/winbridge.png
	rm -rf $(DESTDIR)$(DOCDIR)
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
