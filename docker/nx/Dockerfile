ARG BASE=ghcr.io/chiaki-ng/devkita64@sha256:859aeab50360708e06f6bb570cfc4ed7dfcb022719bf20374e4677bbd0b83414
FROM ${BASE}

ARG NX_DEPS_RELEASE=nx-deps-241026
ARG NX_DEPS_SHA256=f8887c1ea570f1a90fc8db3c986ca785770fa06a71401321788732cb3e167000

RUN apt-get update && apt-get install -y --no-install-recommends \
    bison flex python3-mako python3-pip \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --break-system-packages 'meson>=1.3.0'

RUN for package in switch-ffmpeg switch-libmpv switch-curl; do \
        dkp-pacman -Rdd --noconfirm "$package" || true; \
    done

COPY scripts/docker /opt/akira-docker

RUN curl -fsSL -o /tmp/nx-deps.tar.gz \
        "https://github.com/chiaki-ng/native-deps/releases/download/${NX_DEPS_RELEASE}/build.tar.gz" \
    && echo "${NX_DEPS_SHA256}  /tmp/nx-deps.tar.gz" | sha256sum -c - \
    && mkdir -p /tmp/nx-deps-pool \
    && tar -xzf /tmp/nx-deps.tar.gz -C /tmp/nx-deps-pool \
    && bash /opt/akira-docker/install-nx-deps.sh pool /tmp/nx-deps-pool \
    && rm -rf /tmp/nx-deps.tar.gz /tmp/nx-deps-pool

ENV NX_DEPS=/opt/nx-deps

COPY library/uam /tmp/uam
RUN bash /opt/akira-docker/build-uam.sh /tmp/uam && rm -rf /tmp/uam
