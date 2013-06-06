#!/bin/bash

NDK=D:/Android/android-ndk-r8b

CC=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/windows/bin/arm-linux-androideabi-g++
AR=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/windows/bin/arm-linux-androideabi-ar
CFLAGS="--sysroot=$NDK/platforms/android-9/arch-arm -shared -fPIC -D__STDC_CONSTANT_MACROS -DNDEBUG -DOS_ANDROID -Wno-deprecated-declarations -Wno-psabi"
INCLUDES="-I../../platform -I../../player -I../../foundation/foundation"
LIBSPATH="-L../../foundation/output/android/v7_neon"
LIBS="-lavformat -lavcodec -lswscale -lavutil -lswresample -llog -lz"
EXTRA_LDFLAGS="-Wl,--fix-cortex-a8 -fno-exceptions -fno-rtti"

OUTPUT=../../output/android/libplayer.so
PLATFORMPATH=../../platform
PLAYERPATH=../../player


$CC \
	-o $OUTPUT \
	$CFLAGS \
	$INCLUDES \
	$EXTRA_LDFLAGS \
	$PLATFORMPATH/packetqueue.cpp \
	$PLATFORMPATH/list.cpp \
	$PLATFORMPATH/loop.cpp \
	$PLATFORMPATH/utils.cpp \
	$PLATFORMPATH/audiotrack_android.cpp \
	$PLATFORMPATH/surface_android.cpp \
	$PLATFORMPATH/log_android.c \
	$PLATFORMPATH/i420_rgb.S \
	$PLATFORMPATH/nv12_rgb.S \
	$PLATFORMPATH/nv21_rgb.S \
	$PLAYERPATH/ffstream.cpp \
	$PLAYERPATH/audioplayer.cpp \
	$PLAYERPATH/ffplayer.cpp \
	$LIBSPATH \
	$LIBS

echo "roger done"