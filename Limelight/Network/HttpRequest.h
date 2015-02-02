//
//  HttpRequest.h
//  Limelight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HttpResponse.h"

@interface HttpRequest : NSObject

@property (nonatomic) id<Response> response;
@property (nonatomic) NSURLRequest* request;

+ (instancetype) requestForResponse:(id<Response>)response withUrlRequest:(NSURLRequest*)req;
+ (instancetype) requestWithUrlRequest:(NSURLRequest*)req;

@end
