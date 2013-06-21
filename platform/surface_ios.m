/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#define LOG_TAG "SurfaceWrapper"

#include <dlfcn.h>
#include "errors.h"
#include "log.h"
#include "surface.h"

#import <UIKit/UIKit.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <QuartzCore/QuartzCore.h>

#ifndef GL_UNSIGNED_SHORT_5_6_5
# define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif


static GLuint glTexture[1];
static uint8_t *glBuffer[1];

@interface OpenGLESVideoView : UIView
{
	EAGLContext * _context;
    GLuint _defaultFramebuffer;
	GLuint _colorRenderbuffer;
	BOOL _framebufferDirty;
    
    int _texPixelSize;
    int _texWidth;
    int _texHeight;
}
- (id)initWithFrame:(CGRect)frame;
@property EAGLContext * context;
@property GLuint colorRenderbuffer;
@property int texPixelSize;
@property int texWidth;
@property int texHeight;
- (void)cleanFramebuffer;
@end


static UIView *container;
static OpenGLESVideoView *glView;


static inline int GetAlignedSize(int i_size)
{
    /* Return the nearest power of 2 */
    int i_result = 1;
    while(i_result < i_size)
        i_result *= 2;

    return i_result;
}

#if __cplusplus
extern "C" {
#endif

status_t Surface_open(void* surface)
{
	if(surface == nil)
        return ERROR;
    
    NSAutoreleasePool *nsPool = nil;
    
    /* This will be released in Close(), on
     * main thread, after we are done using it. */
    container = (UIView *)surface;
    
    if (![container isKindOfClass:[UIView class]]) {
        LOGE("Container isn't an UIView, passing over.");
        goto error;
    }
    
    /* This will be released in Close(), on
     * main thread, after we are done using it. */
    [(UIView *)surface retain];

    /* Get our main view*/
    nsPool = [[NSAutoreleasePool alloc] init];

	LOGD("Creating OpenGLESVideoView");
    CGRect bounds = [container bounds];
	glView = [[OpenGLESVideoView alloc] initWithFrame:bounds];
    if (!glView)
        goto error;
    
    /* We don't wait, that means that we'll have to be careful about releasing
     * container.
     * That's why we'll release on main thread in Close(). */
	[container performSelectorOnMainThread:@selector(addSubview:) withObject:glView waitUntilDone:NO];

    [nsPool drain];
    nsPool = nil;
    
    glView.texPixelSize=2;
    int width = (int)bounds.size.width;
    int height = (int)bounds.size.height;
    /* A texture must have a size aligned on a power of 2 */
    glView.texWidth = GetAlignedSize(width);
    glView.texHeight = GetAlignedSize(height);
    
    glTexture[0] = 0;
	glBuffer[0] = 0;
    glGenTextures(1, glTexture);
    glBindTexture(GL_TEXTURE_2D, glTexture);

    /* TODO memalign would be way better */
    glBuffer[0] = malloc(glView.texWidth * glView.texHeight * glView.texPixelSize);
    /* Call glTexImage2D only once, and use glTexSubImage2D later */
	glTexImage2D(GL_TEXTURE_2D,
	    0,
	    GL_RGB, 
	    glView.texWidth,
	    glView.texHeight,
	    0,
	    GL_RGB,
	    GL_UNSIGNED_SHORT_5_6_5,
	    glBuffer[0]);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    return OK;
    
error:
    [nsPool release];
    Surface_close();
    return ERROR;
}

status_t Surface_displayPicture(FFPicture* picture)
{
    [glView cleanFramebuffer];

    glTexSubImage2D(GL_TEXTURE_2D,
		0,
		0, 0,
		picture->width,
		picture->height,
		GL_RGB,
		GL_UNSIGNED_SHORT_5_6_5,
		picture->data);
    
    const float f_normw = picture->width;
    const float f_normh = picture->height;

    float f_x = 0;
    float f_y = 0;
    float f_width = picture->width;
    float f_height = picture->height;

    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.i
       Currently, the OS X provider uses it to get a smooth window resizing */


    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);

	static const GLfloat vertexCoord[] = {
        -1.0f, -1.0f,
		 1.0f, -1.0f,
        -1.0f,  1.0f,
		 1.0f,  1.0f,
    };

    const GLfloat textureCoord[8] = {
        f_x,     f_height,
        f_width, f_height,
        f_x,     f_y,
        f_width, f_y
    };

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vertexCoord);
    glTexCoordPointer(2, GL_FLOAT, 0, textureCoord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisable(GL_TEXTURE_2D);
    
	[glView.context presentRenderbuffer:GL_RENDERBUFFER_OES];
	return OK;
}

status_t Surface_close()
{
    /* release on main thread as explained in Open() */
    [(id)container performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    [glView performSelectorOnMainThread:@selector(removeFromSuperview) withObject:nil waitUntilDone:NO];

    [glView release];

    glFinish();
    glFlush();
    glDeleteTextures(1, glTexture);
    free(glBuffer[0]);

    return OK;
}
    
    
#if __cplusplus
}
#endif

