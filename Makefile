# plasmakey: a KDE Plasma 6 (X11) system tray indicator for Sticky Keys.
# Convenience wrapper around CMake; run `make help` for the targets.
# Installs per user (no root): files go under ~/.local and ~/.config only.

PREFIX      ?= $(HOME)/.local
BUILD_DIR   ?= build
BUILD_TYPE  ?= RelWithDebInfo
QML_DIR     ?= $(PREFIX)/lib/qml
CONFIG_HOME ?= $(or $(XDG_CONFIG_HOME),$(HOME)/.config)
ENV_FILE    ?= $(CONFIG_HOME)/plasma-workspace/env/plasmakey.sh

PLASMOID_ID    := $(shell sed -n 's/.*"Id": *"\([^"]*\)".*/\1/p' package/metadata.json)
PLASMOID_DIR   := $(PREFIX)/share/plasma/plasmoids/$(PLASMOID_ID)
QML_MODULE_DIR := $(QML_DIR)/plasmakey/xkb

# Debian 13 packages needed to build. (Running it needs only Plasma 6 on X11.)
DEB_BUILD_DEPS := build-essential cmake qt6-base-dev qt6-declarative-dev libxcb-xkb-dev libx11-dev
# Optional: lets `make test-live` tap real keys to exercise Sticky Keys itself.
DEB_TEST_DEPS  := xdotool

SESSION := sh scripts/plasma-session.sh

.PHONY: all help deps build test test-live preview install reload uninstall diag clean

all: build

help:
	@echo "plasmakey - targets:"
	@echo "  make deps       check build packages; prints the apt command if any are missing"
	@echo "  make            build into ./$(BUILD_DIR)"
	@echo "  make test       unit tests (no X server needed, changes nothing)"
	@echo "  make test-live  end-to-end test on the running X server (briefly latches/locks"
	@echo "                  modifiers and taps Shift/Ctrl, then restores them)"
	@echo "  make preview    render the icon in all states (light/dark) to ./$(BUILD_DIR)/preview.png"
	@echo "  make install    install for the current user under $(PREFIX)"
	@echo "  make reload     restart plasmashell so it loads (or drops) the indicator"
	@echo "  make uninstall  remove everything 'make install' added"
	@echo "  make diag       print XKB masks/mapping and watch modifier state changes"
	@echo "  make clean      delete ./$(BUILD_DIR)"

deps:
	@missing=""; \
	for p in $(DEB_BUILD_DEPS); do \
		dpkg-query -W -f='$${Status}' $$p 2>/dev/null | grep -q 'ok installed' || missing="$$missing $$p"; \
	done; \
	if [ -n "$$missing" ]; then \
		echo "Missing build packages:$$missing"; \
		echo "Install them with:"; \
		echo "  sudo apt install --no-install-recommends$$missing"; \
		exit 1; \
	fi; \
	echo "All build packages are installed."; \
	command -v xdotool >/dev/null 2>&1 || \
		echo "Optional: 'sudo apt install $(DEB_TEST_DEPS)' enables the key-tap part of 'make test-live'."

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX="$(PREFIX)" -DPLASMAKEY_QMLDIR="$(QML_DIR)"
	cmake --build $(BUILD_DIR) --parallel

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure -LE live

test-live: build
	$(BUILD_DIR)/bin/plasmakey-livetest

preview: build
	QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software $(BUILD_DIR)/bin/plasmakey-preview $(BUILD_DIR)/preview.png

install: build
	cmake --install $(BUILD_DIR)
	$(SESSION) write-env "$(QML_DIR)" "$(ENV_FILE)"
	@echo
	@echo "Installed '$(PLASMOID_ID)'. Run 'make reload' to load it now (restarts plasmashell),"
	@echo "or log out and back in."

reload:
	$(SESSION) reload "$(QML_DIR)" "$(ENV_FILE)"

uninstall:
	rm -rf "$(PLASMOID_DIR)" "$(QML_MODULE_DIR)"
	-rmdir "$(QML_DIR)/plasmakey" 2>/dev/null
	rm -f "$(PREFIX)/bin/plasmakey-diag" "$(ENV_FILE)"
	@echo
	@echo "Removed. Run 'make reload' (or log out and back in) to drop it from the running panel."

diag: build
	$(BUILD_DIR)/bin/plasmakey-diag

clean:
	rm -rf $(BUILD_DIR)
