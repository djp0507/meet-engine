1. download source from http://ffmpeg.org/download.html#releases
2. refine configure line 2074
3. refine config-ffmpeg.sh to add "--disable-decoder=webvtt"
3. ./config-ffmpeg.sh neon
4. refine config.h line 8 