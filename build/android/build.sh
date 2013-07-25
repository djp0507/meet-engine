#!/bin/bash

NDK=D:/Android/android-ndk-r8b

CC=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/windows/bin/arm-linux-androideabi-g++
AR=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/windows/bin/arm-linux-androideabi-ar
INCLUDES="-I../../platform -I../../player -I../../foundation/foundation"
LIBS="-lavformat -lavcodec -lswscale -lavutil -lswresample -llog -lz"
PLATFORMPATH=../../platform
PLAYERPATH=../../player

case $1 in
	neon)
		CFLAGS="-DNDEBUG --sysroot=$NDK/platforms/android-9/arch-arm -shared -fPIC -mfloat-abi=softfp -mfpu=neon -mtune=cortex-a8 -mvectorize-with-neon-quad -march=armv7-a -fpermissive -D__STDC_CONSTANT_MACROS -DOS_ANDROID -Wno-deprecated-declarations -Wno-psabi"
		LIBSPATH="-L../../foundation/output/android/v7_neon"
		EXTRA_LDFLAGS="-Wl,--fix-cortex-a8 -fno-exceptions -fno-rtti"
		OUTPUT=../../output/android/libplayer_neon.so
		;;
	v6_vfp)
		CFLAGS="--sysroot=$NDK/platforms/android-9/arch-arm -shared -fPIC -mfloat-abi=softfp -mfpu=vfp -march=armv6 -fpermissive -D__STDC_CONSTANT_MACROS -DNDEBUG -DOS_ANDROID -Wno-deprecated-declarations -Wno-psabi"
		LIBSPATH="-L../../foundation/output/android/v6_vfp"
		EXTRA_LDFLAGS="-fno-exceptions -fno-rtti"
		OUTPUT=../../output/android/libplayer_v6_vfp.so
		;;
	*)
		echo Unknown target: $1
		exit
esac
# -DNDEBUG
$CC \
	-o $OUTPUT \
	$CFLAGS \
	$INCLUDES \
	$EXTRA_LDFLAGS \
	$PLATFORMPATH/cpu-features.c \
	$PLATFORMPATH/packetqueue.cpp \
	$PLATFORMPATH/list.cpp \
	$PLATFORMPATH/loop.cpp \
	$PLATFORMPATH/utils.cpp \
	$PLATFORMPATH/audiotrack_android.c \
	$PLATFORMPATH/surface_android.c \
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