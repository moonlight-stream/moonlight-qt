//
//  HttpRequest.m
//  Moonlight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "HttpRequest.h"
#import "HttpResponse.h"
#import "HttpManager.h"

@implementation HttpRequest

+ (HttpRequest*) requestForResponse:(id<Response>)response withUrlRequest:(NSURLRequest*)req {
    HttpRequest* request = [[HttpRequest alloc] init];
    request.request = req;
    request.response = response;
    return request;
}

+ (HttpRequest*) requestWithUrlRequest:(NSURLRequest*)req {
    HttpRequest* request = [[HttpRequest alloc] init];
    request.request = req;
    return request;
}

+ (HttpRequest*) requestForResponse:(id<Response>)response withUrlRequest:(NSURLRequest*)req fallbackError:(int)error fallbackRequest:(NSURLRequest*) fallbackReq {
    HttpRequest* request = [[HttpRequest alloc] init];
    request.request = req;
    request.response = response;
    request.fallbackError = error;
    request.fallbackRequest = fallbackReq;
    return request;
}

@end
