#! /usr/bin/env bash

NDK_BUILD=$NDK_HOME/ndk-build

WORKSPACE=`pwd`

if ! cd libsubtitle/jni || ! $NDK_BUILD -B;
then
	echo "Build libsubtitle failed!!!"
	exit -1
fi

cd $WORKSPACE

cp -vf libsubtitle/libs/armeabi/libsubtitle-jni.so ../../output/android

rm -rfv libsubtitle/libs/
rm -rfv libsubtitle/obj
