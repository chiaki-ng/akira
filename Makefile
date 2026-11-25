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

.PHONY: help build build-arisa arisa-package test-arisa deploy crash test check-strings lint-structure rebuild shell clean docker-image devkit-image fetch-devkit devkit-pin release-notes ounce-deps ounce-deps-check image-tag submodules backup platform

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
ARISA_VERSION      := 0.0.1
ARISA_TAG          := a6721fb
ARISA_GPU          ?= drako3d
ARISA_FLAVOUR_nx_drako3d    := switch
ARISA_FLAVOUR_nx_vulkan     := switch
ARISA_FLAVOUR_ounce_drako3d := switch2
ARISA_FLAVOUR_ounce_vulkan  := switch2
ARISA_SHA256_switch         := f4c0845de15252aa801d2d62cdf17ffaaf3cf0c6e3ca06ec2e20a63d05c54d12
ARISA_SHA256_switch2        := 0ff449f4a34cec0bf741f2d3cc653956d21f536f3828ea59ae14453da5e7e962
ARISA_RECIPE_switch         := make package TARGET=switch
ARISA_RECIPE_switch2        := make package-switch2
ARISA_PACKAGE_DIR  ?= $(abspath $(CURDIR)/../arisa/build/package)
ARISA_FLAVOUR       = $(ARISA_FLAVOUR_$(PLATFORM)_$(ARISA_GPU))
ARISA_SHA256        = $(ARISA_SHA256_$(ARISA_FLAVOUR))
ARISA_NAME          = arisa-$(ARISA_VERSION)-$(ARISA_FLAVOUR)-aarch64
ARISA_ARCHIVE       = $(ARISA_PACKAGE_DIR)/$(ARISA_NAME).tar.gz
ARISA_UNPACK        = $(CURDIR)/build/arisa-package/$(ARISA_SHA256)
ARISA_PREFIX        = $(ARISA_UNPACK)/$(ARISA_NAME)
ARISA_BUILD_NAME    = arisa-$(PLATFORM)$(if $(filter ounce_vulkan,$(PLATFORM)_$(ARISA_GPU)),-vulkan)
ARISA_NRO_FILE      = $(CURDIR)/build/$(ARISA_BUILD_NAME)/akira-arisa.nro
ARISA_HOST_NAME    := arisa-$(ARISA_VERSION)-macos-arm64
ARISA_HOST_SHA256  := f8680a022e4ed4a9c860a797d0b048d7f917b520d00aeef5645690b02fe58271
ARISA_HOST_ARCHIVE  = $(ARISA_PACKAGE_DIR)/$(ARISA_HOST_NAME).tar.gz
ARISA_HOST_UNPACK   = $(CURDIR)/build/arisa-package/$(ARISA_HOST_SHA256)
ARISA_HOST_PREFIX   = $(ARISA_HOST_UNPACK)/$(ARISA_HOST_NAME)
ARISA_HOST_CXX     ?= /opt/homebrew/bin/g++-16
ARISA_TEST_DIR      = $(CURDIR)/build/tests/arisa
ARISA_TEST_MODELS   = $(CURDIR)/source/log/log.cpp \
                      $(CURDIR)/source/play/present_timing.cpp \
                      $(CURDIR)/source/play/present_loop.cpp \
                      $(CURDIR)/source/app/strings.cpp \
                      $(CURDIR)/source/consoles/console_entry.cpp \
                      $(CURDIR)/source/consoles/console_list.cpp \
                      $(CURDIR)/source/consoles/console_pin.cpp \
                      $(CURDIR)/source/consoles/debug_consoles.cpp \
                      $(CURDIR)/source/play/connect_flow.cpp \
                      $(CURDIR)/source/play/connect_failure.cpp \
                      $(CURDIR)/source/play/stream_summary.cpp \
                      $(CURDIR)/source/play/stats_readout.cpp \
                      $(CURDIR)/source/play/diagnostics_history.cpp \
                      $(CURDIR)/source/play/diagnostics_report.cpp \
                      $(CURDIR)/source/play/stats_hud_placement.cpp \
                      $(CURDIR)/source/play/stream_settings.cpp \
                      $(CURDIR)/source/play/colour_adjustments.cpp \
                      $(CURDIR)/source/play/volume.cpp \
                      $(CURDIR)/source/play/controller_settings.cpp \
                      $(CURDIR)/source/play/picture_profile.cpp \
                      $(CURDIR)/source/settings/settings_store.cpp \
                      $(CURDIR)/source/settings/help_catalog.cpp \
                      $(CURDIR)/source/network/nat_report.cpp \
                      $(CURDIR)/source/log/core_log.cpp \
                      $(CURDIR)/source/settings/developer_settings.cpp \
                      $(CURDIR)/source/settings/config_listing.cpp \
                      $(CURDIR)/source/cloud/cloud_settings.cpp \
                      $(CURDIR)/source/cloud/saved_datacenters.cpp \
                      $(CURDIR)/source/network/network_settings.cpp \
                      $(CURDIR)/source/network/custom_dns.cpp \
                      $(CURDIR)/source/network/dns_client.cpp \
                      $(CURDIR)/source/app/app_settings.cpp \
                      $(CURDIR)/source/play/combo_capture.cpp \
                      $(CURDIR)/source/play/button_board.cpp \
                      $(CURDIR)/source/play/pad_choice.cpp \
                      $(CURDIR)/source/play/seat_board.cpp \
                      $(CURDIR)/source/app/qr_matrix.cpp \
                      $(CURDIR)/source/app/build_info.cpp \
                      $(CURDIR)/source/log/log_folder.cpp \
                      $(CURDIR)/source/log/log_excerpt.cpp \
                      $(CURDIR)/library/qrcodegen/qrcodegen.cpp \
                      $(CURDIR)/source/consoles/recent_play.cpp \
                      $(CURDIR)/source/trophies/title_listing.cpp \
                      $(CURDIR)/source/trophies/trophy_listing.cpp \
                      $(CURDIR)/source/cloud/models.cpp \
                      $(CURDIR)/source/cloud/library_listing.cpp \
                      $(CURDIR)/source/cloud/library_status.cpp \
                      $(CURDIR)/source/cloud/fetch_progress.cpp \
                      $(CURDIR)/source/cloud/game_actions.cpp \
                      $(CURDIR)/source/cloud/launch_checks.cpp \
                      $(CURDIR)/source/cloud/title_refresh_cooldown.cpp \
                      $(CURDIR)/source/cloud/cloud_shortcuts.cpp \
                      $(CURDIR)/source/cloud/release_notice.cpp \
                      $(CURDIR)/source/cloud/game_badges.cpp \
                      $(CURDIR)/source/cloud/game_facts.cpp \
                      $(CURDIR)/source/cloud/launch_report.cpp \
                      $(CORE_DIR)/src/core/trophies/model/ids.cpp \
                      $(CORE_DIR)/src/core/utils/text/ascii.cpp \
                      $(CORE_DIR)/src/core/version.cpp \
                      $(CORE_DIR)/src/core/play/model/outcome.cpp \
                      $(CORE_DIR)/src/core/consoles/model/pairing/pairing.cpp \
                      $(CORE_DIR)/src/platform/memory/secure_zero_darwin.cpp
