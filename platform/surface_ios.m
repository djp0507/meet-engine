/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#define LOG_TAG "SurfaceWrapper"

#import <UIKit/UIKit.h>
#import <glview.h>

#if __cplusplus
extern "C" {
#endif
    
#include "libavformat/avformat.h"
    
#if __cplusplus
}
#endif

#include "errors.h"
#include "log.h"
#include "surface.h"


struct SurfaceWrapper
{
    UIView *container;
    GLView *glView;
    uint32_t frameWidth;
    uint32_t frameHeight;
    enum PixelFormat frameFormat;
    
};

struct SurfaceWrapper surfaceWrapper;


#if __cplusplus
extern "C" {
#endif
    

status_t Surface_open(void* surface, uint32_t frameWidth, uint32_t frameHeight, uint32_t frameFormat)
{
    if (surface == nil) {
        return ERROR;
    }
    surfaceWrapper.container = (UIView*)surface;
    [surfaceWrapper.container retain];
    surfaceWrapper.frameWidth = frameWidth;
    surfaceWrapper.frameHeight = frameHeight;
    surfaceWrapper.frameFormat = frameFormat;
    
    CGRect bounds = [surfaceWrapper.container bounds];
    surfaceWrapper.glView = [[GLView alloc] initWithFrame:bounds width:frameWidth height:frameHeight format:frameFormat];
    
    
    surfaceWrapper.glView.contentMode = UIViewContentModeScaleAspectFit;
    surfaceWrapper.glView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleBottomMargin;
    
    [surfaceWrapper.container performSelectorOnMainThread:@selector(addSubview:) withObject:surfaceWrapper.glView  waitUntilDone:YES];
    return OK;
}

status_t Surface_displayPicture(void* picture)
{
    if (surfaceWrapper.glView) {
        
        [surfaceWrapper.glView render:picture];
        
    }
    return OK;
}

status_t Surface_close()
{
    [surfaceWrapper.glView performSelectorOnMainThread:@selector(removeFromSuperview) withObject:nil waitUntilDone:NO];
    [surfaceWrapper.glView release];
    [(id)surfaceWrapper.container performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    return OK;
}
    
    
#if __cplusplus
}
#endif
