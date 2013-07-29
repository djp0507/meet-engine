#define LOG_TAG "SubTitleDecoder-JNI"

#include <stdio.h>
#include <jni.h>

#include "player/subtitle.h"

namespace {

__BEGIN_DECLS

JNIEXPORT void JNICALL
Java_android_ppmedia_subtitle_SampleTitleParser_native_1init(JNIEnv* env, jobject thiz)
{
	ISubtitles* subtitle = NULL;
	if (!ISubtitles::create(&subtitle))
	{
		return;
	}
}

__END_DECLS

}
