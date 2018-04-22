//
//  NetworkTraffic.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 28.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import "NetworkTraffic.h"
#include <ifaddrs.h>
#include <net/if.h>

struct ifaddrs *ifap, *ifa;
unsigned long da;

unsigned long getBytesDown() {
    da = 0;
    getifaddrs (&ifap);
    ifa = ifap;
    while (ifa != NULL) {
        if (ifa->ifa_addr->sa_family == AF_LINK) {
            const struct if_data *ifa_data = (struct if_data *)ifa->ifa_data;
            if (ifa_data != NULL) {
                da += ifa_data->ifi_ibytes;
            }
        }
        ifa = ifa->ifa_next;
    }
    
    freeifaddrs(ifap);
    return da;
}

unsigned long getBytesUp() {
    da = 0;
    getifaddrs (&ifap);
    ifa = ifap;
    while (ifa != NULL) {
        if (ifa->ifa_addr->sa_family == AF_LINK) {
            const struct if_data *ifa_data = (struct if_data *)ifa->ifa_data;
            if (ifa_data != NULL) {
                da += ifa_data->ifi_obytes;
            }
        }
        ifa = ifa->ifa_next;
    }
    
    freeifaddrs(ifap);
    return da;
}
