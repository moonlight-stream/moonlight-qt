//
//  Logger.h
//  Limelight
//
//  Created by Diego Waxemberg on 2/10/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#ifndef Limelight_Logger_h
#define Limelight_Logger_h

#import <stdarg.h>

typedef enum {
    LOG_D,
    LOG_I,
    LOG_W,
    LOG_E
} LogLevel;

#define PRFX_DEBUG @"<DEBUG>"
#define PRFX_INFO @"<INFO>"
#define PRFX_WARN @"<WARN>"
#define PRFX_ERROR @"<ERROR>"

void Log(LogLevel level, NSString* fmt, ...);

#endif
