FROM ubuntu:24.04@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates curl python3-venv cmake ninja-build git pkg-config libsystemd-dev libc6-dev linux-libc-dev \
    libgmp10 libmpfr6 libmpc3 libisl23 libzstd1 zlib1g libjansson4 \
    libglvnd-dev libopengl0 libglx0 libgl1 libegl1 libxkbcommon-x11-0 libxcb-cursor0 \
    libglib2.0-0t64 libfontconfig1 libfreetype6 libdbus-1-3 libasound2t64 libpulse0 libgbm1 \
    libnss3 libxcomposite1 libxdamage1 libxrandr2 libxtst6 \
    xvfb xauth rpm patchelf zstd xz-utils p7zip-full xdg-utils
COPY tools/prepare-linux-sdk.sh /prepare-linux-sdk.sh
RUN bash /prepare-linux-sdk.sh /opt/sdk
ENV PATH="/opt/sdk/gcc/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    LD_LIBRARY_PATH="/opt/sdk/gcc/usr/lib:/opt/sdk/Qt/6.11.2/gcc_64/lib" \
    CPLUS_INCLUDE_PATH="/usr/include/x86_64-linux-gnu" \
    C_INCLUDE_PATH="/usr/include/x86_64-linux-gnu" \
    LIBRARY_PATH="/usr/lib/x86_64-linux-gnu" \
    SDK_DIR="/opt/sdk" \
    LANG="C.UTF-8"
WORKDIR /work
