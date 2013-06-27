/*
 * Copyright (C) 2013 Roger Shen  rogershen@pptv.com
 *
 */

#import <UIKit/UIKit.h>

@interface GLView : UIView

- (id) initWithFrame:(CGRect)frame
               width:(uint32_t)width height:(uint32_t)height format:(uint32_t)format;

- (void) render: (void *) frame;

@end
