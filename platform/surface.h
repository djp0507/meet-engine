/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#ifndef SURFACE_WRAPPER_H
#define SURFACE_WRAPPER_H

#include <stdint.h>
#include "errors.h"


#if __cplusplus
extern "C" {
#endif
    
#ifdef OS_ANDROID
status_t Surface_open(void* surface);
#endif

#ifdef OS_IOS
status_t Surface_open(void* surface, uint32_t frameWidth, uint32_t frameHeight, uint32_t frameFormat);
#endif

status_t Surface_getRes(uint32_t* width, uint32_t* height);

status_t Surface_getPixels(uint32_t* width, uint32_t* height, void** pixels);
	
status_t Surface_updateSurface();

status_t Surface_displayPicture(void* picture);
	
status_t Surface_close();

#if __cplusplus
}
#endif
#endif

