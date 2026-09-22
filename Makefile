# VoX DoN 3 Host Installer - Native PS5 ELF Makefile

PYTHON := python3
CC     := $(PS5_PAYLOAD_SDK)/bin/prospero-clang
STRIP  := $(PS5_PAYLOAD_SDK)/bin/prospero-strip

ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

SDK      := $(PS5_PAYLOAD_SDK)
TARGET   := $(SDK)/target
INCLUDES := -Iinclude -I$(TARGET)/include
LIBS     := -L$(TARGET)/lib -lpthread \
            -lSceNetCtl -lSceUserService -lSceSystemService \
            -lSceAppInstUtil -lSceNotification

SRCS := src/main.c src/http_server.c src/app_installer.c \
        src/notification.c src/ps5_launcher.c src/inflate.c
ELF := VoX DoN 3.elf

FILE_REGISTRY_H := include/file_registry.h
FILE_REGISTRY_C := include/file_registry.c
FILE_REGISTRY_STAMP := include/.file_registry.stamp

VERSION_HEADER := include/voxd_version.h

FRONTEND_INSTALLER_PAGE := frontend/installer-page
FRONTEND_POINTER := frontend/pointer
FRONTEND_STAGE := frontend/dist
FRONTEND_FILES := $(shell find $(FRONTEND_INSTALLER_PAGE) $(FRONTEND_POINTER) -type f 2>/dev/null)

# The VoX DoN 3 Host — its files are copied verbatim into the app dir.
VOXHOST := host
VOXHOST_APPCACHE := $(VOXHOST)/cache.appcache

CFLAGS  := -Os -Wall -Werror -ffunction-sections -fdata-sections $(INCLUDES)
LDFLAGS := -Wl,--gc-sections

# Build type: dev (default, timestamp in version) or stable (base version only)
BUILD_TYPE ?= dev

all: $(ELF)

.PHONY: version print-version
version:
	$(PYTHON) tools/gen_version.py

print-version:
	@$(PYTHON) tools/gen_version.py --print

$(FILE_REGISTRY_H) $(FILE_REGISTRY_C): $(FILE_REGISTRY_STAMP)

$(FILE_REGISTRY_STAMP): $(FRONTEND_FILES) version $(VOXHOST_APPCACHE)
	@echo "Staging frontend into $(FRONTEND_STAGE)/..."
	@export BUILD_TYPE=$(BUILD_TYPE); \
	V=$$($(PYTHON) tools/gen_version.py --print); \
	rm -rf $(FRONTEND_STAGE) && \
	mkdir -p $(FRONTEND_STAGE)/app/$$V && \
	cp -R $(FRONTEND_INSTALLER_PAGE)/. $(FRONTEND_STAGE)/ && \
	cp -R $(FRONTEND_POINTER)/. $(FRONTEND_STAGE)/app/ && \
	cp -R $(VOXHOST)/. $(FRONTEND_STAGE)/app/$$V/ && \
	rm -rf $(FRONTEND_STAGE)/app/$$V/.git && \
	rm -f $(FRONTEND_STAGE)/app/$$V/cache.appcache
	@echo "Generating file registry from $(FRONTEND_STAGE)/..."
	$(PYTHON) tools/gen_file_registry.py $(FRONTEND_STAGE) $(VOXHOST_APPCACHE) $(FILE_REGISTRY_H) $(FILE_REGISTRY_C)
	@touch $(FILE_REGISTRY_STAMP)

$(ELF): $(FILE_REGISTRY_H) $(FILE_REGISTRY_C) $(SRCS) assets/icon0.png assets/param.json
	@echo "Building $(ELF)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o "$(ELF)" $(SRCS) $(FILE_REGISTRY_C) $(LIBS)
	@echo "Stripping $(ELF)..."
	$(STRIP) "$(ELF)"

clean:
	rm -rf $(FRONTEND_STAGE)
	rm -f "$(ELF)" $(FILE_REGISTRY_H) $(FILE_REGISTRY_C) $(FILE_REGISTRY_STAMP)
	rm -f $(VERSION_HEADER)

.PHONY: all version print-version clean