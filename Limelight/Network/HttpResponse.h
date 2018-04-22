//
//  HttpResponse.h
//  Moonlight
//
//  Created by Diego Waxemberg on 1/30/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

static NSString* TAG_STATUS_CODE = @"status_code";
static NSString* TAG_STATUS_MESSAGE = @"status_message";

@protocol Response <NSObject>

- (void) populateWithData:(NSData*)data;

@property (nonatomic) NSInteger statusCode;
@property (nonatomic) NSString* statusMessage;
@property (nonatomic) NSData* data;

@end

@interface HttpResponse : NSObject <Response>

- (void) populateWithData:(NSData*)data;
- (void) parseData;
- (NSString*) getStringTag:(NSString*)tag;
- (BOOL) getIntTag:(NSString *)tag value:(NSInteger*)value;
- (BOOL) isStatusOk;

@end
