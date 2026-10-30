FROM devkitpro/devkita64:latest

RUN rm -f /etc/apt/sources.list.d/devkitpro.list && \
    apt-get update && apt-get install -y \
    autoconf automake libtool pkg-config \
    protobuf-compiler python3-protobuf \
    python3 python3-pip \
    python3-mako ninja-build bison flex \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --break-system-packages meson>=1.3.0

RUN dkp-pacman -Syu --noconfirm && \
    dkp-pacman -S --noconfirm \
    switch-dev \
    switch-sdl2 \
    switch-mbedtls \
    switch-zlib \
    switch-bzip2 \
    switch-freetype \
    switch-libass \
    switch-libfribidi \
    switch-harfbuzz \
    switch-mesa \
    switch-glfw \
    switch-glad \
    switch-libjson-c \
    switch-dav1d \
    switch-libopus \
    switch-miniupnpc \
    switch-libssh2 \
    switch-pkg-config \
    dkp-toolchain-vars \
    && dkp-pacman -Rdd --noconfirm switch-ffmpeg switch-libmpv || true \
    && dkp-pacman -Scc --noconfirm

WORKDIR /build/ffmpeg
COPY library/ffmpeg /build/ffmpeg

# Akira only opens H.264 and HEVC video streams. Keep their parsers and
# Switch nvtegra paths, not FFmpeg's full codec registry.
RUN bash -c '\
    source /opt/devkitpro/switchvars.sh && \
    ./configure \
        --prefix=$PORTLIBS_PREFIX \
        --enable-gpl \
        --disable-shared \
        --enable-static \
        --cross-prefix=aarch64-none-elf- \
        --enable-cross-compile \
        --arch=aarch64 \
        --cpu=cortex-a57 \
        --target-os=horizon \
        --enable-pic \
        --extra-cflags="-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec" \
        --extra-cxxflags="-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec" \
        --extra-ldflags="-fPIE -L${PORTLIBS_PREFIX}/lib -L${DEVKITPRO}/libnx/lib" \
        --disable-runtime-cpudetect \
        --disable-programs \
        --disable-debug \
        --disable-doc \
        --enable-asm \
        --enable-neon \
        --disable-autodetect \
        --disable-everything \
        --enable-version3 \
        --disable-avdevice \
        --disable-encoders \
        --disable-muxers \
        --enable-decoder=h264,hevc \
        --enable-hwaccel=h264_nvtegra,hevc_nvtegra \
        --enable-parser=h264,hevc \
        --enable-swscale \
        --enable-swresample \
        --disable-network \
        --enable-nvtegra && \
    make -j$(nproc) && \
    make install'

WORKDIR /build/uam
COPY library/uam /build/uam

RUN echo "option('build_as_library', type : 'boolean', value : false)" > meson_options.txt && \
    sed -i "/static_library/,/^[[:space:]]*)/{s/include_directories: uam_incs$/include_directories: uam_incs, install: true/}" meson.build && \
    bash -c '\
    source /opt/devkitpro/switchvars.sh && \
    /opt/devkitpro/meson-cross.sh switch crossfile_dkp build -Dbuild_as_library=true && \
    meson compile -C build && \
    meson install -C build && \
    cp source/uam.h $PORTLIBS_PREFIX/include/'

WORKDIR /build
