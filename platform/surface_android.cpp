/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#define LOG_TAG "SurfaceWrapper"

#include <dlfcn.h>
#include <android/native_window.h>
#include <jni.h>
#include <android/native_window_jni.h>
#include "errors.h"
#include "log.h"
#include "surface.h"

#define ANDROID_SYM_S_LOCK "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEb"
#define ANDROID_SYM_S_LOCK2 "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionE"
#define ANDROID_SYM_S_UNLOCK "_ZN7android7Surface13unlockAndPostEv"


typedef struct _SurfaceInfo {
    uint32_t    w;
    uint32_t    h;
    uint32_t    s;
    uint32_t    usage;
    uint32_t    format;
    uint32_t*   bits;
    uint32_t    reserved[2];
} SurfaceInfo;

typedef status_t (*Surface_lock)(void *, void *, int);
typedef status_t (*Surface_lock2)(void *, void *, void *);
typedef status_t (*Surface_unlockAndPost)(void *);
typedef ANativeWindow* (*ptr_ANativeWindow_fromSurface)(JNIEnv*, jobject);
typedef void (*ptr_ANativeWindow_release)(ANativeWindow*);
typedef int32_t (*ptr_ANativeWindow_lock)(ANativeWindow*, ANativeWindow_Buffer*, ARect*);

extern JavaVM* gs_jvm;
extern jobject gs_androidsurface;

static void* p_library = NULL;
static void* sSurface = NULL;
static Surface_lock s_lock = NULL;
static Surface_lock2 s_lock2 = NULL;
static ANativeWindow* window = NULL;
static ptr_ANativeWindow_fromSurface s_winFromSurface = NULL;
static ptr_ANativeWindow_release s_winRelease = NULL;
static ptr_ANativeWindow_lock s_winLock = NULL;
static Surface_unlockAndPost s_unlockAndPost = NULL;

static inline status_t LoadSurface(const char *psz_lib)
{
    p_library = dlopen(psz_lib, RTLD_NOW);
    if (p_library)
    {
        LOGI("Load lib %s success", psz_lib);
        s_lock = (Surface_lock)(dlsym(p_library, ANDROID_SYM_S_LOCK));
        s_lock2 = (Surface_lock2)(dlsym(p_library, ANDROID_SYM_S_LOCK2));
        s_unlockAndPost =
            (Surface_unlockAndPost)(dlsym(p_library, ANDROID_SYM_S_UNLOCK));
        if ((s_lock ||s_lock2) && s_unlockAndPost)
        {
            return OK;
        }
        else
        {
            LOGE("lib %s does not provide required functions", psz_lib);
        }
        s_lock = NULL;
        s_lock2 = NULL;
        s_unlockAndPost = NULL;
        dlclose(p_library);
    }
    else
    {
        LOGE("Load lib %s failed", psz_lib);
    }
    return ERROR;
}

static inline status_t LoadANativeWindow(const char *psz_lib)
{
    p_library = dlopen(psz_lib, RTLD_NOW);
    if (p_library)
    {
        LOGI("Load lib %s success", psz_lib);
		s_winFromSurface = (ptr_ANativeWindow_fromSurface)(dlsym(p_library, "ANativeWindow_fromSurface"));
		s_winRelease = (ptr_ANativeWindow_release)(dlsym(p_library, "ANativeWindow_release"));
		s_winLock = (ptr_ANativeWindow_lock)(dlsym(p_library, "ANativeWindow_lock"));
		s_unlockAndPost = (Surface_unlockAndPost)(dlsym(p_library, "ANativeWindow_unlockAndPost"));

		if (s_winFromSurface && s_winRelease && s_winLock && s_unlockAndPost)
		{
        	if(gs_androidsurface && gs_jvm)
        	{
        	    JNIEnv* env = NULL;
                if (gs_jvm->GetEnv((void**) &env, JNI_VERSION_1_4) == JNI_OK)
                {
            	    window = s_winFromSurface(env, gs_androidsurface);
                    if(window)
                    {
                        sSurface = window;
                        return OK;
                    }
                    else
                    {
            		    LOGE("failed to get window");
                    }
                }
                else
                {
        		    LOGE("GetEnv failed");
                }
        	}
        	else
        	{
        		LOGE("java surface not ready");
        	}
		}
        else
        {
            LOGE("lib %s does not provide required functions", psz_lib);
        }
        
	    s_winFromSurface = NULL;
	    s_winRelease = NULL;
	    s_winLock = NULL;
	    s_unlockAndPost = NULL;
   		dlclose(p_library);
    }
    else
    {
        LOGE("Load lib %s failed", psz_lib);
    }
    return ERROR;
}

