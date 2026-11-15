ARG BASE=ghcr.io/chiaki-ng/devkita64@sha256:3a978bfec72c624aed88b8197f3f46b7288e091f3c28f0e5697b7c211df9c969
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

LABEL org.opencontainers.image.title="devkita64-ounce"
CMD ["bash"]
