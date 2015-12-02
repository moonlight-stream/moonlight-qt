//
//  AppListResponse.m
//  Moonlight
//
//  Created by Diego Waxemberg on 2/1/15.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "AppListResponse.h"
#import "TemporaryApp.h"
#import "DataManager.h"
#import <libxml2/libxml/xmlreader.h>

@implementation AppListResponse {
    NSMutableSet* _appList;
}
@synthesize data, statusCode, statusMessage;

static const char* TAG_APP = "App";
static const char* TAG_APP_TITLE = "AppTitle";
static const char* TAG_APP_ID = "ID";
static const char* TAG_APP_IS_RUNNING = "IsRunning";

- (void)populateWithData:(NSData *)xml {
    self.data = xml;
    _appList = [[NSMutableSet alloc] init];
    [self parseData];
}

- (void) parseData {
    xmlDocPtr docPtr = xmlParseMemory([self.data bytes], (int)[self.data length]);
    if (docPtr == NULL) {
        Log(LOG_W, @"An error occured trying to parse xml.");
        return;
    }
    
    xmlNodePtr node = xmlDocGetRootElement(docPtr);
    if (node == NULL) {
        Log(LOG_W, @"No root XML element.");
        xmlFreeDoc(docPtr);
        return;
    }
    
    xmlChar* statusStr = xmlGetProp(node, (const xmlChar*)[TAG_STATUS_CODE UTF8String]);
    if (statusStr != NULL) {
        int status = [[NSString stringWithUTF8String:(const char*)statusStr] intValue];
        xmlFree(statusStr);
        self.statusCode = status;
    }
    
    xmlChar* statusMsgXml = xmlGetProp(node, (const xmlChar*)[TAG_STATUS_MESSAGE UTF8String]);
    NSString* statusMsg;
    if (statusMsgXml != NULL) {
        statusMsg = [NSString stringWithUTF8String:(const char*)statusMsgXml];
        xmlFree(statusMsgXml);
    }
    else {
        statusMsg = @"Server Error";
    }
    self.statusMessage = statusMsg;
    
    node = node->children;
    
    while (node != NULL) {
        //Log(LOG_D, @"node: %s", node->name);
        if (!xmlStrcmp(node->name, (xmlChar*)TAG_APP)) {
            xmlNodePtr appInfoNode = node->xmlChildrenNode;
            NSString* appName = @"";
            NSString* appId = nil;
            BOOL appIsRunning = NO;
            while (appInfoNode != NULL) {
                if (!xmlStrcmp(appInfoNode->name, (xmlChar*)TAG_APP_TITLE)) {
                    xmlChar* nodeVal = xmlNodeListGetString(docPtr, appInfoNode->xmlChildrenNode, 1);
                    if (nodeVal != NULL) {
                        appName = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
                        xmlFree(nodeVal);
                    } else {
                        appName = @"";
                    }
                } else if (!xmlStrcmp(appInfoNode->name, (xmlChar*)TAG_APP_ID)) {
                    xmlChar* nodeVal = xmlNodeListGetString(docPtr, appInfoNode->xmlChildrenNode, 1);
                    if (nodeVal != NULL) {
                        appId = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
                        xmlFree(nodeVal);
                    }
                } else if (!xmlStrcmp(appInfoNode->name, (xmlChar*)TAG_APP_IS_RUNNING)) {
                    xmlChar* nodeVal = xmlNodeListGetString(docPtr, appInfoNode->xmlChildrenNode, 1);
                    if (nodeVal != NULL) {
                        appIsRunning = [[[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding] isEqualToString:@"1"];
                        xmlFree(nodeVal);
                    } else {
                        appIsRunning = NO;
                    }
                }
                appInfoNode = appInfoNode->next;
            }
            if (appId != nil) {
                TemporaryApp* app = [[TemporaryApp alloc] init];
                app.name = appName;
                app.id = appId;
                app.isRunning = appIsRunning;
                [_appList addObject:app];
            }
        }
        node = node->next;
    }
    
    xmlFreeDoc(docPtr);
}

- (NSSet*) getAppList {
    return _appList;
}

- (BOOL) isStatusOk {
    return self.statusCode == 200;
}

@end
