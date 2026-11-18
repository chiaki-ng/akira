ARG BASE=ghcr.io/chiaki-ng/devkita64@sha256:3a978bfec72c624aed88b8197f3f46b7288e091f3c28f0e5697b7c211df9c969

FROM ${BASE} AS uam2

ARG RUST_VERSION=1.91.0
ARG MESON_VERSION=1.9.1
ARG UAM2_COMMIT=b794767cd0e4854199271dfd959798b8714edd23

RUN apt-get update && \
    apt-get install -y --no-install-recommends bison flex g++ pkg-config git curl ca-certificates python3-venv \
        libdrm-dev libexpat1-dev zlib1g-dev libzstd-dev spirv-tools libclang-dev && \
    rm -rf /var/lib/apt/lists/*

ENV PATH=/opt/uam2/venv/bin:/root/.cargo/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN python3 -m venv /opt/uam2/venv && \
    pip install --no-cache-dir "meson==${MESON_VERSION}" mako pyyaml packaging ninja && \
    curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain "${RUST_VERSION}" && \
    cargo install --locked bindgen-cli cbindgen

RUN git clone -q https://github.com/ouncebrew/uam2.git /tmp/uam2 && \
    git -C /tmp/uam2 checkout -q "${UAM2_COMMIT}" && \
    meson setup /tmp/uam2/build /tmp/uam2 && \
    ninja -C /tmp/uam2/build && \
    install -D /tmp/uam2/build/source/uam2 /opt/uam2/bin/uam2

FROM ${BASE}

ARG OUNCE_DEVKIT_VERSION

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITA64=/opt/devkitpro/devkitA64
ENV PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/portlibs/switch2/bin:/opt/devkitpro/tools/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN apt-get update && \
    apt-get install -y --no-install-recommends glslang-tools && \
    rm -rf /var/lib/apt/lists/*

RUN dkp-pacman -Qq | grep -vxE 'devkitA64|devkita64-(binutils|cmake|gcc|newlib|rules)|devkit-env|devkitpro-keyring|dkp-(cmake-common-utils|meson-scripts|toolchain-vars)|general-tools|pacman|switch-(cmake|tools|pkg-config|mbedtls)|uam' | \
        xargs dkp-pacman -Rdd --noconfirm && \
    rm -rf "${DEVKITPRO}/libnx" "${DEVKITPRO}/examples" && \
    test -f "${DEVKITPRO}/portlibs/switch/lib/libmbedtls.a" && \
    test -z "$(ls "${DEVKITPRO}/portlibs/switch/lib" | grep -vE '^(libmbed(tls|x509|crypto)\.a|pkgconfig|cmake)$')"

COPY --from=ounce-devkit libounce /opt/devkitpro/libounce
COPY --from=ounce-devkit portlibs/switch2 /opt/devkitpro/portlibs/switch2
COPY --from=ounce-devkit switch2vars.sh ounce-devkit-version.txt /opt/devkitpro/
RUN grep -qx "ounce-devkit ${OUNCE_DEVKIT_VERSION}" "${DEVKITPRO}/ounce-devkit-version.txt" && \
    test -f "${DEVKITPRO}/libounce/lib/libounce.a" && \
    ln -s libounce "${DEVKITPRO}/libnx"

ARG DRAKOTOOLS_COMMIT=b32fe6e43533e80414f6aa4ad3b899ed077f806c
ARG DRAKO3D_COMMIT=d1f05077667d0eabdd39d8a65483ffcec51807c2

RUN apt-get update && \
    apt-get install -y --no-install-recommends meson ninja-build bison flex g++ git python3 ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN git clone -q https://github.com/ouncebrew/drakotools.git /tmp/drakotools && \
    git -C /tmp/drakotools checkout -q "${DRAKOTOOLS_COMMIT}" && \
    meson setup /tmp/drakotools/build /tmp/drakotools --prefix="${DEVKITPRO}/tools" && \
    ninja -C /tmp/drakotools/build install && \
    command -v drakodef drakomme && \
    rm -rf /tmp/drakotools

RUN git clone -q https://github.com/ouncebrew/drako3d.git /tmp/drako3d && \
    git -C /tmp/drako3d checkout -q "${DRAKO3D_COMMIT}" && \
    make -C /tmp/drako3d -j"$(nproc)" && \
    make -C /tmp/drako3d install && \
    test -f "${DEVKITPRO}/libounce/lib/libdrako3d.a" && \
    test -f "${DEVKITPRO}/libounce/include/drako3d.hpp" && \
    rm -rf /tmp/drako3d

RUN apt-get update && \
    apt-get install -y --no-install-recommends libdrm2 libexpat1 zlib1g libzstd1 spirv-tools && \
    rm -rf /var/lib/apt/lists/*

COPY --from=uam2 /opt/uam2/bin/uam2 /opt/devkitpro/tools/bin/uam2

RUN printf '#version 460\nlayout (location = 0) out vec4 color;\nvoid main() { color = vec4(1.0); }\n' > /tmp/smoke.glsl && \
    uam2 -s frag -o /tmp/smoke.dksh /tmp/smoke.glsl && \
    test -s /tmp/smoke.dksh && \
    rm /tmp/smoke.glsl /tmp/smoke.dksh

LABEL org.opencontainers.image.title="devkita64-ounce"
CMD ["bash"]
