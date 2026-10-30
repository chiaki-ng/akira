# Dev workflow for building and deploying akira.nro to Nintendo Switch
#
# Usage:
#   make build                          Build the NRO
#   make deploy SWITCH_IP=192.168.x.x   Build and deploy to Switch
#   make rebuild                         Force full rebuild (clean libs + rebuild docker image)
#   make shell                           Open shell in build container
#   make clean-libs                      Clean library build artifacts
#   make build MUTE_CHIAKI=true          Build with chiaki library logs muted
#   make crash SWITCH_IP=192.168.x.x     Pull + symbolicate the latest crash report
#   make crash LOG=path/to/report.log    Symbolicate a crash report already on disk
#   make test                            Run the host-side unit tests

.PHONY: help build deploy crash test rebuild shell clean-libs docker-image submodules backup

# A distinct tag prevents an existing builder with the broad FFmpeg codec set
# from silently producing a larger binary after this Dockerfile change.
DOCKER_IMAGE ?= akira-builder-lean-ffmpeg
NRO_FILE     := $(CURDIR)/build/akira.nro
ELF_FILE     := build/akira.elf
FTP_PORT     ?= 5000
MUTE_CHIAKI  ?= false
SWITCH_IP    ?=

# Linked worktrees store .git as a file pointing at the main repository's
# metadata. Mount that metadata read-only at its original absolute path so
# Git/version stamping works in the build container without touching master.
ifeq ($(shell test -f "$(CURDIR)/.git" && echo yes),yes)
GIT_COMMON_DIR := $(shell git rev-parse --path-format=absolute --git-common-dir)
WORKTREE_DOCKER_MOUNTS := -v "$(CURDIR):$(CURDIR)" -v "$(GIT_COMMON_DIR):$(GIT_COMMON_DIR):ro"
endif

