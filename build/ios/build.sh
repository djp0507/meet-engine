#!/bin/bash

CC=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/llvm-g++
AR=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/ar
LIPO=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo
CFLAGS="--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -DNDEBUG -DOS_IOS -Wno-deprecated-declarations"
INCLUDES="-I../../platform -I../../player -I../../foundation/foundation"

OUTPUTPATH=../../output/ios/v7
PLATFORMPATH=../../platform
PLAYERPATH=../../player
FOUNDATIONPATH="../../foundation/output/ios/v7"

rm *.o *.a

$CC -c \
	$CFLAGS \
	$INCLUDES \
	$PLATFORMPATH/packetqueue.cpp \
	$PLATFORMPATH/list.cpp \
	$PLATFORMPATH/loop.cpp \
	$PLATFORMPATH/utils.cpp \
	$PLATFORMPATH/audiotrack_ios.c \
	$PLATFORMPATH/surface_ios.m \
	$PLATFORMPATH/log_ios.c \
	$PLAYERPATH/ffstream.cpp \
	$PLAYERPATH/audioplayer.cpp \
	$PLAYERPATH/ffplayer.cpp
	
$AR rcs \
	$OUTPUTPATH/libplayer.a \
	packetqueue.o \
	list.o \
	loop.o \
	utils.o \
	audiotrack_ios.o \
	surface_ios.o \
	log_ios.o \
	ffstream.o \
	audioplayer.o \
	ffplayer.o \
	$FOUNDATIONPATH/libavformat.a \
	$FOUNDATIONPATH/libavcodec.a \
	$FOUNDATIONPATH/libswscale.a \
	$FOUNDATIONPATH/libavutil.a \
	$FOUNDATIONPATH/libswresample.a


echo "roger done"
