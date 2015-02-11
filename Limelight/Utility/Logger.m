//
//  Logger.m
//  Limelight
//
//  Created by Diego Waxemberg on 2/10/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "Logger.h"

void Log(LogLevel level, NSString* fmt, ...) {
    NSString* levelPrefix = @"";
    
    switch(level) {
        case LOG_D:
            levelPrefix = PRFX_DEBUG;
            break;
        case LOG_I:
            levelPrefix = PRFX_INFO;
            break;
        case LOG_W:
            levelPrefix = PRFX_WARN;
            break;
        case LOG_E:
            levelPrefix = PRFX_ERROR;
            break;
        default:
            break;
    }
    NSString* prefixedString = [NSString stringWithFormat:@"%@ %@", levelPrefix, fmt];
    va_list args;
    va_start(args, fmt);
    NSLogv(prefixedString, args);
    va_end(args);
}
