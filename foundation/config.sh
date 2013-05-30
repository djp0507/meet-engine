#!/bin/bash

NDK=D:/Android/android-ndk-r8b

export TMPDIR=D:/cygwin/tmp

cd ffmpeg

case $1 in
	neon)
		ARCH=arm
		CPU=armv7-a
		EXTRA_CFLAGS="-mfloat-abi=softfp -mfpu=neon -mtune=cortex-a8 -mvectorize-with-neon-quad"
		EXTRA_LDFLAGS="-Wl,--fix-cortex-a8"
		EXTRA_PARAMETERS="--disable-debug --disable-fast-unaligned --disable-protocols --enable-protocol=http --enable-protocol=hls --enable-protocol=file"
		;;
	neon_debug)
		ARCH=arm
		CPU=armv7-a
		EXTRA_CFLAGS="-mfloat-abi=softfp -mfpu=neon -mtune=cortex-a8 -mvectorize-with-neon-quad"
		EXTRA_LDFLAGS="-Wl,--fix-cortex-a8"
		EXTRA_PARAMETERS="--enable-debug=1   --disable-fast-unaligned --disable-protocols --enable-protocol=http --enable-protocol=hls --enable-protocol=file"
		;;
	neon_cut)
		ARCH=arm
		CPU=armv7-a
		EXTRA_CFLAGS="-mfloat-abi=softfp -mfpu=neon -mtune=cortex-a8 -mvectorize-with-neon-quad"
		EXTRA_LDFLAGS="-Wl,--fix-cortex-a8"
		EXTRA_PARAMETERS="--disable-debug --disable-fast-unaligned --disable-protocols --enable-protocol=http --disable-demuxers --enable-demuxer=mov --enable-demuxer=hls --enable-demuxer=mpegts --disable-bsfs --enable-bsf=aac_adtstoasc --enable-bsf=h264_mp4toannexb --disable-parsers  --enable-parser=h264 --enable-parser=aac --disable-decoders --enable-decoder=h264 --enable-decoder=aac"
		;;
	tegra2)
		ARCH=arm
		CPU=armv7-a
		EXTRA_CFLAGS="-mfloat-abi=softfp -mfpu=vfpv3-d16"
		EXTRA_PARAMETERS="--disable-debug --disable-fast-unaligned --disable-protocols --enable-protocol=http --enable-protocol=hls --enable-protocol=file"
		;;
	v6_vfp)
		ARCH=arm
		CPU=armv6
		EXTRA_CFLAGS="-mfloat-abi=softfp -mfpu=vfp"
		EXTRA_PARAMETERS="--disable-debug --disable-fast-unaligned --disable-protocols --enable-protocol=http --enable-protocol=hls --enable-protocol=file"
		;;
	v6_vfp_cut)
		ARCH=arm
		CPU=armv6
		EXTRA_CFLAGS="-mfloat-abi=softfp -mfpu=vfp"
		EXTRA_PARAMETERS="--disable-debug --disable-fast-unaligned --disable-protocols --enable-protocol=http --enable-protocol=file --disable-demuxers --enable-demuxer=mov --enable-demuxer=hls --enable-demuxer=mpegts --disable-bsfs --enable-bsf=aac_adtstoasc --enable-bsf=h264_mp4toannexb --disable-parsers  --enable-parser=h264 --enable-parser=aac --disable-decoders --enable-decoder=h264 --enable-decoder=aac"
		;;
	v6_vfp_cut_debug)
		ARCH=arm
		CPU=armv6
		EXTRA_CFLAGS="-mfloat-abi=softfp -mfpu=vfp"
		EXTRA_PARAMETERS="--enable-debug=1 --disable-fast-unaligned --disable-protocols --enable-protocol=http --disable-demuxers --enable-demuxer=mov --enable-demuxer=hls --enable-demuxer=mpegts --disable-bsfs --enable-bsf=aac_adtstoasc --enable-bsf=h264_mp4toannexb --disable-parsers  --enable-parser=h264 --enable-parser=aac --disable-decoders --enable-decoder=h264 --enable-decoder=aac"
		;;
	v6)
		ARCH=arm
		CPU=armv6
		EXTRA_CFLAGS="-msoft-float"
		EXTRA_PARAMETERS="--disable-debug"
		;;
	v5te)
		ARCH=arm
		CPU=armv5te
		EXTRA_CFLAGS="-msoft-float -mtune=xscale"
		EXTRA_PARAMETERS="--disable-debug"
		;;
	x86)
		ARCH=x86
		CPU=i686
		EXTRA_CFLAGS=
		;;
	mips)
		ARCH=mips
		CPU=mips32r2
		EXTRA_CFLAGS=
		EXTRA_PARAMETERS="--disable-mipsdspr1 --disable-mipsdspr2"
		;;
	*)
		echo Unknown target: $1
		exit
