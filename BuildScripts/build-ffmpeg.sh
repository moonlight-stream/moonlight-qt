#!/bin/bash

SYSROOT_iPHONE="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
SYSROOT_SIMULATOR="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"
CC_IPHONE="xcrun -sdk iphoneos clang"
CC_SIMULATOR="xcrun -sdk iphonesimulator clang"
function build_one {
    ./configure \
        --prefix=$PREFIX \
        --disable-ffmpeg \
        --disable-ffplay \
        --disable-ffprobe \
        --disable-ffserver \
        --disable-armv5te \
        --disable-armv6 \
        --disable-doc \
        --disable-everything \
        --disable-debug \
        --enable-decoder=h264 \
        --enable-avresample \
        --enable-cross-compile \
        --sysroot=$SYSROOT \
        --target-os=darwin \
        --cc="$CC" \
        --extra-cflags="$CFLAGS" \
        --extra-ldflags="$LDFLAGS" \
        --enable-pic \
        $ADDI_FLAGS
    make clean && make -j4 && make install
}


# armv7
function build_armv7 {
    PREFIX="armv7"
    SYSROOT=$SYSROOT_iPHONE
    CC=$CC_IPHONE
    CFLAGS="-arch armv7 -mfpu=neon -miphoneos-version-min=7.1 -fpic"
    LDFLAGS="-arch armv7 -isysroot $SYSROOT_iPHONE -miphoneos-version-min=7.1"
    ADDI_FLAGS="--arch=arm --cpu=cortex-a9"
    build_one
}

# armv7s
function build_armv7s {
    PREFIX="armv7s"
    SYSROOT=$SYSROOT_iPHONE
    CC=$CC_IPHONE
    CFLAGS="-arch armv7s -mfpu=neon -miphoneos-version-min=7.1"
    LDFLAGS="-arch armv7s -isysroot $SYSROOT_iPHONE -miphoneos-version-min=7.1"
    ADDI_FLAGS="--arch=arm --cpu=cortex-a9"
    build_one
}

# i386
function build_i386 {
    PREFIX="i386"
    SYSROOT=$SYSROOT_SIMULATOR
    CC=$CC_SIMULATOR
    CFLAGS="-arch i386"
    LDFLAGS="-arch i386 -isysroot $SYSROOT_SIMULATOR -mios-simulator-version-min=7.1"
    ADDI_FLAGS="--arch=i386 --cpu=i386 --disable-asm"
    build_one
}


# create fat library
function build_universal {
    cd armv7/lib
    for file in *.a
    do
        cd ../..
        xcrun -sdk iphoneos lipo -output universal/lib/$file  -create \
        -arch armv7 armv7/lib/$file \
        -arch armv7s armv7s/lib/$file \
        -arch i386 i386/lib/$file
        echo "Universal $file created."
        cd -
    done
    cd ../..
}

echo "FFmpeg directory?"
read SOURCE

cd $SOURCE

if [ "$1" = "clean" ]
    then
        rm -r armv7 armv7s i386 universal
        make clean
        exit 0
fi
mkdir armv7
mkdir armv7s
mkdir i386
mkdir -p universal/lib

build_armv7
build_armv7s
build_i386
build_universal
echo "Ouput files in $SOURCE/armv7 $SOURCE/armv7s $SOURCE/i386 $SOURCE/universal"

