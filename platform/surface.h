/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#ifndef SURFACE_WRAPPER_H
#define SURFACE_WRAPPER_H

#include <stdint.h>
#include "libffplayer/ffplayer/errors.h"

status_t Surface_open(void* surface);

status_t Surface_getRes(uint32_t* width, uint32_t* height);

status_t Surface_getPixels(uint32_t* width, uint32_t* height, void** pixels);
	
status_t Surface_updateSurface(int32_t autoscale = true);
	
status_t Surface_close();

#endif

