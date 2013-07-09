/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
	 
#ifndef FF_IOS_LOG_H
#define FF_IOS_LOG_H

#import <stdarg.h>
#import <stdio.h>

#ifdef NDEBUG
#define LOG_NDEBUG 1
#else
#define LOG_NDEBUG 0
#endif
//#endif

#if LOG_NDEBUG
#define LOGV(...) ((void)0)
#else
#define LOGV(...) __ios_log(__VA_ARGS__)
#endif

#if LOG_NDEBUG
#define LOGD(...) ((void)0)
#else
#define LOGD(...) __ios_log(__VA_ARGS__)
#endif

#define LOGI(...) __ios_log(__VA_ARGS__)

#define LOGE(...) __ios_log(__VA_ARGS__)

#if __cplusplus
extern "C" {
#endif

void __ios_log(const char *fmt, ...);
void setFFmpegLogCallback(void* avcl, int level, const char* fmt, va_list vl);

#if __cplusplus
}
#endif

#endif
