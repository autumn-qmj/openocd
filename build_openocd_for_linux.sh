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

sudo apt install libusb-dev
sudo apt-get install -y libudev-dev
sudo apt install libhidapi-dev
sudo apt install libjaylink-dev
sudo apt-get install build-essential pkg-config autoconf automake libtool libusb-dev libusb-1.0-0-dev libhidapi-dev


if [ ! -f "libusb-1.0.26.tar.bz2" ]; then
    wget https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26.tar.bz2
    tar -jxvf libusb-1.0.26.tar.bz2
fi

if [ ! -f "hidapi-0.13.1.tar.gz" ]; then
    wget https://github.com/libusb/hidapi/archive/refs/tags/hidapi-0.13.1.tar.gz
    tar -zxvf hidapi-0.13.1.tar.gz
fi

# Build openocd
./bootstrap
./configure --prefix=$OPENOCD_PREFIX --bindir=$OPENOCD_PREFIX --disable-shared --enable-static  \
        CFLAGS="-static -Os" LDFLAGS="-static"
make clean
make -j8
make install
#${BUILD_HOST}-strip -S  $OPENOCD_PREFIX/openocd.exe



