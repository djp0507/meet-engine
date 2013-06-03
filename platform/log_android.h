/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
	 
#ifndef FF_ANDROID_LOG_H
#define FF_ANDROID_LOG_H

#include <stdarg.h>

#include <jni.h>
#include <android/log.h>

//#ifndef LOG_NDEBUG
#ifdef NDEBUG
#define LOG_NDEBUG 1
#else
#define LOG_NDEBUG 0
#endif
//#endif

#if LOG_NDEBUG
#define LOGV(...) ((void)0)
#else
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE,LOG_TAG,__VA_ARGS__)
#endif

#if LOG_NDEBUG
#define LOGD(...) ((void)0)
#else
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#endif

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

void setFFmpegLogCallback(void* avcl, int level, const char* fmt, va_list vl);

#endif
