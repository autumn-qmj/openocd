# !/bin/bash

#########################################################
#
#       Build scripts for openocd windows executable on
#       Linux (debain/Ubuntu)
#
#
#########################################################

OPENOCD_PREFIX=$HOME/openocd-linux
LIBUSB_PREFIX=$HOME/libusb-dev
HIDAPI_PREFIX=$HOME/hidapi-dev

# Prepare build environment
# Install toolchains
sudo apt-get install -y autoconf automake autotools-dev bc bison \
     build-essential curl dejagnu expect flex gawk gperf libtool patchutils texinfo

# Install cross-compilers
sudo apt-get install -y autoconf automake autotools-dev bc bison \
    build-essential curl dejagnu expect flex gawk gperf \
    libtool patchutils texinfo python3 zip

sudo apt-get install -y libudev-dev


# Build openocd
./bootstrap
./configure --prefix=$OPENOCD_PREFIX --bindir=$OPENOCD_PREFIX --disable-shared --enable-static  \
        CFLAGS="-static -Os" LDFLAGS="-static"
make clean
make -j8
make install
#${BUILD_HOST}-strip -S  $OPENOCD_PREFIX/openocd.exe



