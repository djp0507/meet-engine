#!/bin/bash

NDK=D:/Android/android-ndk-r8b
SYSROOT=$NDK/platforms/android-9/arch-arm
OUTPUT=../../prebuilt

CC=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/windows/bin/arm-linux-androideabi-gcc
AR=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/windows/bin/arm-linux-androideabi-ar
EXTRA_LDFLAGS="-Wl,--fix-cortex-a8 -fno-exceptions -fno-rtti -D__STDC_CONSTANT_MACROS -DNDEBUG"

rm *.o

$CC --sysroot=$SYSROOT -I../../ -I../../libffplayer/ffmpeg/ffmpeg $EXTRA_LDFLAGS -c \
	ffstream.cpp \
	packetqueue.cpp \
	list.cpp \
	loop.cpp \
	utils.cpp \
	audiotrack_android.cpp \
	surface_android.cpp \
	i420_rgb.S \
	nv12_rgb.S \
	nv21_rgb.S
$AR rcs \
	$OUTPUT/libengine.a ffstream.o packetqueue.o list.o loop.o utils.o audiotrack_android.o surface_android.o i420_rgb.o nv12_rgb.o nv21_rgb.o