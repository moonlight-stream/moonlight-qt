//
//  ServerInfoResponse.h
//  Limelight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "HttpResponse.h"

@interface ServerInfoResponse : HttpResponse <Response>

- (void) populateWithData:(NSData *)data;
- (void) populateHost:(Host*)host;

@end