static status_t InitLibrary()
{
    if(LoadANativeWindow("libandroid.so") == OK)
        return OK;
    if(LoadSurface("libsurfaceflinger_client.so") == OK)
        return OK;
    if(LoadSurface("libgui.so") == OK)
        return OK;
    if(LoadSurface("libui.so") == OK)
        return OK;
    return ERROR;
}
    
status_t Surface_open(void* surface)
{
	if(surface == NULL)
        return ERROR;
    sSurface = surface;
    return InitLibrary();
}

status_t Surface_getRes(uint32_t* width, uint32_t* height)
{
    SurfaceInfo info;

	if(s_winLock)
    {
        ANativeWindow_Buffer buf = { 0 };
        s_winLock(window, &buf, NULL);
        info.w = buf.width;
        info.h = buf.height;
        info.bits = buf.bits;
        info.s = buf.stride;
        info.format = buf.format;
    }
    else if(s_lock)
    {
        if(s_lock(sSurface, &info, 1) != OK)
        {
            LOGE("s_lock failed");
            return ERROR;
        }
    }
    else if(s_lock2)
    {
        if(s_lock2(sSurface, &info, NULL) != OK)
        {
            LOGE("s_lock2 failed");
            return ERROR;
        }
    }
	else 
	{
		LOGE("no available lock api");
		return ERROR;
	}

    *width = info.w;
    *height = info.h;
    LOGD("info.format:%d", info.format);

    if(s_unlockAndPost(sSurface) != OK)
    {
        LOGE("s_unlockAndPost failed");
        return ERROR;
    }
    
	return OK;
}

status_t Surface_getPixels(uint32_t* width, uint32_t* height, void** pixels)
{
    SurfaceInfo info;

	if(s_winLock)
    {
        ANativeWindow_Buffer buf = { 0 };
        s_winLock(window, &buf, NULL);
        info.w = buf.width;
        info.h = buf.height;
        info.bits = buf.bits;
        info.s = buf.stride;
        info.format = buf.format;
    }
    else if(s_lock)
    {
        if(s_lock(sSurface, &info, 1) != OK)
        {
            LOGE("s_lock failed");
            return ERROR;
        }
    }
    else if(s_lock2)
    {
        if(s_lock2(sSurface, &info, NULL) != OK)
        {
            LOGE("s_lock2 failed");
            return ERROR;
        }
    }
	else
	{
		LOGE("no available lock api");
		return ERROR;
	}

    if(width != NULL)
    {
        *width = info.w;
    }
    if(height != NULL)
    {
        *height = info.h;
    }
	*pixels = info.bits;
    
	return OK;
}

status_t Surface_updateSurface()
{
	// Surface inherit from ANativeWindow.
    if(s_unlockAndPost(sSurface) != OK)
    {
        LOGE("s_unlockAndPost failed");
        return ERROR;
    }
	return OK;
}

status_t Surface_close()
{
	if (window && s_winRelease)
	{
        s_winRelease(window);
	}
	if(p_library != NULL)
	{
		dlclose(p_library);
	}
	LOGD("unregistered");
    return OK;
}