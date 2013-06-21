/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#ifndef SURFACE_WRAPPER_H
#define SURFACE_WRAPPER_H

#include <stdint.h>
#include "errors.h"

#ifdef OS_IOS

#define USE_OPENGL_ES 1

typedef struct FFPicture {
#define AV_NUM_DATA_POINTERS 8
    uint8_t *data[AV_NUM_DATA_POINTERS];
    int linesize[AV_NUM_DATA_POINTERS];
    uint8_t **extended_data;
    int width, height;
    int nb_samples;
    int format;
}FFPicture;

#endif

#if __cplusplus
extern "C" {
#endif

status_t Surface_open(void* surface); 

status_t Surface_getRes(uint32_t* width, uint32_t* height);

status_t Surface_getPixels(uint32_t* width, uint32_t* height, void** pixels);
	
status_t Surface_updateSurface();

status_t Surface_displayPicture(FFPicture* picture);
	
status_t Surface_close();

#if __cplusplus
}
#endif
#endif

