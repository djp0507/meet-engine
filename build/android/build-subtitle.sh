#! /usr/bin/env bash

NDK_BUILD=$NDK_HOME/ndk-build

if ! cd libsubtitle/jni || ! $NDK_BUILD -B;
then
	echo "Build libsubtitle failed!!!"
	exit -1
fi

cp ../obj/local/armeabi/libsubtitle.a ../../../../output/android
