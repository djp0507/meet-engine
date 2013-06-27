/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
	 
#ifndef FF_IOS_LOG_H
#define FF_IOS_LOG_H

#include <stdarg.h>

#ifdef NDEBUG
#define LOG_NDEBUG 1
#else
#define LOG_NDEBUG 0
#endif
//#endif

#if LOG_NDEBUG
#define LOGV(...) ((void)0)
#else
#define LOGV(...)((void)0)
#endif

#if LOG_NDEBUG
#define LOGD(...) ((void)0)
#else
#define LOGD(...)((void)0)
#endif

#define LOGI(...) ((void)0)

#define LOGE(...) ((void)0)

#if __cplusplus
extern "C" {
#endif
    
void setFFmpegLogCallback(void* avcl, int level, const char* fmt, va_list vl);

#if __cplusplus
}
#endif

#endif
