#include "log_android.h"

extern "C" {
#include "libavformat/avformat.h"
} 


void setFFmpegLogCallback(void* avcl, int level, const char* fmt, va_list vl)
{
    AVClass* avc = avcl ? *(AVClass**)avcl : NULL;
    char msg[1024];
	vsnprintf(msg, sizeof(msg), fmt, vl);
    int32_t androidLevel = ANDROID_LOG_UNKNOWN;
	switch(level)
    {
		case AV_LOG_PANIC:
            androidLevel = ANDROID_LOG_FATAL;
			break;
		case AV_LOG_FATAL:
            androidLevel = ANDROID_LOG_FATAL;
			break;
		case AV_LOG_ERROR:
            androidLevel = ANDROID_LOG_ERROR;
			break;
		case AV_LOG_WARNING:
            androidLevel = ANDROID_LOG_WARN;
			break;
			
		case AV_LOG_INFO:
            androidLevel = ANDROID_LOG_INFO;
			break;
			
		case AV_LOG_DEBUG:
            androidLevel = ANDROID_LOG_DEBUG;
			break;

		case AV_LOG_VERBOSE:
            androidLevel = ANDROID_LOG_VERBOSE;
			break;
			
	}
	__android_log_print(androidLevel, "ffmpeg", "[%s]%s", avc!=NULL?avc->class_name:"", msg);
}

