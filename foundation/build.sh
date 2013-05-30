#!/bin/bash

cd foundation

case $1 in
	neon)
		CONFIG_SRC=../config_neon
		TARGET1=../../output/v7_neon
		;;
	neon_debug)
		CONFIG_SRC=../config_neon_debug
		TARGET1=../../output/v7_neon_debug
		;;
	neon_cut)
		CONFIG_SRC=../config_neon_cut
		TARGET1=../../output/v7_neon_cut
		;;
	tegra2)
		CONFIG_SRC=../config_tegra2
		TARGET1=../../output/v7_vfpv3d16
		;;
	v6_vfp)
		CONFIG_SRC=../config_v6_vfp
		TARGET1=../../output/v6_vfp
		;;
	v6_vfp_cut)
		CONFIG_SRC=../config_v6_vfp_cut
		TARGET1=../../output/v6_vfp_cut
		;;
	v6_vfp_cut_debug)
		CONFIG_SRC=../config_v6_vfp_cut_debug
		TARGET1=../../output/v6_vfp_cut_debug
		;;
	v6)
		CONFIG_SRC=../config_v6
		TARGET1=../../output/v6
		;;
	v5te)
		CONFIG_SRC=../config_v5te
		TARGET1=../../output/v5te
		;;
esac

case $2 in
	clean)
		make clean
		cp $CONFIG_SRC/config.h . -f
		cp $CONFIG_SRC/config.mak . -f
		cp $CONFIG_SRC/config.fate . -f
		;;
esac

make

cp libavutil/libavutil.a $TARGET1/
cp libswresample/libswresample.a $TARGET1/
cp libavcodec/libavcodec.a $TARGET1/
cp libavformat/libavformat.a $TARGET1/
cp libswscale/libswscale.a $TARGET1/

cd ..