/*****************************************************************************
 * Our UIView object
 * *ALL* OpenGL calls should happen in the render thread
 *****************************************************************************/

@interface OpenGLESVideoView (Private)
- (void)_createFramebuffer;
- (void)_updateViewportWithBackingWitdh:(GLuint)backingWidth andBackingHeight:(GLuint)backingHeight;
- (void)_destroyFramebuffer;
@end

@implementation OpenGLESVideoView
@synthesize context=_context;
@synthesize colorRenderbuffer=_colorRenderbuffer;
@synthesize texPixelSize=_texPixelSize;
@synthesize texWidth=_texWidth;
@synthesize texHeight=_texHeight;

#define VLCAssertMainThread() assert([[NSThread currentThread] isMainThread])

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

/**
 * Gets called by the Open() method.
 */

- (id)initWithFrame:(CGRect)frame {
	if (self = [super initWithFrame:frame]) {
		//_vd = vd;
		CAEAGLLayer * eaglLayer = (CAEAGLLayer *)self.layer;

        eaglLayer.opaque = TRUE;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
//										[NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking,
										kEAGLColorFormatRGB565, kEAGLDrawablePropertyColorFormat,
										nil];

		_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
		NSAssert(_context && [EAGLContext setCurrentContext:_context], @"Creating context");

        // This shouldn't need to be done on the main thread.
        // Indeed, it works just fine from the render thread on iOS 3.2 to 4.1
        // However, if you don't call it from the main thread, it doesn't work on iOS 4.2 beta 1
        [self performSelectorOnMainThread:@selector(_createFramebuffer) withObject:nil waitUntilDone:YES];

		_framebufferDirty = NO;

		[self setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
	}
    return self;
}

- (void) dealloc
{
    [_context release];
    [super dealloc];
}


/**
 * Method called by UIKit when we have been resized
 */
- (void)layoutSubviews {
	// CAUTION : This is called from the main thread
	_framebufferDirty = YES;
}

- (void)cleanFramebuffer {
	if (_framebufferDirty) {
		[self _destroyFramebuffer];
		[self _createFramebuffer];
		_framebufferDirty = NO;
	}
}

@end

@implementation OpenGLESVideoView (Private)
- (void)_createFramebuffer {
	LOGD("Creating framebuffer for layer %p with bounds (%.1f,%.1f,%.1f,%.1f)", self.layer, self.layer.bounds.origin.x, self.layer.bounds.origin.y, self.layer.bounds.size.width, self.layer.bounds.size.height);
	[EAGLContext setCurrentContext:_context];
	// Create default framebuffer object. The backing will be allocated for the current layer in -resizeFromLayer
	glGenFramebuffersOES(1, &_defaultFramebuffer); // Generate one framebuffer, store it in _defaultFrameBuffer
	glGenRenderbuffersOES(1, &_colorRenderbuffer);
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, _defaultFramebuffer);
	glBindRenderbufferOES(GL_RENDERBUFFER_OES, _colorRenderbuffer);

	// This call associates the storage for the current render buffer with the EAGLDrawable (our CAEAGLLayer)
	// allowing us to draw into a buffer that will later be rendered to screen wherever the layer is (which corresponds with our view).
	[_context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(id<EAGLDrawable>)self.layer];
	glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, _colorRenderbuffer);

	GLuint backingWidth, backingHeight;
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

	if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
		LOGE("Failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
	}
	[self _updateViewportWithBackingWitdh:backingWidth andBackingHeight:backingHeight];
}

- (void)_updateViewportWithBackingWitdh:(GLuint)backingWidth andBackingHeight:(GLuint)backingHeight {
	LOGD("Reshaping to %dx%d", backingWidth, backingHeight);

	CGFloat width = (CGFloat)backingWidth;
	CGFloat height = (CGFloat)backingHeight;

    GLint x = width, y = height;
/*
	if (_vd) {
		CGFloat videoHeight = _vd->source.i_visible_height;
		CGFloat videoWidth = _vd->source.i_visible_width;

		GLint sarNum = _vd->source.i_sar_num;
		GLint sarDen = _vd->source.i_sar_den;

		if (height * videoWidth * sarNum < width * videoHeight * sarDen)
		{
			x = (height * videoWidth * sarNum) / (videoHeight * sarDen);
			y = height;
		}
		else
		{
			x = width;
			y = (width * videoHeight * sarDen) / (videoWidth * sarNum);
		}
	}
*/
	[EAGLContext setCurrentContext:_context];
	glViewport((width - x) / 2, (height - y) / 2, x, y);
}

- (void)_destroyFramebuffer {
	[EAGLContext setCurrentContext:_context];
	glDeleteFramebuffersOES(1, &_defaultFramebuffer);
	_defaultFramebuffer = 0;
	glDeleteRenderbuffersOES(1, &_colorRenderbuffer);
	_colorRenderbuffer = 0;
}
@end
