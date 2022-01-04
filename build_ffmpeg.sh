#!/bin/bash
#设置交叉编译工具集目录
echo "NDK_TOOLCHAINS_HOME:$NDK_TOOLCHAINS_HOME"
ls "$NDK_TOOLCHAINS_HOME"
echo "----------------------------------------------------------------------------------"
echo "                           "
#交叉编译环境目录
SYSROOT=$NDK_TOOLCHAINS_HOME/sysroot
#编译工具链目录：
TOOLCHAIN=$NDK_TOOLCHAINS_HOME/bin

OS=android
API=21

CROSS_PREFIX=$TOOLCHAIN/$PLATFORM-
#armv7 arm64 x86 x86_64 ARCH=armv7
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


echo "ARCH:${ARCH}, ARCH2:${ARCH2}"

#保存目录
PREFIX=$(pwd)/build_android/$ARCH

echo "PREFIX:$PREFIX"
ls "$PREFIX"
rm  -rf "$PREFIX"
echo "------------------------------------"
echo "    "


echo "FFmpeg source dir:"
cd /Users/pcyfox/FFmpeg ||exit
pwd
echo "    "


echo "ToolChain ------->$TOOLCHAIN"
echo "ANDROID_CROSS_PREFIX------>$ANDROID_CROSS_PREFIX"
echo "CROSS_PREFIX------>$CROSS_PREFIX"
echo " "
echo "----------------configure start!--------------------"

build()
{
    make clean all
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
    --disable-x86asm \
    --disable-symver \
    --disable-stripping \
    --disable-debug \
    --fatal-warnings \
    --enable-gpl \
    --enable-version3 \
    --enable-asm \
    --enable-neon \
    --enable-nonfree \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-indevs \
    --enable-pic \
    --enable-jni \
    --enable-shared \
    --disable-decoders \
    --enable-mediacodec \
    --enable-decoder=aac \
    --enable-decoder=aac_latm \
    --enable-decoder=mjpeg \
    --enable-decoder=png \
    --enable-decoder=h264_mediacodec \
    --enable-decoder=mpeg4_mediacodec \

    echo "----------------configure finish!--------------------"
    make -j8
    make install

    echo "build finish:------->$PREFIX"
    ls "$PREFIX/lib"
    echo "------------------------------------"
}
build