esac

if [ $ARCH == 'arm' ] 
then
	CROSS_PREFIX=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/windows/bin/arm-linux-androideabi-
	EXTRA_CFLAGS="$EXTRA_CFLAGS -fstack-protector -fstrict-aliasing"
	OPTFLAGS="-O2"
elif [ $ARCH == 'x86' ] 
then
	CROSS_PREFIX=$NDK/toolchains/x86-4.6/prebuilt/windows/bin/i686-linux-android-
	EXTRA_CFLAGS="$EXTRA_CFLAGS -fstrict-aliasing"
	OPTFLAGS="-O2 -fno-pic"
	MXLIB_PATH="../libs/x86"
elif [ $ARCH == 'mips' ] 
then
	CROSS_PREFIX=$NDK/toolchains/mipsel-linux-android-4.6/prebuilt/windows/bin/mipsel-linux-android-
	EXTRA_CFLAGS="$EXTRA_CFLAGS -fno-strict-aliasing -fmessage-length=0 -fno-inline-functions-called-once -frerun-cse-after-loop -frename-registers"
	OPTFLAGS="-O2"
	MXLIB_PATH="../libs/mips"
fi

./configure \
--enable-optimizations \
--disable-doc \
--disable-symver \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-ffserver \
--disable-avdevice \
--disable-postproc \
--disable-avfilter \
--disable-swscale-alpha \
--disable-encoders \
--disable-muxers \
--disable-devices \
--disable-parser=dca \
--disable-filters \
--disable-demuxer=srt \
--disable-demuxer=microdvd \
--disable-demuxer=jacosub \
--disable-demuxer=sami \
--disable-demuxer=realtext \
--disable-demuxer=dts \
--disable-demuxer=subviewer \
--disable-decoder=ass \
--disable-decoder=srt \
--disable-decoder=subrip \
--disable-decoder=microdvd \
--disable-decoder=jacosub \
--disable-decoder=sami \
--disable-decoder=realtext \
--disable-decoder=dca \
--disable-decoder=movtext \
--disable-decoder=subviewer \
--disable-decoder=webvtt \
--enable-zlib \
--enable-pic \
--arch=$ARCH \
--cpu=$CPU \
--cross-prefix=$CROSS_PREFIX \
--enable-cross-compile \
--sysroot=$NDK/platforms/android-9/arch-$ARCH \
--target-os=linux \
--extra-cflags="-DNDEBUG -mandroid -ftree-vectorize -ffunction-sections -funwind-tables -fomit-frame-pointer -funswitch-loops -finline-limit=300 -finline-functions -fpredictive-commoning -fgcse-after-reload -fipa-cp-clone $EXTRA_CFLAGS" \
--extra-ldflags="$EXTRA_LDFLAGS" \
--optflags="$OPTFLAGS" \
$EXTRA_PARAMETERS

#--disable-debug \
#--enable-debug=1
#--disable-static \
#--enable-shared \


