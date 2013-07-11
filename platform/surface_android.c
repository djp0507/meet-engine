/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#define LOG_TAG "SurfaceWrapper"

#include <dlfcn.h>
#include "errors.h"
#include "log.h"
#include "surface.h"

#ifndef ANDROID_SYM_S_LOCK
# define ANDROID_SYM_S_LOCK "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEb"
#endif
#ifndef ANDROID_SYM_S_LOCK2
# define ANDROID_SYM_S_LOCK2 "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionE"
#endif
#ifndef ANDROID_SYM_S_UNLOCK
# define ANDROID_SYM_S_UNLOCK "_ZN7android7Surface13unlockAndPostEv"
#endif

/* */
typedef struct _SurfaceInfo {
    uint32_t    w;
    uint32_t    h;
    uint32_t    s;
    uint32_t    usage;
    uint32_t    format;
    uint32_t*   bits;
    uint32_t    reserved[2];
} SurfaceInfo;

// _ZN7android7Surface4lockEPNS0_11SurfaceInfoEb
typedef status_t (*Surface_lock)(void *, void *, int);
// _ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionE
typedef status_t (*Surface_lock2)(void *, void *, void *);
// _ZN7android7Surface13unlockAndPostEv
typedef status_t (*Surface_unlockAndPost)(void *);

static void* sSurface = NULL;
static void* p_library = NULL;
static Surface_lock s_lock = NULL;
static Surface_lock2 s_lock2 = NULL;
static Surface_unlockAndPost s_unlockAndPost = NULL;

static inline status_t LoadSurface(const char *psz_lib) {
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
	LOGD("registering video surface");
	if(surface == NULL)
        return ERROR;
    sSurface = surface;
    return InitLibrary();
}

status_t Surface_getRes(uint32_t* width, uint32_t* height)
{
    SurfaceInfo info;

    if (s_lock)
    {
        if(s_lock(sSurface, &info, 1) != OK)
        {
            LOGE("s_lock failed");
            return ERROR;
        }
    }
    else
    {
        if(s_lock2(sSurface, &info, NULL) != OK)
        {
            LOGE("s_lock2 failed");
            return ERROR;
        }
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

    if (s_lock)
    {
        if(s_lock(sSurface, &info, 1) != OK)
        {
            LOGE("s_lock failed");
            return ERROR;
        }
    }
    else
    {
        if(s_lock2(sSurface, &info, NULL) != OK)
        {
            LOGE("s_lock2 failed");
            return ERROR;
        }
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
    if(s_unlockAndPost(sSurface) != OK)
    {
        LOGE("s_unlockAndPost failed");
        return ERROR;
    }
	return OK;
}

status_t Surface_close()
{
	LOGD("unregistered");
    return OK;
}