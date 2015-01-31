//
//  HttpResponse.h
//  Limelight
//
//  Created by Diego Waxemberg on 1/30/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Host.h"

@interface HttpResponse : NSObject

@property (nonatomic) NSInteger statusCode;
@property (nonatomic) NSString* statusMessage;
@property (nonatomic) NSData* responseData;

+ (HttpResponse*) responseWithData:(NSData*)xml;

- (NSString*) parseStringTag:(NSString*)tag;
- (BOOL)parseIntTag:(NSString *)tag value:(NSInteger*)value;
- (BOOL) isStatusOk;
- (BOOL) populateHost:(Host*)host;
- (NSArray*) getAppList;

@end
