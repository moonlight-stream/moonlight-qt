#include "macosnetwork.h"

#import <Foundation/Foundation.h>

bool isAirDropDiscoverable()
{
    @autoreleasepool {
        NSDictionary* preferences = [[NSUserDefaults standardUserDefaults]
            persistentDomainForName:@"com.apple.sharingd"];
        NSString* discoverableMode = preferences[@"DiscoverableMode"];

        return [discoverableMode isEqualToString:@"Contacts Only"] ||
               [discoverableMode isEqualToString:@"Everyone"];
    }
}
