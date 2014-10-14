//
//  MDNSManager.h
//  Limelight
//
//  Created by Diego Waxemberg on 10/14/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol MDNSCallback <NSObject>

- (void) updateHosts:(NSArray*)hosts;

@end

@interface MDNSManager : NSObject <NSNetServiceBrowserDelegate, NSNetServiceDelegate>

@property id<MDNSCallback> callback;

- (id) initWithCallback:(id<MDNSCallback>) callback;
- (void) searchForHosts;
- (NSArray*) getFoundHosts;

@end



