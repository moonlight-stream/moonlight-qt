//
//  TemporaryApp.h
//  Moonlight
//
//  Created by Cameron Gutman on 9/30/15.
//  Copyright © 2015 Moonlight Stream. All rights reserved.
//

#import "TemporaryHost.h"
#import "App.h"

@interface TemporaryApp : NSObject

@property (nullable, nonatomic, retain) NSString *id;
@property (nullable, nonatomic, retain) NSData *image;
@property (nullable, nonatomic, retain) NSString *name;
@property (nullable, nonatomic, retain) TemporaryHost *host;

NS_ASSUME_NONNULL_BEGIN

- (id) initFromApp:(App*)app withTempHost:(TemporaryHost*)tempHost;

- (NSComparisonResult)compareName:(TemporaryApp *)other;

- (void) propagateChangesToParent:(App*)parent withHost:(Host*)host;

NS_ASSUME_NONNULL_END

@end
