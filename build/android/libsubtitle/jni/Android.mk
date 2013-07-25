LOCAL_PATH := $(call my-dir)

PLAYER_BASE := ../../../../player
SUBTITLE_BASE := $(PLAYER_BASE)/subtitle

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

