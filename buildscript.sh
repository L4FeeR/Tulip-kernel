mkdir -p out
export ARCH=arm64
export SUBARCH=arm64
export KBUILD_BUILD_USER=Lafeer
export KBUILD_BUILD_HOST=Linux
export CC=clang
export CLANG_PATH="/usr/bin"
export CLANG_TRIPLE=aarch64-linux-gnu-
export CROSS_COMPILE="/usr/bin/aarch64-linux-gnu-"
export CROSS_COMPILE_ARM32="/usr/bin/arm-linux-gnueabi-"
make CC=clang O=out mystic-tulip-oldcam_defconfig
make CC=clang O=out ARCH=$ARCH menuconfig
echo "  time make CC=clang O=out -j$(nproc --all)"
