//
//  AppListResponse.h
//  Moonlight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "HttpResponse.h"

@interface AppListResponse : NSObject <Response>

- (void)populateWithData:(NSData *)data;
- (NSArray*) getAppList;
- (BOOL) isStatusOk;

@end
