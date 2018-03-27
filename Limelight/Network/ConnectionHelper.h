//
//  ConnectionHelper.h
//  Moonlight macOS
//
//  Created by Felix on 22.03.18.
//  Copyright © 2018 Felix. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AppListResponse.h"

#ifndef ConnectionHelper_h
#define ConnectionHelper_h


@interface ConnectionHelper : NSObject

+(AppListResponse*) getAppListForHostWithHostIP:(NSString*) hostIP deviceName:(NSString*)deviceName cert:(NSData*) cert uniqueID:(NSString*) uniqueId;

@end

#endif /* ConnectionHelper_h */
