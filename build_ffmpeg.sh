#!/bin/bash
#设置ndk目录
NDK=/Users/pcyfox/toolchains/arm-linux-androideabi
API=21
SYSROOT=$NDK/sysroot
TOOLCHAIN=$NDK/bin
#armv7 arm64 x86 x86_64 ARCH=armv7
#arm aarch64  i686 x86_64
ARCH2=aarch64

echo '第一个参数: '$1 
if [ ! -n "$1" ]
then
    echo "没有参数 使用默认架构"
else
    ARCH=$1
    if [ $1 == "arm64" ]
    then
        ARCH2=aarch64
    elif [ $1 == "x86" ]
    then
        ARCH2=i686
    elif [ $1 == "x86_64" ]
    then
        ARCH2=x86_64
    fi
fi



if [ $ARCH == "armv7" ]
    then
    PLATFORM=$ARCH2-linux-androideabi
    ANDROID_CROSS_PREFIX=$TOOLCHAIN/armv7a-linux-androideabi$API-
else 
    PLATFORM=$ARCH2-linux-android
    ANDROID_CROSS_PREFIX=$TOOLCHAIN/$PLATFORM$API-  
fi


OS=android
CROSS_PREFIX=$TOOLCHAIN/$PLATFORM-

echo "ARCH:${ARCH}, ARCH2:${ARCH2}"
pwd
cd /Users/pcyfox/FFmpeg
pwd
#保存目录
PREFIX=$(pwd)/build_android/$ARCH

echo "TooChain ------->$TOOLCHAIN"
echo "PREFIX   ------->$PREFIX"
echo "ANDROID_CROSS_PREFIX------>$ANDROID_CROSS_PREFIX"
echo "CROSS_PREFIX------>$CROSS_PREFIX"

build()
{
    make clean
    echo "----------------configure start!--------------------"
    ./configure \
    --prefix=$PREFIX \
    --enable-cross-compile \
    --cross-prefix=$CROSS_PREFIX \
    --arch=$ARCH \
    --target-os=$OS \
    --pkg-config=$(which pkg-config) \
    --cc=${ANDROID_CROSS_PREFIX}clang \
    --cxx=${ANDROID_CROSS_PREFIX}clang++ \
    --strip=$TOOLCHAIN/arm-linux-androideabi-strip \
    --sysroot=$SYSROOT \
    --disable-static \
    --disable-doc \
    --disable-asm \
    --disable-x86asm \
    --disable-symver \
    --disable-stripping \
    --disable-debug \
    --fatal-warnings \
    --enable-gpl \
    --enable-version3 \
    --enable-neon \
    --enable-nonfree \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-indevs \
    --enable-pic \
    --enable-jni \
    --enable-shared \

    echo "----------------configure finish!--------------------"
    make clean all
    make -j8
    make install
    echo "build finish:------->$PREFIX"

}

build


