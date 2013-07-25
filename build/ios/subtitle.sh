#!/bin/bash

#CC=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/llvm-g++
CC=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
LIBTOOL=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/libtool
LIPO=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo
CFLAGS="--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -DOS_IOS -DNDEBUG -Wno-deprecated-declarations"
INCLUDES="-I../../player/subtitle/libass -I../../player"
#-DNDEBUG
OUTPUTPATH=../../output/ios/v7
SUBTITLEPATH=../../player/subtitle

rm *.o

$CC -c \
	$CFLAGS \
	$INCLUDES \
    $SUBTITLEPATH/simpletextsubtitle.cpp \
    $SUBTITLEPATH/stssegment.cpp \
    $SUBTITLEPATH/subtitle.cpp

$CC -c \
    $CFLAGS \
    $SUBTITLEPATH/libass_glue.c \
    $SUBTITLEPATH/libass/ass.c \
    $SUBTITLEPATH/libass/ass_library.c

$LIBTOOL \
	-o $OUTPUTPATH/libsubtitle.a \
    libass_glue.o \
    simpletextsubtitle.o \
    stssegment.o \
    subtitle.o \
    ass.o \
    ass_library.o

echo "roger done"
