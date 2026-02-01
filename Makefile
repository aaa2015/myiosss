# ss - Socket Statistics for Apple platforms (macOS/iOS)
# Makefile for building on macOS and cross-compiling for iOS

# Compiler settings
CC = clang
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = 

# Source files
SRCDIR = src
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/sockets.c \
          $(SRCDIR)/output.c

HEADERS = $(SRCDIR)/ss.h

# Output binary
TARGET = ss

# Build directory
BUILDDIR = build

# macOS SDK (auto-detect)
MACOS_SDK := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
MACOS_MIN_VERSION = 10.15

# iOS SDK paths
IOS_SDK := $(shell xcrun --sdk iphoneos --show-sdk-path 2>/dev/null)
IOS_SIM_SDK := $(shell xcrun --sdk iphonesimulator --show-sdk-path 2>/dev/null)
IOS_MIN_VERSION = 13.0

# Default target: build for macOS
.PHONY: all
all: macos

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Build for macOS (current architecture)
.PHONY: macos
macos: $(BUILDDIR)
	$(CC) $(CFLAGS) \
		-mmacosx-version-min=$(MACOS_MIN_VERSION) \
		$(SOURCES) \
		$(LDFLAGS) \
		-o $(BUILDDIR)/$(TARGET)
	@echo "Built: $(BUILDDIR)/$(TARGET)"

# Build for macOS (Universal Binary - arm64 + x86_64)
.PHONY: macos-universal
macos-universal: $(BUILDDIR)
	$(CC) $(CFLAGS) -arch arm64 -arch x86_64 \
		-mmacosx-version-min=$(MACOS_MIN_VERSION) \
		$(SOURCES) \
		$(LDFLAGS) \
		-o $(BUILDDIR)/$(TARGET)-universal
	@echo "Built: $(BUILDDIR)/$(TARGET)-universal"

# Build for iOS (arm64)
.PHONY: ios
ios: $(BUILDDIR)
	@if [ -z "$(IOS_SDK)" ]; then \
		echo "Error: iOS SDK not found. Install Xcode and command line tools."; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -arch arm64 \
		-isysroot $(IOS_SDK) \
		-miphoneos-version-min=$(IOS_MIN_VERSION) \
		-DIOS_BUILD=1 \
		$(SOURCES) \
		$(LDFLAGS) \
		-o $(BUILDDIR)/$(TARGET)-ios
	@echo "Built: $(BUILDDIR)/$(TARGET)-ios"
	@echo "Note: For jailbroken devices, copy to /usr/local/bin/"

# Build for iOS Simulator (x86_64 + arm64)
.PHONY: ios-sim
ios-sim: $(BUILDDIR)
	@if [ -z "$(IOS_SIM_SDK)" ]; then \
		echo "Error: iOS Simulator SDK not found."; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -arch x86_64 -arch arm64 \
		-isysroot $(IOS_SIM_SDK) \
		-mios-simulator-version-min=$(IOS_MIN_VERSION) \
		-DIOS_BUILD=1 \
		$(SOURCES) \
		$(LDFLAGS) \
		-o $(BUILDDIR)/$(TARGET)-sim
	@echo "Built: $(BUILDDIR)/$(TARGET)-sim"

# Build all targets
.PHONY: build-all
build-all: macos macos-universal ios

# Install (macOS only)
.PHONY: install
install: macos
	@echo "Installing to /usr/local/bin/ss"
	@sudo cp $(BUILDDIR)/$(TARGET) /usr/local/bin/$(TARGET)
	@sudo chmod 755 /usr/local/bin/$(TARGET)
	@echo "Installed successfully"

