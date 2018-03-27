//
//  OSPortabilityDefs.h
//  Moonlight
//
//  Created by Cameron Gutman on 3/26/18.
//  Copyright © 2018 Moonlight Stream. All rights reserved.
//

#ifndef OSPortabilityDefs_h
#define OSPortabilityDefs_h

#if TARGET_OS_IPHONE
#define OSImage UIImage
#define OSColor UIColor
#define OSView UIView
#define OSApplication UIApplication
#else
#define OSImage NSImage
#define OSColor NSColor
#define OSView NSView
#define OSApplication NSApplication
#endif

#endif /* OSPortabilityDefs_h */
