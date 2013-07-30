#! /usr/bin/env bash

NDK=${1:-$NDK_HOME}

ENGINE_BASE="../../.."
PLAYER_BASE="$ENGINE_BASE/player"
SUBTITLE_BASE="$PLAYER_BASE/subtitle"

SYSROOT=$NDK/platforms/android-9/arch-arm

case `uname -sm` in
	Linux\ x86*)
		HOST_TAG="linux-x86"
		;;
	CYGWIN*)
		HOST_TAG="windows"
		;;
	*)
		echo "Unkown Platform: `uname -sm`"
		exit -1
		;;
esac

TOOLCHAINS_BASE="$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/$HOST_TAG/bin"
CC="$TOOLCHAINS_BASE/arm-linux-androideabi-gcc --sysroot=$SYSROOT "
CXX="$TOOLCHAINS_BASE/arm-linux-androideabi-g++ --sysroot=$SYSROOT "
AR="$TOOLCHAINS_BASE/arm-linux-androideabi-ar "

CFLAGS=" -fpic -ffunction-sections -funwind-tables -fstack-protector -no-canonical-prefixes -march=armv5te -mtune=xscale -msoft-float -mthumb -Os -g -DNDEBUG -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 "
CXXFLAGS=" -fpic -ffunction-sections -funwind-tables -fstack-protector -no-canonical-prefixes -march=armv5te -mtune=xscale -msoft-float -fno-exceptions -fno-rtti -mthumb -Os -g -DNDEBUG -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 "
INCLUDES=" -I$PLAYER_BASE -I$SUBTITLE_BASE -I$SUBTITLE_BASE/libass -I$NDK/sources/cxx-stl/stlport/stlport -I$NDK/sources/cxx-stl/gabi++/include "
EXTRA_CFLAGS=" -DANDROID -Wa,--noexecstack "
EXTRA_CXXFLAGS=" -DANDROID -Wa,--noexecstack -frtti "
EXTRA_INCLUDES=" -I$NDK/platforms/android-9/arch-arm/usr/include "

CPP_SOURCES="$SUBTITLE_BASE/simpletextsubtitle $SUBTITLE_BASE/stssegment $SUBTITLE_BASE/subtitle"
for SOURCE in $CPP_SOURCES;
do
	echo "Compile++ : `echo $SOURCE | sed 's/.*\///g'`.cpp"
	$CXX \
		$CXXFLAGS \
		$INCLUDES \
		$EXTRA_CXXFLAGS \
		$EXTRA_INCLUDES \
		-c $SOURCE.cpp \
		-o `echo $SOURCE | sed 's/.*\///g'`.o
done

C_SOURCES="$SUBTITLE_BASE/libass_glue $SUBTITLE_BASE/libass/ass $SUBTITLE_BASE/libass/ass_library"
for SOURCE in $C_SOURCES;
do
	echo "Compile : `echo $SOURCE | sed 's/.*\///g'`.c"
	$CC \
		$CFLAGS \
		$INCLUDES \
		$EXTRA_CFLAGS \
		$EXTRA_INCLUDES \
		-c $SOURCE.c \
		-o `echo $SOURCE | sed 's/.*\///g'`.o
done

OBJS="simpletextsubtitle.o stssegment.o subtitle.o libass_glue.o ass.o ass_library.o"
OUTPUT="../../output/android/libsubtitle.a"
echo "Static Library : libsubtitle.a"
$AR \
	crs \
	$OUTPUT \
	$OBJS
	
rm *.o
