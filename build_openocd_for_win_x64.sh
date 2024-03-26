# !/bin/bash

#########################################################
#
#       Build scripts for openocd windows executable on
#       Linux (debain/Ubuntu)
#
#
#########################################################

OPENOCD_PREFIX=$HOME/openocd-win_x64
LIBUSB_PREFIX=$HOME/libusb-dev
HIDAPI_PREFIX=$HOME/hidapi-dev

BUILD_HOST=x86_64-w64-mingw32

# Prepare build environment
# Install toolchains
sudo apt-get install -y autoconf automake autotools-dev bc bison \
     build-essential curl dejagnu expect flex gawk gperf libtool patchutils texinfo

# Install cross-compilers
sudo apt-get install -y autoconf automake autotools-dev bc bison \
    build-essential curl dejagnu expect flex gawk gperf \
    libtool patchutils texinfo python3 zip \
    mingw-w64 gdb-mingw-w64 libz-mingw-w64-dev

sudo apt-get install -y libudev-dev

if [ ! -f "libusb-1.0.26.tar.bz2" ]; then
    wget https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26.tar.bz2
    tar -jxvf libusb-1.0.26.tar.bz2
fi

if [ ! -f "hidapi-0.13.1.tar.gz" ]; then
    wget https://github.com/libusb/hidapi/archive/refs/tags/hidapi-0.13.1.tar.gz
    tar -zxvf hidapi-0.13.1.tar.gz
fi

# Build libusb
cd libusb-1.0.26
rm $LIBUSB_PREFIX -rf
./configure --host=$BUILD_HOST --prefix=$LIBUSB_PREFIX LDFLAGS="-static" CFLAGS="-static" --enable-static --disable-shared
make clean
make -j8
make install
rm $LIBUSB_PREFIX/lib/*.dll*
cd ..

# Build hidapi
cd hidapi-hidapi-0.13.1
rm $HIDAPI_PREFIX -rf
./bootstrap
./configure --host=$BUILD_HOST --prefix=$HIDAPI_PREFIX LDFLAGS="-static"  CFLAGS="-static" --disable-shared --enable-static
make clean
make -j8
make install
cd ..

# Build openocd
./bootstrap
./configure --host=$BUILD_HOST --prefix=$OPENOCD_PREFIX --bindir=$OPENOCD_PREFIX --disable-shared --enable-static  \
        LIBUSB1_LIBS="-L${LIBUSB_PREFIX}/lib -lusb-1.0" LIBUSB1_CFLAGS="-I${LIBUSB_PREFIX}/include/libusb-1.0 -static" \
        HIDAPI_LIBS="-L${HIDAPI_PREFIX}/lib -lhidapi" HIDAPI_CFLAGS="-I${HIDAPI_PREFIX}/include/hidapi -static" \
        CFLAGS="-static -Os" LDFLAGS="-static" \
        PKG_CONFIG_PATH="${LIBUSB_PREFIX}/lib/pkgconfig:${HIDAPI_PREFIX}/lib/pkgconfig"
make clean
make -j8
make install
#${BUILD_HOST}-strip -S  $OPENOCD_PREFIX/openocd.exe



