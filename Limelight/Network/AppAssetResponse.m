//
//  AppAssetResponse.m
//  Moonlight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "AppAssetResponse.h"

@implementation AppAssetResponse
@synthesize data, statusCode, statusMessage;

- (void)populateWithData:(NSData *)imageData {
    self.data = imageData;
    self.statusMessage = @"App asset has no status message";
    self.statusCode = -1;
}
- (OSImage*) getImage {
#if TARGET_OS_IPHONE
    OSImage* appImage = [[OSImage alloc] initWithData:self.data];
#else
    OSImage* appImage = nil;
#endif
    return appImage;
}

@end
