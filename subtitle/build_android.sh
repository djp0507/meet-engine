#! /usr/bin/env bash

if [ "$NDK"x = "x" ]; then
	echo "NDK path is not specified."
	exit -1
fi

case `uname -sm` in
	Linux\ x86*)
		HOST_TAG="linux-x86"
		;;
	CYGWIN*)
		HOST_TAG="windows"
		;;
	MINGW*)
		HOST_TAG="windows"
		;;
	*)
		echo "Unkown Platform: `uname -sm`"
		exit -1
		;;
esac

make PLATFORM=android HOST_TAG=$HOST_TAG $@