# The psn package is plain C++ over json-c with no libnx or borealis dependency, so it
# builds and runs natively. Everything else in the app needs the Switch toolchain.
TEST_BIN     := $(CURDIR)/build/tests/psn_tests
TEST_SRC     := $(wildcard $(CURDIR)/tests/*.cpp) \
                $(CURDIR)/source/cloud/models.cpp \
                $(CURDIR)/source/psn/auth_bootstrap.cpp \
                $(CURDIR)/source/psn/models.cpp \
                $(CURDIR)/source/psn/client.cpp \
                $(CURDIR)/source/psn/log.cpp \
                $(CURDIR)/source/core/pair_crypto.cpp \
                $(CURDIR)/source/input/rumble_profile.cpp \
                $(CURDIR)/source/input/pad_output_state.cpp \
                $(CURDIR)/source/stream/audio_ring_buffer.cpp
PAIR_UECC_SRC := $(CURDIR)/source/core/pair/microecc/uECC.c
PAIR_UECC_OBJ := $(CURDIR)/build/tests/uECC.o
JSONC_PREFIX ?= $(shell pkg-config --variable=prefix json-c 2>/dev/null || echo /opt/homebrew)

# Colors
GREEN  := \033[0;32m
YELLOW := \033[1;33m
RED    := \033[0;31m
NC     := \033[0m

help:
	@echo "Usage: make <target> [SWITCH_IP=<ip>] [MUTE_CHIAKI=true]"
	@echo ""
	@echo "Targets:"
	@echo "  build        Build the NRO"
	@echo "  deploy       Build and deploy to Switch (requires SWITCH_IP)"
	@echo "  rebuild      Force full rebuild (clean libs + rebuild docker image)"
	@echo "  shell        Open shell in build container"
	@echo "  crash        Symbolicate the latest Switch crash report (SWITCH_IP or LOG)"
	@echo "  backup       Pull akira.toml off the Switch over sys-ftpd (SWITCH_IP)"
	@echo "  test         Run the host-side unit tests for the psn package"
	@echo "  clean-libs   Clean library build artifacts"
	@echo "  help         Show this help"
	@echo ""
	@echo "Environment:"
	@echo "  SWITCH_IP      IP address of Nintendo Switch"
	@echo "  FTP_PORT       sys-ftpd port for 'make crash' (default 5000)"
	@echo "  LOG            Local crash report path for 'make crash'"
	@echo "  MUTE_CHIAKI    Set to 'true' to mute chiaki library logs"
	@echo ""
	@echo "On your Switch:"
	@echo "  1. Open Homebrew Menu"
	@echo "  2. Press Y for NetLoader mode"
	@echo "  3. Note the IP address shown"

crash:
	@if [ ! -f "$(ELF_FILE)" ]; then \
		printf "$(RED)[x]$(NC) $(ELF_FILE) not found - run 'make build' first\n"; \
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

test:
	@if [ ! -f "$(JSONC_PREFIX)/include/json-c/json.h" ]; then \
		printf "$(RED)[x]$(NC) json-c headers not found under $(JSONC_PREFIX)\n"; \
		echo "    brew install json-c, or pass JSONC_PREFIX=<prefix>"; \
		exit 1; \
	fi
	@mkdir -p "$(CURDIR)/build/tests"
	@printf "$(GREEN)[*]$(NC) Building host tests...\n"
	@cc -std=c11 -O2 -I"$(CURDIR)/source/core/pair/microecc" -c "$(PAIR_UECC_SRC)" -o "$(PAIR_UECC_OBJ)"
	@c++ -std=c++23 -g -O0 -Wall -Wextra -Wno-unused-parameter \
		-I"$(CURDIR)/include" -I"$(CURDIR)/tests" -I"$(JSONC_PREFIX)/include" \
		-I"$(CURDIR)/library/tomlplusplus/include" \
		$(TEST_SRC) "$(PAIR_UECC_OBJ)" -L"$(JSONC_PREFIX)/lib" -ljson-c -o "$(TEST_BIN)"
	@printf "$(GREEN)[*]$(NC) Running host tests...\n"
	@"$(TEST_BIN)"

submodules:
	@if [ ! -f "$(CURDIR)/library/borealis/README.md" ]; then \
		printf "$(GREEN)[*]$(NC) Initializing submodules...\n"; \
		git -C "$(CURDIR)" submodule update --init --recursive; \
	fi

docker-image: submodules
	@IMAGE_EXISTS=$$(docker image inspect $(DOCKER_IMAGE) > /dev/null 2>&1 && echo yes || echo no); \
	if [ "$$IMAGE_EXISTS" = "no" ]; then \
		printf "$(GREEN)[*]$(NC) Building Docker image...\n"; \
		docker build -t $(DOCKER_IMAGE) "$(CURDIR)"; \
	fi

build: docker-image
	@printf "$(GREEN)[*]$(NC) Building...\n"
	@docker run --rm \
		-v "$(CURDIR):/build" \
		$(WORKTREE_DOCKER_MOUNTS) \
		-w /build \
		-e "MUTE_CHIAKI=$(MUTE_CHIAKI)" \
		$(DOCKER_IMAGE) \
		bash -c " \
			set -e; \
			git config --global --add safe.directory /build; \
			git config --global --add safe.directory /build/library/borealis; \
			git config --global --add safe.directory /build/library/chiaki-ng; \
			git config --global --add safe.directory /build/library/curl-libnx; \
			chmod +x /build/scripts/build-docker.sh; \
			/build/scripts/build-docker.sh \
		"
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

deploy: build
	@if [ -z "$(SWITCH_IP)" ]; then \
		printf "$(YELLOW)[!]$(NC) No SWITCH_IP provided - skipping deployment\n"; \
		echo ""; \
		echo "To deploy, run: make deploy SWITCH_IP=<ip>"; \
		exit 0; \
	fi
	@pkill -f nxlink 2>/dev/null || true
	@mkdir -p "$(CURDIR)/logs"
	$(eval LOG_FILE := $(CURDIR)/logs/$(shell date +%d%m%y%H%M%S).log)
	@printf "$(GREEN)[*]$(NC) Deploying to Switch at $(SWITCH_IP)...\n"
	@printf "$(GREEN)[*]$(NC) Logging to: $(LOG_FILE)\n"
	@printf "$(GREEN)[*]$(NC) Press Ctrl+C to stop receiving logs\n"
	@docker run --rm -it --init \
		--network host \
		-v "$(CURDIR):/build" \
		-w /build \
		$(DOCKER_IMAGE) \
		nxlink -s -a "$(SWITCH_IP)" /build/build/akira.nro 2>&1 | tee "$(LOG_FILE)"
	@printf "$(GREEN)[*]$(NC) Done\n"

rebuild: clean-libs
	@printf "$(GREEN)[*]$(NC) Rebuilding Docker image...\n"
	@docker build -t $(DOCKER_IMAGE) "$(CURDIR)"
	@$(MAKE) build

shell: docker-image
	@printf "$(GREEN)[*]$(NC) Opening shell in build container...\n"
	@docker run --rm -it \
		-v "$(CURDIR):/build" \
		-w /build \
		$(DOCKER_IMAGE) \
		bash

clean-libs:
	@printf "$(GREEN)[*]$(NC) Cleaning library build artifacts...\n"
	@git -C "$(CURDIR)/library/chiaki-ng" clean -fdx 2>/dev/null || true
	@git -C "$(CURDIR)/library/curl-libnx" clean -fdx 2>/dev/null || true
	@printf "$(GREEN)[*]$(NC) Library artifacts cleaned\n"
