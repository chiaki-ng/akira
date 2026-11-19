# Dev workflow for building and deploying akira.nro to Nintendo Switch
#
# Usage:
#   make build                          Build the NRO
#   make deploy SWITCH_IP=192.168.x.x   Build and deploy to Switch
#   make rebuild                         Force full rebuild (clean libs + rebuild docker image)
#   make shell                           Open shell in build container
#   make clean                           Remove the build directory
#   make build MUTE_CHIAKI=true          Build with chiaki library logs muted
#   make crash SWITCH_IP=192.168.x.x     Pull + symbolicate the latest crash report
#   make crash LOG=path/to/report.log    Symbolicate a crash report already on disk
#   make test                            Run the host-side unit tests
#   make devkit-image OUNCE_DEVKIT=dir   Build devkita64-ounce from an extracted ounce devkit
#   make fetch-devkit OUNCE_DEVKIT_URL=u Download the pinned ounce devkit and check its sha256
#   make ounce-deps                      Build ../native-deps-ounce inside devkita64-ounce

.PHONY: help build deploy crash test rebuild shell clean docker-image devkit-image fetch-devkit devkit-pin release-notes ounce-deps ounce-deps-check image-tag submodules backup platform

PLATFORM ?=

OUNCE_DEVKIT_VERSION := 20261113f
OUNCE_DEVKIT_SHA256  := 6134335c80034aeaa2a769a64dc6d7266b19adcd270a8522077547512a113146
OUNCE_DEVKIT_URL     ?= https://github.com/ouncebrew/ounce-devkit/releases/download/devkit-$(OUNCE_DEVKIT_VERSION)/ounce-devkit-$(OUNCE_DEVKIT_VERSION).tar.xz
OUNCE_DEVKIT ?= $(if $(wildcard $(CURDIR)/../ounce-devkit/$(OUNCE_DEVKIT_VERSION)/ounce-devkit-version.txt),$(abspath $(CURDIR)/../ounce-devkit/$(OUNCE_DEVKIT_VERSION)),$(CURDIR)/.cache/ounce-devkit/$(OUNCE_DEVKIT_VERSION))
DEVKIT_REV   := $(shell shasum -a 256 "$(CURDIR)/scripts/devkit/Dockerfile" 2>/dev/null | cut -c1-8)
DEVKIT_IMAGE ?= devkita64-ounce:$(OUNCE_DEVKIT_VERSION)-$(DEVKIT_REV)
NATIVE_DEPS  ?= $(CURDIR)/library/native-deps
OUNCE_DEPS   ?= $(NATIVE_DEPS)/out/aarch64/pool
OUNCE_DEPS_PACKAGES := make cmake ninja-build pkg-config python3 perl patch git ca-certificates curl jq rsync xz-utils bzip2 zstd
ifeq ($(PLATFORM),ounce)
IMAGE_HASH   := $(shell { echo "$(DEVKIT_IMAGE)"; cat "$(CURDIR)/docker/ounce/Dockerfile" "$(CURDIR)/.dockerignore" "$(CURDIR)/scripts/docker/install-ounce-deps.sh" \
                          $(sort $(wildcard $(OUNCE_DEPS)/*/BUILD-INFO)); } 2>/dev/null | shasum -a 256 | cut -c1-12)
DOCKER_BUILD  = docker build -f "$(CURDIR)/docker/ounce/Dockerfile" --build-context ounce-deps="$(OUNCE_DEPS)" --build-arg BASE="$(DEVKIT_IMAGE)"
IMAGE_DEPS   := devkit-image ounce-deps-check
OVERRIDES    := CORE BOREALIS WIREGUARD LWIP
SUBMODULE_FLAGS :=
LIBRARY_SUFFIX := -ounce
TEST_PLATFORM_SRC :=
else
IMAGE_HASH   := $(shell { cat "$(CURDIR)/docker/nx/Dockerfile" "$(CURDIR)/docker/nx/Dockerfile.dockerignore" \
                          "$(CURDIR)/scripts/docker/build-uam.sh" "$(CURDIR)/scripts/docker/install-nx-deps.sh"; \
                          git -C "$(CURDIR)/library/uam" rev-parse HEAD; \
                          git -C "$(CURDIR)/library/uam" diff HEAD; } 2>/dev/null | shasum -a 256 | cut -c1-12)
DOCKER_BUILD  = docker build -f "$(CURDIR)/docker/nx/Dockerfile"
IMAGE_DEPS   :=
OVERRIDES    := CORE BOREALIS WIREGUARD LWIP UAM NX_DEPS CURL FFMPEG NGHTTP2 SIMDUTF
SUBMODULE_FLAGS := --recursive
LIBRARY_SUFFIX :=
TEST_PLATFORM_SRC := $(CURDIR)/source/platform/pads/extended/rumble_keys.cpp \
                     $(CURDIR)/source/platform/pads/extended/pad_output_state.cpp
endif
DOCKER_IMAGE ?= akira-builder:$(IMAGE_HASH)
NRO_FILE     := $(CURDIR)/build/$(PLATFORM)/akira.nro
ELF_FILE     := build/$(PLATFORM)/akira.elf
FTP_PORT     ?= 5000
MUTE_CHIAKI  ?= false
CLOUD ?= false
PS5_CLOUD_LAUNCH ?= false
PS4_CLOUD_LAUNCH ?= false
SWITCH_IP    ?=
BUILD_CHANNEL      ?= dev
VERSION_PRERELEASE ?=
DOCKER_TTY   := $(shell [ -t 0 ] && echo -it || echo -i)

override_path = $(realpath $(patsubst ~/%,$(HOME)/%,$(WITH_$(1))))
override_git = $(filter-out $(1)/%,$(shell test -f "$(1)/.git" && git -C "$(1)" rev-parse --path-format=absolute --git-common-dir 2>/dev/null))
$(foreach lib,$(OVERRIDES),$(if $(WITH_$(lib)),$(if $(call override_path,$(lib)),,$(error WITH_$(lib)=$(WITH_$(lib)) does not exist))))
OVERRIDE_DOCKER_ARGS := $(foreach lib,$(OVERRIDES),$(if $(WITH_$(lib)), \
    -v "$(call override_path,$(lib)):$(call override_path,$(lib)):ro" \
    $(foreach git,$(call override_git,$(call override_path,$(lib))),-v "$(git):$(git):ro") \
    -e WITH_$(lib)="$(call override_path,$(lib))"))

# Linked worktrees store .git as a file pointing at the main repository's
# metadata. Mount that metadata read-only at its original absolute path so
# Git/version stamping works in the build container without touching master.
ifeq ($(shell test -f "$(CURDIR)/.git" && echo yes),yes)
GIT_COMMON_DIR := $(shell git rev-parse --path-format=absolute --git-common-dir)
WORKTREE_DOCKER_MOUNTS := -v "$(GIT_COMMON_DIR):$(GIT_COMMON_DIR):ro"
endif

DOCKER_RUN := docker run --rm -v "$(CURDIR):$(CURDIR)" $(WORKTREE_DOCKER_MOUNTS) $(OVERRIDE_DOCKER_ARGS) -w "$(CURDIR)"

# The psn package is plain C++ over json-c with no libnx or borealis dependency, so it
# builds and runs natively. Everything else in the app needs the Switch toolchain.
TEST_BIN     := $(CURDIR)/build/tests/$(PLATFORM)/psn_tests
TEST_SRC     := $(wildcard $(CURDIR)/tests/*.cpp) \
                $(wildcard $(CURDIR)/tests/$(PLATFORM)/*.cpp) \
                $(CURDIR)/source/cloud/models.cpp \
                $(CURDIR)/source/cloud/library_listing.cpp \
                $(CURDIR)/source/cloud/game_actions.cpp \
                $(CURDIR)/source/cloud/launch_checks.cpp \
                $(CURDIR)/source/cloud/start_steps.cpp \
                $(CURDIR)/source/cloud/fetch_progress.cpp \
                $(CURDIR)/source/consoles/pair_crypto.cpp \
                $(CURDIR)/source/consoles/console_address.cpp \
                $(CURDIR)/source/consoles/console_entry.cpp \
                $(CURDIR)/source/consoles/console_list.cpp \
                $(CURDIR)/source/consoles/console_pin.cpp \
                $(CURDIR)/source/consoles/takion_pin_choice.cpp \
                $(CURDIR)/source/consoles/psn_identity.cpp \
                $(CURDIR)/source/log/log.cpp \
                $(CURDIR)/source/log/core_log.cpp \
                $(CURDIR)/source/log/log_tail.cpp \
                $(CURDIR)/source/play/connect_failure.cpp \
                $(CURDIR)/source/play/connect_flow.cpp \
                $(CURDIR)/source/settings/settings_store.cpp \
                $(CURDIR)/source/settings/developer_settings.cpp \
                $(CURDIR)/source/play/stream_settings.cpp \
                $(CURDIR)/source/play/controller_settings.cpp \
                $(CURDIR)/source/cloud/cloud_settings.cpp \
                $(CURDIR)/source/cloud/saved_datacenters.cpp \
                $(CURDIR)/source/network/network_settings.cpp \
                $(CURDIR)/source/network/custom_dns.cpp \
                $(CURDIR)/source/accounts/account_settings.cpp \
                $(CURDIR)/source/accounts/sign_in_store.cpp \
                $(CURDIR)/source/accounts/profile_tables.cpp \
                $(CURDIR)/source/accounts/profile_choices.cpp \
                $(CURDIR)/source/consoles/console_settings.cpp \
                $(CURDIR)/source/consoles/pairing_store.cpp \
                $(CURDIR)/source/app/app_settings.cpp \
                $(CURDIR)/source/network/dns_client.cpp \
                $(CURDIR)/source/trophies/icon_cache.cpp \
                $(CURDIR)/source/platform/video/viddec/h264_sps.cpp \
                $(TEST_PLATFORM_SRC)
CORE_DIR     := $(or $(call override_path,CORE),$(CURDIR)/library/libchiaki-core$(LIBRARY_SUFFIX))
CORE_TEST_SRC := $(CORE_DIR)/src/platform/crypto/provider.cpp \
                 $(CORE_DIR)/src/platform/crypto/evp/primitives.cpp \
                 $(CORE_DIR)/src/platform/crypto/openssl/provider.cpp \
                 $(CORE_DIR)/src/platform/crypto/openssl/pairing_primitives.cpp \
                 $(CORE_DIR)/src/platform/random/darwin.cpp \
                 $(CORE_DIR)/src/platform/memory/secure_zero_darwin.cpp \
                 $(CORE_DIR)/src/platform/net/address.cpp \
                 $(CORE_DIR)/src/platform/net/posix.cpp \
                 $(CORE_DIR)/src/core/utils/text/ascii.cpp \
                 $(CORE_DIR)/src/core/users/model/account_id.cpp \
                 $(CORE_DIR)/src/core/users/model/account_context.cpp \
                 $(CORE_DIR)/src/core/users/model/device_id.cpp \
                 $(CORE_DIR)/src/core/consoles/model/console/hardware_id.cpp \
                 $(CORE_DIR)/src/core/consoles/model/console/console_name.cpp \
                 $(CORE_DIR)/src/core/consoles/model/console/device_uid.cpp \
                 $(CORE_DIR)/src/core/consoles/model/console/host.cpp \
                 $(CORE_DIR)/src/core/consoles/model/pairing/pairing.cpp \
                 $(CORE_DIR)/src/core/play/model/outcome.cpp
JSONC_PREFIX ?= $(shell pkg-config --variable=prefix json-c 2>/dev/null || echo /opt/homebrew)
SIMDUTF_PREFIX ?= $(shell pkg-config --variable=prefix simdutf 2>/dev/null || echo /opt/homebrew)
OPENSSL_PREFIX ?= $(shell pkg-config --variable=prefix openssl 2>/dev/null || echo /opt/homebrew/opt/openssl@3)

# Colors
GREEN  := \033[0;32m
YELLOW := \033[1;33m
RED    := \033[0;31m
NC     := \033[0m

%-nx:
	@$(MAKE) --no-print-directory $* PLATFORM=nx

%-ounce:
	@$(MAKE) --no-print-directory $* PLATFORM=ounce

platform:
	@case "$(PLATFORM)" in \
		nx|ounce) ;; \
		*) printf "$(RED)[x]$(NC) Pick a console: make <target>-nx (Switch 1) or make <target>-ounce (Switch 2)\n"; exit 1 ;; \
	esac

help:
	@echo "Usage: make <target>-nx | <target>-ounce [SWITCH_IP=<ip>] [MUTE_CHIAKI=true]"
	@echo ""
	@echo "Targets (-nx builds for Switch 1, -ounce for Switch 2):"
	@echo "  build-nx, build-ounce            Build the NRO into build/<platform>"
	@echo "  deploy-nx, deploy-ounce          Build and deploy to Switch (requires SWITCH_IP)"
	@echo "  rebuild-nx, rebuild-ounce        Rebuild the docker image and the NRO from scratch"
	@echo "  shell-nx, shell-ounce            Open shell in build container"
	@echo "  crash-nx, crash-ounce            Symbolicate the latest Switch crash report (SWITCH_IP or LOG)"
	@echo "  test-nx, test-ounce              Run the host-side unit tests"
	@echo "  image-tag-nx, image-tag-ounce    Print the builder image tag for this checkout"
	@echo "  backup       Pull akira.toml off the Switch over sys-ftpd (SWITCH_IP)"
	@echo "  clean        Remove the build directory"
	@echo "  help         Show this help"
	@echo ""
	@echo "Environment:"
	@echo "  SWITCH_IP      IP address of Nintendo Switch"
	@echo "  FTP_PORT       sys-ftpd port for 'make crash' (default 5000)"
	@echo "  LOG            Local crash report path for 'make crash'"
	@echo "  MUTE_CHIAKI    Set to 'true' to mute chiaki library logs"
	@echo "  CLOUD          Set to 'true' to build cloud streaming; left out by default"
	@echo "  PS5_CLOUD_LAUNCH  Set to 'true' to allow PS Cloud launches with CLOUD=true; refused by default"
	@echo "  PS4_CLOUD_LAUNCH  Set to 'true' to allow PS Now launches with CLOUD=true; refused by default"
	@echo "  BUILD_CHANNEL  Version channel stamped into the build (default dev)"
	@echo "  VERSION_PRERELEASE  Prerelease suffix stamped into the build"
	@echo ""
	@echo "Library overrides (build against a local copy instead of the pinned one):"
	@echo "  WITH_CORE=<dir>       libchiaki-core source tree"
	@echo "  WITH_BOREALIS=<dir>   borealis source tree"
	@echo "  WITH_WIREGUARD=<dir>  switch-wireguard source tree"
	@echo "  WITH_LWIP=<dir>       lwIP source tree"
	@echo "  WITH_UAM=<dir>        uam source tree, rebuilt into the container"
	@echo "  WITH_NX_DEPS=<dir>    a native-deps pool (out/aarch64/pool) replacing the release"
	@echo "  WITH_CURL=<prefix>    an installed curl prefix (also FFMPEG, NGHTTP2, SIMDUTF)"
	@echo ""
	@echo "  make build WITH_CORE=~/Documents/projects/libchiaki-integration"
	@echo "  make build WITH_NX_DEPS=~/Documents/projects/native-deps/out/aarch64/pool"
	@echo ""
	@echo "On your Switch:"
	@echo "  1. Open Homebrew Menu"
	@echo "  2. Press Y for NetLoader mode"
	@echo "  3. Note the IP address shown"

crash: platform
	@if [ ! -f "$(ELF_FILE)" ]; then \
		printf "$(RED)[x]$(NC) $(ELF_FILE) not found - run 'make build-$(PLATFORM)' first\n"; \
		exit 1; \
	fi
	@if [ -n "$(LOG)" ]; then \
		printf "$(GREEN)[*]$(NC) Symbolicating local report: $(LOG)\n"; \
		"$(CURDIR)/scripts/ns_debug.sh" local "$(ELF_FILE)" "$(LOG)"; \
	elif [ -n "$(SWITCH_IP)" ]; then \
		printf "$(GREEN)[*]$(NC) Pulling latest crash report from $(SWITCH_IP):$(FTP_PORT)\n"; \
		"$(CURDIR)/scripts/ns_debug.sh" "ftp://$(SWITCH_IP):$(FTP_PORT)" "$(ELF_FILE)"; \
	else \
		printf "$(YELLOW)[!]$(NC) Provide SWITCH_IP or LOG\n"; \
		echo ""; \
		echo "  make crash SWITCH_IP=192.168.1.5"; \
		echo "  make crash SWITCH_IP=192.168.1.5 FTP_PORT=5000"; \
		echo "  make crash LOG=01785002094_010015f005c8e000.log"; \
		echo ""; \
		echo "Requires sys-ftpd running on the Switch."; \
		exit 1; \
	fi

test: platform
	@if [ ! -f "$(JSONC_PREFIX)/include/json-c/json.h" ]; then \
		printf "$(RED)[x]$(NC) json-c headers not found under $(JSONC_PREFIX)\n"; \
		echo "    brew install json-c, or pass JSONC_PREFIX=<prefix>"; \
		exit 1; \
	fi
	@mkdir -p "$(CURDIR)/build/tests/$(PLATFORM)"
	@printf "$(GREEN)[*]$(NC) Building host tests...\n"
	@c++ -std=c++23 -g -O0 -Wall -Wextra -Wno-unused-parameter -DAKIRA_MUTE_CORE_LOGS=0 \
		-I"$(CURDIR)/include" -I"$(CURDIR)/tests" -I"$(JSONC_PREFIX)/include" -I"$(SIMDUTF_PREFIX)/include" \
		-I"$(CURDIR)/library/tomlplusplus/include" -I"$(CORE_DIR)/include" -I"$(CORE_DIR)" -I"$(OPENSSL_PREFIX)/include" \
		$(TEST_SRC) $(CORE_TEST_SRC) -L"$(JSONC_PREFIX)/lib" -ljson-c -L"$(SIMDUTF_PREFIX)/lib" -lsimdutf \
		-L"$(OPENSSL_PREFIX)/lib" -lcrypto -o "$(TEST_BIN)"
	@printf "$(GREEN)[*]$(NC) Running host tests...\n"
	@"$(TEST_BIN)"

submodules:
	@if [ ! -f "$(CURDIR)/library/borealis$(LIBRARY_SUFFIX)/README.md" ]; then \
		printf "$(GREEN)[*]$(NC) Initializing submodules...\n"; \
		git -C "$(CURDIR)" submodule update --init $(SUBMODULE_FLAGS); \
	fi

devkit-pin:
	@echo $(OUNCE_DEVKIT_VERSION)

fetch-devkit:
	@if [ ! -f "$(OUNCE_DEVKIT)/ounce-devkit-version.txt" ]; then \
		test -n "$(OUNCE_DEVKIT_URL)" || { printf "$(RED)[x]$(NC) No ounce devkit at $(OUNCE_DEVKIT): pass OUNCE_DEVKIT=<dir> or OUNCE_DEVKIT_URL=<url>\n"; exit 1; }; \
		printf "$(GREEN)[*]$(NC) Downloading ounce devkit $(OUNCE_DEVKIT_VERSION)...\n"; \
		mkdir -p "$(OUNCE_DEVKIT)"; \
		curl -fsSL --retry 3 -o "$(OUNCE_DEVKIT).tar.xz" "$(OUNCE_DEVKIT_URL)"; \
		echo "$(OUNCE_DEVKIT_SHA256)  $(OUNCE_DEVKIT).tar.xz" | shasum -a 256 -c - > /dev/null || { printf "$(RED)[x]$(NC) The ounce devkit does not match sha256 $(OUNCE_DEVKIT_SHA256)\n"; rm -f "$(OUNCE_DEVKIT).tar.xz"; exit 1; }; \
		tar -xJf "$(OUNCE_DEVKIT).tar.xz" -C "$(OUNCE_DEVKIT)"; \
		rm -f "$(OUNCE_DEVKIT).tar.xz"; \
	fi
	@grep -qx "ounce-devkit $(OUNCE_DEVKIT_VERSION)" "$(OUNCE_DEVKIT)/ounce-devkit-version.txt" || { printf "$(RED)[x]$(NC) $(OUNCE_DEVKIT) is not ounce devkit $(OUNCE_DEVKIT_VERSION)\n"; exit 1; }

devkit-image: fetch-devkit
	@docker image inspect $(DEVKIT_IMAGE) > /dev/null 2>&1 || { \
		printf "$(GREEN)[*]$(NC) Building $(DEVKIT_IMAGE) from $(OUNCE_DEVKIT)...\n"; \
		docker build --build-context ounce-devkit="$(OUNCE_DEVKIT)" --build-arg OUNCE_DEVKIT_VERSION="$(OUNCE_DEVKIT_VERSION)" \
			-t $(DEVKIT_IMAGE) "$(CURDIR)/scripts/devkit"; }

ounce-deps: devkit-image
	@printf "$(GREEN)[*]$(NC) Building $(NATIVE_DEPS) in $(DEVKIT_IMAGE)...\n"
	@docker run --rm -v "$(CURDIR):$(CURDIR)" -v "$(NATIVE_DEPS):$(NATIVE_DEPS)" -w "$(NATIVE_DEPS)" $(DEVKIT_IMAGE) bash -c \
		'apt-get update -qq && apt-get install -y -qq --no-install-recommends $(OUNCE_DEPS_PACKAGES) >/dev/null && \
		git config --global --add safe.directory "*" && make build PLATFORM=ounce ARCH=aarch64'

ounce-deps-check:
	@docker image inspect $(DOCKER_IMAGE) > /dev/null 2>&1 || test -d "$(OUNCE_DEPS)" || { printf "$(RED)[x]$(NC) No ounce deps at $(OUNCE_DEPS): run make ounce-deps\n"; exit 1; }

docker-image: platform submodules $(IMAGE_DEPS)
	@IMAGE_EXISTS=$$(docker image inspect $(DOCKER_IMAGE) > /dev/null 2>&1 && echo yes || echo no); \
	if [ "$$IMAGE_EXISTS" = "no" ]; then \
		printf "$(GREEN)[*]$(NC) Building Docker image...\n"; \
		$(DOCKER_BUILD) -t $(DOCKER_IMAGE) "$(CURDIR)"; \
	fi

image-tag: platform
	@echo $(DOCKER_IMAGE)

release-notes: platform
	@"$(CURDIR)/scripts/release-notes.sh" "$(OUNCE_DEVKIT)" "$(DOCKER_IMAGE)"

build: platform docker-image
	@printf "$(GREEN)[*]$(NC) Building $(PLATFORM) with $(DOCKER_IMAGE)...\n"
	@$(DOCKER_RUN) \
		-e AKIRA_PLATFORM="$(PLATFORM)" \
		-e AKIRA_SOURCE="$(CURDIR)" \
		-e AKIRA_IMAGE="$(DOCKER_IMAGE)" \
		-e MUTE_CHIAKI="$(MUTE_CHIAKI)" \
		-e CLOUD="$(CLOUD)" \
		-e PS5_CLOUD_LAUNCH="$(PS5_CLOUD_LAUNCH)" \
		-e PS4_CLOUD_LAUNCH="$(PS4_CLOUD_LAUNCH)" \
		-e BUILD_CHANNEL="$(BUILD_CHANNEL)" \
		-e VERSION_PRERELEASE="$(VERSION_PRERELEASE)" \
		$(DOCKER_IMAGE) \
		bash "$(CURDIR)/scripts/build-docker.sh"
	@if [ ! -f "$(NRO_FILE)" ]; then \
		printf "$(RED)[x]$(NC) Build failed - NRO not found at $(NRO_FILE)\n"; \
		exit 1; \
	fi
	@printf "$(GREEN)[*]$(NC) Build successful: $(NRO_FILE)\n"

backup:
	@if [ -z "$(SWITCH_IP)" ]; then \
		printf "$(YELLOW)[!]$(NC) No SWITCH_IP provided\n"; \
		echo "  make backup SWITCH_IP=<ip> [FTP_PORT=5000]"; \
		echo "  Requires sys-ftpd running on the Switch."; \
		exit 1; \
	fi
	@mkdir -p "$(CURDIR)/backups"
	@stamp=$$(date +%Y%m%d-%H%M%S); \
	dest="$(CURDIR)/backups/akira-$(SWITCH_IP)-$$stamp.toml"; \
	printf "$(GREEN)[*]$(NC) Pulling /switch/akira/akira.toml from $(SWITCH_IP):$(FTP_PORT)\n"; \
	if curl -fsS "ftp://$(SWITCH_IP):$(FTP_PORT)/switch/akira/akira.toml" -o "$$dest"; then \
		printf "$(GREEN)[*]$(NC) Saved $$dest\n"; \
	else \
		printf "$(RED)[x]$(NC) Backup failed - is sys-ftpd running on $(SWITCH_IP):$(FTP_PORT)?\n"; \
		rm -f "$$dest"; \
		exit 1; \
	fi

deploy: platform build
	@if [ -z "$(SWITCH_IP)" ]; then \
		printf "$(YELLOW)[!]$(NC) No SWITCH_IP provided - skipping deployment\n"; \
		echo ""; \
		echo "To deploy, run: make deploy-$(PLATFORM) SWITCH_IP=<ip>"; \
		exit 0; \
	fi
	@pkill -f nxlink 2>/dev/null || true
	@mkdir -p "$(CURDIR)/logs"
	$(eval LOG_FILE := $(CURDIR)/logs/$(shell date +%d%m%y%H%M%S).log)
	@printf "$(GREEN)[*]$(NC) Deploying to Switch at $(SWITCH_IP)...\n"
	@printf "$(GREEN)[*]$(NC) Logging to: $(LOG_FILE)\n"
	@printf "$(GREEN)[*]$(NC) Press Ctrl+C to stop receiving logs\n"
	@$(DOCKER_RUN) $(DOCKER_TTY) --init \
		--network host \
		$(DOCKER_IMAGE) \
		nxlink -s -a "$(SWITCH_IP)" "$(NRO_FILE)" 2>&1 | tee "$(LOG_FILE)"
	@printf "$(GREEN)[*]$(NC) Done\n"

rebuild: platform clean submodules $(IMAGE_DEPS)
	@printf "$(GREEN)[*]$(NC) Rebuilding Docker image...\n"
	@$(DOCKER_BUILD) --no-cache -t $(DOCKER_IMAGE) "$(CURDIR)"
	@$(MAKE) --no-print-directory build PLATFORM=$(PLATFORM)

shell: platform docker-image
	@printf "$(GREEN)[*]$(NC) Opening shell in build container...\n"
	@$(DOCKER_RUN) -it $(DOCKER_IMAGE) bash

clean:
	@printf "$(GREEN)[*]$(NC) Removing $(CURDIR)/build...\n"
	@rm -rf "$(CURDIR)/build"