ARISA_TEST_SCREENS  = $(CURDIR)/source/consoles/ui/console_shelf.cpp \
                      $(CURDIR)/source/consoles/ui/console_grid.cpp \
                      $(CURDIR)/source/consoles/ui/console_card.cpp \
                      $(CURDIR)/source/consoles/ui/pin_pad.cpp \
                      $(CURDIR)/source/consoles/ui/console_editor.cpp \
                      $(CURDIR)/source/consoles/ui/add_console_sheet.cpp \
                      $(CURDIR)/source/consoles/ui/registration_progress.cpp \
                      $(CURDIR)/source/play/ui/connecting_screen.cpp \
                      $(CURDIR)/source/play/ui/stream_dock.cpp \
                      $(CURDIR)/source/play/ui/stats_board.cpp \
                      $(CURDIR)/source/play/ui/diagnostics_overlay.cpp \
                      $(CURDIR)/source/play/ui/mapping.cpp \
                      $(CURDIR)/source/play/ui/button_board_view.cpp \
                      $(CURDIR)/source/play/ui/couch.cpp \
                      $(CURDIR)/source/play/ui/players_panel.cpp \
                      $(CURDIR)/source/play/ui/pad_picker.cpp \
                      $(CURDIR)/source/play/ui/couch_prompt.cpp \
                      $(CURDIR)/source/play/ui/seat_passcode.cpp \
                      $(CURDIR)/source/play/ui/login_pin_card.cpp \
                      $(CURDIR)/source/consoles/ui/recent_shelf.cpp \
                      $(CURDIR)/source/consoles/ui/recent_rail.cpp \
                      $(wildcard $(CURDIR)/source/trophies/ui/*.cpp) \
                      $(wildcard $(CURDIR)/source/cloud/ui/*.cpp) \
                      $(CURDIR)/source/app/arisa/artwork.cpp \
                      $(CURDIR)/source/app/arisa/hero_figure.cpp \
                      $(CURDIR)/source/app/arisa/art_session.cpp \
                      $(wildcard $(CURDIR)/source/accounts/ui/*.cpp) \
                      $(wildcard $(CURDIR)/source/log/ui/*.cpp) \
                      $(wildcard $(CURDIR)/source/settings/ui/*.cpp) \
                      $(wildcard $(CURDIR)/source/network/ui/*.cpp) \
                      $(CURDIR)/source/app/arisa/shell_screen.cpp \
                      $(CURDIR)/source/app/arisa/shell_model.cpp \
                      $(CURDIR)/source/app/arisa/notices.cpp \
                      $(CURDIR)/source/app/arisa/notice_toast.cpp \
                      $(CURDIR)/source/app/arisa/stage_screen.cpp \
                      $(CURDIR)/source/app/arisa/overlay_handoff.cpp \
                      $(CURDIR)/source/app/arisa/stream_stage.cpp \
                      $(CURDIR)/source/app/arisa/overlay_palette.cpp \
                      $(CURDIR)/source/app/arisa/confirmation.cpp \
                      $(CURDIR)/source/app/arisa/rail.cpp \
                      $(CURDIR)/source/app/arisa/header.cpp \
                      $(CURDIR)/source/app/arisa/brand_lockup.cpp \
                      $(CURDIR)/source/app/arisa/ambient_background.cpp \
                      $(CURDIR)/source/app/arisa/theme_packs.cpp \
                      $(CURDIR)/source/app/arisa/qr_code.cpp \
                      $(CURDIR)/source/app/arisa/about_page.cpp \
                      $(CURDIR)/source/app/arisa/build_info_page.cpp \
                      $(CURDIR)/source/app/arisa/pad_capture.cpp
ARISA_TEST_STB      = $(CURDIR)/library/borealis/library/lib/extern/glfw/deps
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
                $(CURDIR)/source/app/strings.cpp \
                $(CURDIR)/source/cloud/models.cpp \
                $(CURDIR)/source/cloud/library_listing.cpp \
                $(CURDIR)/source/cloud/game_actions.cpp \
                $(CURDIR)/source/cloud/launch_checks.cpp \
                $(CURDIR)/source/cloud/library_status.cpp \
                $(CURDIR)/source/cloud/catalog_rows.cpp \
                $(CURDIR)/source/cloud/cloud_shortcuts.cpp \
                $(CURDIR)/source/cloud/game_badges.cpp \
                $(CURDIR)/source/cloud/game_facts.cpp \
                $(CURDIR)/source/cloud/release_notice.cpp \
                $(CURDIR)/source/cloud/title_refresh_cooldown.cpp \
                $(CURDIR)/source/cloud/start_steps.cpp \
                $(CURDIR)/source/cloud/launch_report.cpp \
                $(CURDIR)/source/cloud/fetch_progress.cpp \
                $(CURDIR)/source/consoles/pair_crypto.cpp \
                $(CURDIR)/source/consoles/console_address.cpp \
                $(CURDIR)/source/consoles/console_entry.cpp \
                $(CURDIR)/source/consoles/console_list.cpp \
                $(CURDIR)/source/consoles/console_pin.cpp \
                $(CURDIR)/source/consoles/takion_pin_choice.cpp \
                $(CURDIR)/source/consoles/psn_identity.cpp \
                $(CURDIR)/source/consoles/register_flow.cpp \
                $(CURDIR)/source/consoles/discovery_gate.cpp \
                $(CURDIR)/source/consoles/remote_lookup.cpp \
                $(CURDIR)/source/consoles/debug_consoles.cpp \
                $(CURDIR)/source/log/log.cpp \
                $(CURDIR)/source/log/core_log.cpp \
                $(CURDIR)/source/log/log_tail.cpp \
                $(CURDIR)/source/log/log_folder.cpp \
                $(CURDIR)/source/log/log_excerpt.cpp \
                $(CURDIR)/source/app/build_info.cpp \
                $(CURDIR)/source/app/qr_matrix.cpp \
                $(CURDIR)/library/qrcodegen/qrcodegen.cpp \
                $(CURDIR)/source/play/pad_choice.cpp \
                $(CURDIR)/source/play/seat_board.cpp \
                $(CURDIR)/source/play/connect_failure.cpp \
                $(CURDIR)/source/play/connect_flow.cpp \
                $(CURDIR)/source/play/session_ending.cpp \
                $(CURDIR)/source/play/stream_route.cpp \
                $(CURDIR)/source/play/menu_hold.cpp \
                $(CURDIR)/source/play/stream_recovery.cpp \
                $(CURDIR)/source/play/paused_frame.cpp \
                $(CURDIR)/source/play/stream_summary.cpp \
                $(CURDIR)/source/play/stats_readout.cpp \
                $(CURDIR)/source/play/diagnostics_history.cpp \
                $(CURDIR)/source/play/diagnostics_report.cpp \
                $(CURDIR)/source/play/screenshot_chord.cpp \
                $(CURDIR)/source/play/present_timing.cpp \
                $(CURDIR)/source/play/present_loop.cpp \
                $(CURDIR)/source/play/audio_gain.cpp \
                $(CURDIR)/source/play/volume.cpp \
                $(CURDIR)/source/play/colour_adjustments.cpp \
                $(CURDIR)/source/app/screenshot.cpp \
                $(CURDIR)/source/play/stats_hud_placement.cpp \
                $(CURDIR)/source/play/stream_controls.cpp \
                $(CURDIR)/source/settings/settings_store.cpp \
                $(CURDIR)/source/settings/developer_settings.cpp \
                $(CURDIR)/source/play/stream_settings.cpp \
                $(CURDIR)/source/play/picture_profile.cpp \
                $(CURDIR)/source/play/combo_capture.cpp \
                $(CURDIR)/source/play/button_board.cpp \
                $(CURDIR)/source/settings/config_listing.cpp \
                $(CURDIR)/source/settings/help_catalog.cpp \
                $(CURDIR)/source/play/controller_settings.cpp \
                $(CURDIR)/source/cloud/cloud_settings.cpp \
                $(CURDIR)/source/cloud/saved_datacenters.cpp \
                $(CURDIR)/source/network/network_settings.cpp \
                $(CURDIR)/source/network/custom_dns.cpp \
                $(CURDIR)/source/network/nat_report.cpp \
                $(CURDIR)/source/network/sony_dns_failure.cpp \
                $(CURDIR)/source/accounts/account_settings.cpp \
                $(CURDIR)/source/accounts/sign_in_store.cpp \
                $(CURDIR)/source/accounts/profile_tables.cpp \
                $(CURDIR)/source/accounts/profile_choices.cpp \
                $(CURDIR)/source/accounts/profile_switch.cpp \
                $(CURDIR)/source/consoles/console_settings.cpp \
                $(CURDIR)/source/consoles/pairing_store.cpp \
                $(CURDIR)/source/app/app_settings.cpp \
                $(CURDIR)/source/network/dns_client.cpp \
                $(CURDIR)/source/trophies/icon_cache.cpp \
                $(CURDIR)/source/app/art_cache.cpp \
                $(CURDIR)/source/trophies/title_listing.cpp \
                $(CURDIR)/source/trophies/trophy_listing.cpp \
                $(CURDIR)/source/consoles/recent_play.cpp \
                $(CURDIR)/source/platform/video/viddec/h264_sps.cpp \
                $(TEST_PLATFORM_SRC)
CORE_DIR     := $(or $(call override_path,CORE),$(CURDIR)/library/libchiaki-core)
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
                 $(CORE_DIR)/src/core/play/model/outcome.cpp \
                 $(CORE_DIR)/src/core/trophies/model/ids.cpp
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
	@echo "  build-arisa-nx, build-arisa-ounce  Build akira-arisa.nro on the pinned arisa package (ARISA_GPU=drako3d|vulkan on ounce)"
	@echo "  arisa-package-nx, arisa-package-ounce  Check and extract the pinned arisa package"
	@echo "  test-arisa     Run the arisa shell seam tests on the headless arisa macOS package"
	@echo "  image-tag-nx, image-tag-ounce    Print the builder image tag for this checkout"
	@echo "  check-strings  Check resources/strings matches the borealis catalogs"
	@echo "  lint-structure Check the subject/views include rules"
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
	@c++ -std=c++23 -g -O0 -Wall -Wextra -Wno-unused-parameter -DAKIRA_MUTE_CORE_LOGS=0 -DTOML_EXCEPTIONS=0 \
		-DAKIRA_RESOURCES_DIR=\"$(CURDIR)/resources\" -I"$(CURDIR)/include" -I"$(CURDIR)/tests" -I"$(JSONC_PREFIX)/include" -I"$(SIMDUTF_PREFIX)/include" \
		-I"$(CURDIR)/library/tomlplusplus/include" -I"$(CORE_DIR)/include" -I"$(CORE_DIR)" -I"$(OPENSSL_PREFIX)/include" -I"$(CURDIR)/library/qrcodegen" -isystem "$(CURDIR)/library/stb" \
		$(TEST_SRC) $(CORE_TEST_SRC) -L"$(JSONC_PREFIX)/lib" -ljson-c -L"$(SIMDUTF_PREFIX)/lib" -lsimdutf \
		-L"$(OPENSSL_PREFIX)/lib" -lcrypto -o "$(TEST_BIN)"
	@printf "$(GREEN)[*]$(NC) Running host tests...\n"
	@"$(TEST_BIN)"

ARISA_TEST_OBJ      = $(ARISA_TEST_DIR)/obj
ARISA_TEST_GEN      = $(ARISA_TEST_DIR)/generated
ARISA_TEST_INCLUDES = -I"$(CURDIR)/include" -I"$(CORE_DIR)/include" -I"$(CORE_DIR)" -I"$(ARISA_TEST_GEN)" \
                      -I"$(CURDIR)/library/tomlplusplus/include" -I"$(JSONC_PREFIX)/include" -I"$(SIMDUTF_PREFIX)/include"
ARISA_MODEL_FLAGS   = -std=c++23 -O0 -g -DAKIRA_CLOUD=1 -DAKIRA_MUTE_CORE_LOGS=0 -DTOML_EXCEPTIONS=0 \
                      $(ARISA_TEST_INCLUDES) -I"$(CURDIR)/library/qrcodegen"
ARISA_SCREEN_FLAGS  = -std=c++23 -O0 -g -fno-rtti -DAKIRA_CLOUD=1 -DAKIRA_MUTE_CORE_LOGS=0 -DTOML_EXCEPTIONS=0 \
                      -DAKIRA_RESOURCES_DIR=\"$(CURDIR)/resources\" -DARISA_ASSETS_DIR=\"$(ARISA_HOST_PREFIX)/share/arisa/assets\" \
                      -DAKIRA_SHOTS_DIR=\"$(CURDIR)/work/shots\" -I"$(CURDIR)/tests" -I"$(ARISA_HOST_PREFIX)/include" \
                      $(ARISA_TEST_INCLUDES) -isystem "$(ARISA_TEST_STB)"
ARISA_SCREEN_SOURCES = $(CURDIR)/tests/test_main.cpp $(wildcard $(CURDIR)/tests/arisa/*.cpp) \
                      $(CURDIR)/source/app/arisa/dispatcher.cpp $(CURDIR)/source/app/arisa/string_catalog.cpp $(ARISA_TEST_SCREENS)
ARISA_MODEL_OBJECTS  = $(patsubst /%.cpp,$(ARISA_TEST_OBJ)/models/%.o,$(abspath $(ARISA_TEST_MODELS)))
ARISA_SCREEN_OBJECTS = $(patsubst /%.cpp,$(ARISA_TEST_OBJ)/screens/%.o,$(abspath $(ARISA_SCREEN_SOURCES)))
ARISA_TEST_HEADERS   = $(ARISA_TEST_GEN)/akira_version.h $(ARISA_TEST_GEN)/version_config.hpp
ARISA_TEST_STAMP     = $(ARISA_TEST_DIR)/flags.stamp
ARISA_TEST_LIB       = $(ARISA_HOST_PREFIX)/lib/libarisa.a

test-arisa:
	@test -f "$(ARISA_HOST_ARCHIVE)" || { printf "$(RED)[x]$(NC) No arisa host package at $(ARISA_HOST_ARCHIVE): build arisa $(ARISA_TAG) with make package TARGET=macos\n"; exit 1; }
	@echo "$(ARISA_HOST_SHA256)  $(ARISA_HOST_ARCHIVE)" | shasum -a 256 -c - > /dev/null || { printf "$(RED)[x]$(NC) $(ARISA_HOST_ARCHIVE) does not match the pinned sha256\n"; exit 1; }
	@if [ ! -f "$(ARISA_TEST_LIB)" ]; then mkdir -p "$(ARISA_HOST_UNPACK)" && tar -xzf "$(ARISA_HOST_ARCHIVE)" -C "$(ARISA_HOST_UNPACK)"; fi
	@printf "$(GREEN)[*]$(NC) Building arisa seam and screen tests...\n"
	@$(MAKE) --no-print-directory -j4 "$(ARISA_TEST_DIR)/seam_tests"
	@printf "$(GREEN)[*]$(NC) Running arisa seam tests...\n"
	@"$(ARISA_TEST_DIR)/seam_tests"

$(ARISA_TEST_STAMP): FORCE
	@mkdir -p "$(@D)"
	@echo '$(ARISA_MODEL_FLAGS) | $(ARISA_SCREEN_FLAGS)' | cmp -s - "$@" || echo '$(ARISA_MODEL_FLAGS) | $(ARISA_SCREEN_FLAGS)' > "$@"

$(ARISA_TEST_GEN)/akira_version.h: $(CURDIR)/cmake/akira_version.h.in
	@mkdir -p "$(@D)"
	@sed -e 's/@VERSION_MAJOR@/0/' -e 's/@VERSION_MINOR@/0/' -e 's/@VERSION_PATCH@/0/' -e 's/@APP_VERSION@/test/' \
		-e 's/@APP_VERSION_NACP@/0.0.0/' -e 's/@BUILD_CHANNEL@/dev/' -e 's/@GIT_COMMIT@/test/' -e 's/@BUILD_DATE@/test/' "$<" > "$@"

$(ARISA_TEST_GEN)/version_config.hpp: $(CORE_DIR)/cmake/version_config.hpp.in
	@mkdir -p "$(@D)"
	@sed -e 's/@PROJECT_VERSION_MAJOR@/0/' -e 's/@PROJECT_VERSION_MINOR@/0/' -e 's/@PROJECT_VERSION_PATCH@/0/' \
		-e 's/@LIBCHIAKING_VERSION_PRERELEASE@//' -e 's/@libchiaking_commit@/test/' "$<" > "$@"

$(ARISA_TEST_OBJ)/models/%.o: /%.cpp $(ARISA_TEST_STAMP) | $(ARISA_TEST_HEADERS)
	@mkdir -p "$(@D)"
	@nice -n 10 $(ARISA_HOST_CXX) $(ARISA_MODEL_FLAGS) -MMD -MP -c "$<" -o "$@"

$(ARISA_TEST_OBJ)/screens/%.o: /%.cpp $(ARISA_TEST_STAMP) | $(ARISA_TEST_HEADERS)
	@mkdir -p "$(@D)"
	@nice -n 10 $(ARISA_HOST_CXX) $(ARISA_SCREEN_FLAGS) -MMD -MP -c "$<" -o "$@"

$(ARISA_TEST_DIR)/seam_tests: $(ARISA_MODEL_OBJECTS) $(ARISA_SCREEN_OBJECTS) $(ARISA_TEST_LIB)
	@nice -n 10 $(ARISA_HOST_CXX) -std=c++23 -g $(ARISA_MODEL_OBJECTS) $(ARISA_SCREEN_OBJECTS) "$(ARISA_TEST_LIB)" \
		-L"$(JSONC_PREFIX)/lib" -ljson-c -L"$(SIMDUTF_PREFIX)/lib" -lsimdutf -o "$@"

-include $(ARISA_MODEL_OBJECTS:.o=.d) $(ARISA_SCREEN_OBJECTS:.o=.d)

.PHONY: FORCE
FORCE:

check-strings:
	@python3 "$(CURDIR)/scripts/convert-i18n.py" --check

lint-structure:
	@bash "$(CURDIR)/scripts/lint-structure.sh"

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

arisa-package: platform
	@test -n "$(ARISA_FLAVOUR)" || { printf "$(RED)[x]$(NC) ARISA_GPU must be drako3d or vulkan, not $(ARISA_GPU)\n"; exit 1; }
	@test -f "$(ARISA_ARCHIVE)" || { printf "$(RED)[x]$(NC) No arisa package at $(ARISA_ARCHIVE): build arisa $(ARISA_TAG) with $(ARISA_RECIPE_$(ARISA_FLAVOUR)), or pass ARISA_PACKAGE_DIR\n"; exit 1; }
	@echo "$(ARISA_SHA256)  $(ARISA_ARCHIVE)" | shasum -a 256 -c - > /dev/null || { printf "$(RED)[x]$(NC) $(ARISA_ARCHIVE) does not match the pinned sha256 $(ARISA_SHA256)\n"; exit 1; }
	@if [ ! -f "$(ARISA_PREFIX)/lib/libarisa.a" ]; then \
		printf "$(GREEN)[*]$(NC) Extracting $(ARISA_NAME) ($(ARISA_TAG))...\n"; \
		mkdir -p "$(ARISA_UNPACK)"; \
		tar -xzf "$(ARISA_ARCHIVE)" -C "$(ARISA_UNPACK)"; \
	fi

build-arisa: platform docker-image arisa-package
	@printf "$(GREEN)[*]$(NC) Building akira-arisa for $(PLATFORM) with $(DOCKER_IMAGE)...\n"
	@$(DOCKER_RUN) \
		-e AKIRA_PLATFORM="$(PLATFORM)" \
		-e AKIRA_SHELL=arisa \
		-e AKIRA_ARISA_PREFIX="$(ARISA_PREFIX)" \
		-e AKIRA_ARISA_GPU="$(ARISA_GPU)" \
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
	@test -f "$(ARISA_NRO_FILE)" || { printf "$(RED)[x]$(NC) Build failed - NRO not found at $(ARISA_NRO_FILE)\n"; exit 1; }
	@printf "$(GREEN)[*]$(NC) Build successful: $(ARISA_NRO_FILE)\n"

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