# Uninstall
.PHONY: uninstall
uninstall:
	@sudo rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstalled"

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILDDIR)
	rm -f $(SRCDIR)/*.o
	@echo "Cleaned"

# Debug build (macOS)
.PHONY: debug
debug: CFLAGS += -g -DDEBUG -O0
debug: $(BUILDDIR)
	$(CC) $(CFLAGS) \
		-mmacosx-version-min=$(MACOS_MIN_VERSION) \
		$(SOURCES) \
		$(LDFLAGS) \
		-o $(BUILDDIR)/$(TARGET)-debug
	@echo "Built debug: $(BUILDDIR)/$(TARGET)-debug"

# Run tests
.PHONY: test
test: macos
	@echo "Running basic tests..."
	@echo "Test 1: Help output"
	@$(BUILDDIR)/$(TARGET) -h > /dev/null && echo "  PASS: -h" || echo "  FAIL: -h"
	@echo "Test 2: Version output"
	@$(BUILDDIR)/$(TARGET) -V > /dev/null && echo "  PASS: -V" || echo "  FAIL: -V"
	@echo "Test 3: TCP listing"
	@$(BUILDDIR)/$(TARGET) -t > /dev/null && echo "  PASS: -t" || echo "  FAIL: -t"
	@echo "Test 4: UDP listing"
	@$(BUILDDIR)/$(TARGET) -u > /dev/null && echo "  PASS: -u" || echo "  FAIL: -u"
	@echo "Test 5: Summary"
	@$(BUILDDIR)/$(TARGET) -s > /dev/null && echo "  PASS: -s" || echo "  FAIL: -s"
	@echo "Test 6: Listening sockets"
	@$(BUILDDIR)/$(TARGET) -tuln > /dev/null && echo "  PASS: -tuln" || echo "  FAIL: -tuln"
	@echo "Tests complete"

# Build ss_proc for iOS
.PHONY: ios-proc
ios-proc: $(BUILDDIR)
	@if [ -z "$(IOS_SDK)" ]; then \
		echo "Error: iOS SDK not found."; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -arch arm64 \
		-isysroot $(IOS_SDK) \
		-miphoneos-version-min=$(IOS_MIN_VERSION) \
		$(SRCDIR)/ss_proc.c \
		-o $(BUILDDIR)/ss_proc
	@echo "Built: $(BUILDDIR)/ss_proc"

# Create Debian package for iOS (jailbreak)
VERSION = 1.0.0
PACKAGE_ID = com.fangqingyuan.ss

.PHONY: deb
deb: ios-proc
	@echo "Creating .deb package for iOS..."
	@rm -rf $(BUILDDIR)/deb
	@mkdir -p $(BUILDDIR)/deb/DEBIAN
	@mkdir -p $(BUILDDIR)/deb/usr/local/bin
	@cp ss-ios.sh $(BUILDDIR)/deb/usr/local/bin/ss
	@cp $(BUILDDIR)/ss_proc $(BUILDDIR)/deb/usr/local/bin/ss_proc
	@cp ss.entitlements $(BUILDDIR)/deb/usr/local/bin/
	@chmod 755 $(BUILDDIR)/deb/usr/local/bin/ss
	@chmod 755 $(BUILDDIR)/deb/usr/local/bin/ss_proc
	@echo "Package: $(PACKAGE_ID)" > $(BUILDDIR)/deb/DEBIAN/control
	@echo "Name: SS (Socket Statistics)" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "Version: $(VERSION)" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "Architecture: iphoneos-arm" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "Description: Linux ss command for iOS - display socket statistics with process info" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "Maintainer: fangqingyuan <fangqingyuan@example.com>" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "Author: fangqingyuan" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "Section: Utilities" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "Depends: firmware (>= 13.0)" >> $(BUILDDIR)/deb/DEBIAN/control
	@echo "#!/bin/bash" > $(BUILDDIR)/deb/DEBIAN/postinst
	@echo "ldid -S/usr/local/bin/ss.entitlements /usr/local/bin/ss_proc 2>/dev/null || true" >> $(BUILDDIR)/deb/DEBIAN/postinst
	@chmod 755 $(BUILDDIR)/deb/DEBIAN/postinst
	@dpkg-deb -Zgzip --build $(BUILDDIR)/deb $(BUILDDIR)/ss-apple_$(VERSION)_iphoneos-arm.deb 2>/dev/null || \
		(echo "dpkg-deb not found, creating tar.gz instead" && cd $(BUILDDIR) && tar czf ss-apple_$(VERSION)_iphoneos-arm.tar.gz -C deb .)
	@echo "Package created: $(BUILDDIR)/ss-apple_$(VERSION)_iphoneos-arm.deb"

# Show help
.PHONY: help
help:
	@echo "ss - Socket Statistics for Apple platforms"
	@echo ""
	@echo "Build targets:"
	@echo "  make            - Build for macOS (current arch)"
	@echo "  make macos      - Build for macOS (current arch)"
	@echo "  make macos-universal - Build Universal Binary (arm64 + x86_64)"
	@echo "  make ios        - Build for iOS (arm64, jailbroken devices)"
	@echo "  make ios-sim    - Build for iOS Simulator"
	@echo "  make build-all  - Build all targets"
	@echo "  make debug      - Build with debug symbols"
	@echo ""
	@echo "Install/Uninstall:"
	@echo "  make install    - Install to /usr/local/bin (requires sudo)"
	@echo "  make uninstall  - Remove from /usr/local/bin"
	@echo ""
	@echo "Package:"
	@echo "  make deb        - Create .deb package for iOS jailbreak"
	@echo ""
	@echo "Other:"
	@echo "  make test       - Run basic tests"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make help       - Show this help"
