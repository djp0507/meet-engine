LOCAL_PATH := $(call my-dir)

ENGINE_BASE := ../../../..
PLAYER_BASE := $(ENGINE_BASE)/player
SUBTITLE_BASE := $(PLAYER_BASE)/subtitle
JNI_BASE := ./

####################[libsubtitle]####################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	$(SUBTITLE_BASE)/simpletextsubtitle.cpp \
	$(SUBTITLE_BASE)/stssegment.cpp \
	$(SUBTITLE_BASE)/subtitle.cpp \
	$(SUBTITLE_BASE)/libass_glue.c \
	$(SUBTITLE_BASE)/libass/ass.c \
	$(SUBTITLE_BASE)/libass/ass_library.c 

LOCAL_C_INCLUDES := \
	$(PLAYER_BASE)/ \
	$(SUBTITLE_BASE)/ \
	$(SUBTITLE_BASE)/libass

LOCAL_MODULE := subtitle

include $(BUILD_STATIC_LIBRARY)

####################[libsubtitle-jni]####################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	$(JNI_BASE)/android_ppmedia_subtitle_SampleSubtitleParser.cpp

LOCAL_C_INCLUDES := \
	$(ENGINE_BASE)

LOCAL_STATIC_LIBRARIES := \
	libsubtitle

LOCAL_MODULE := subtitle-jni

include $(BUILD_SHARED_LIBRARY)

