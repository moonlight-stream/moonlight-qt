//
//  OverlayView.h
//  Moonlight macOS
//
//  Created by Felix Kratz on 01.04.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface OverlayView : NSView

- (id)initWithFrame:(NSRect)frame sender:(StreamView*)sender;
- (void)toggleOverlay:(int)codec;

@end
