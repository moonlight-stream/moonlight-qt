//
//  HttpResponse.m
//  Limelight
//
//  Created by Diego Waxemberg on 1/30/15.
//  Copyright (c) 2015 Limelight Stream. All rights reserved.
//

#import "HttpResponse.h"
#import "App.h"
#import <libxml2/libxml/xmlreader.h>

@implementation HttpResponse

+ (HttpResponse*) responseWithData:(NSData*)xml {
    HttpResponse* response = [[HttpResponse alloc] init];
    response.responseData = xml;
    
    xmlDocPtr docPtr = xmlParseMemory([xml bytes], (int)[xml length]);
    
    if (docPtr == NULL) {
        NSLog(@"ERROR: An error occured trying to parse xml.");
        return response;
    }
    
    xmlNodePtr rootNode = xmlDocGetRootElement(docPtr);
    if (rootNode == NULL) {
        NSLog(@"ERROR: No root XML element.");
        xmlFreeDoc(docPtr);
        return response;
    }
    
    xmlChar* statusStr = xmlGetProp(rootNode, (const xmlChar*)"status_code");
    if (statusStr != NULL) {
        int status = [[NSString stringWithUTF8String:(const char*)statusStr] intValue];
        xmlFree(statusStr);
        response.statusCode = status;
    }
    
    xmlChar* statusMsgXml = xmlGetProp(rootNode, (const xmlChar*)"status_message");
    NSString* statusMsg;
    if (statusMsgXml != NULL) {
        statusMsg = [NSString stringWithUTF8String:(const char*)statusMsgXml];
        xmlFree(statusMsgXml);
    }
    else {
        statusMsg = @"Server Error";
    }
    response.statusMessage = statusMsg;
    
    
    return response;
}

- (NSString*)parseStringTag:(NSString *)tag {
    xmlDocPtr docPtr = xmlParseMemory([self.responseData bytes], (int)[self.responseData length]);
    
    if (docPtr == NULL) {
        NSLog(@"ERROR: An error occured trying to parse xml.");
        return nil;
    }
    
    xmlNodePtr node = xmlDocGetRootElement(docPtr);
    
    // Check root status_code
    if (![self isStatusOk]) {
        NSLog(@"ERROR: Request returned with failure status");
        xmlFreeDoc(docPtr);
        return nil;
    }
    
    // Skip the root node
    node = node->children;
   
    NSString* value;
    while (node != NULL) {
        //NSLog(@"node: %s", node->name);
        if (!xmlStrcmp(node->name, (const xmlChar*)[tag UTF8String])) {
            xmlChar* nodeVal = xmlNodeListGetString(docPtr, node->xmlChildrenNode, 1);
            value = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
            xmlFree(nodeVal);
        }
        node = node->next;
    }
    //NSLog(@"xmlValue: %@", value);
    
    xmlFreeDoc(docPtr);
    return value;
}

- (BOOL)parseIntTag:(NSString *)tag value:(NSInteger*)value {
    NSString* stringVal = [self parseStringTag:tag];
    if (stringVal != nil) {
        *value = [stringVal integerValue];
        return true;
    } else {
        return false;
    }
}

- (BOOL) isStatusOk {
    return self.statusCode == 200;
}

- (BOOL) populateHost:(Host*)host {
    xmlDocPtr docPtr = xmlParseMemory([self.responseData bytes], (int)[self.responseData length]);
    
    if (docPtr == NULL) {
        NSLog(@"ERROR: An error occured trying to parse xml.");
        return false;
    }
    
    xmlNodePtr node = xmlDocGetRootElement(docPtr);
    
    // Check root status_code
    if (![self isStatusOk]) {
        NSLog(@"ERROR: Request returned with failure status");
        xmlFreeDoc(docPtr);
        return false;
    }
    
    // Skip the root node
    node = node->children;
    
    while (node != NULL) {
        //NSLog(@"node: %s", node->name);
        if (!xmlStrcmp(node->name, (const xmlChar*)"hostname")) {
            xmlChar* nodeVal = xmlNodeListGetString(docPtr, node->xmlChildrenNode, 1);
            host.name = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
            xmlFree(nodeVal);
        } else if (!xmlStrcmp(node->name, (const xmlChar*)"ExternalIP")) {
            xmlChar* nodeVal = xmlNodeListGetString(docPtr, node->xmlChildrenNode, 1);
            host.externalAddress = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
            xmlFree(nodeVal);
        } else if (!xmlStrcmp(node->name, (const xmlChar*)"LocalIP")) {
            xmlChar* nodeVal = xmlNodeListGetString(docPtr, node->xmlChildrenNode, 1);
            host.localAddress = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
            xmlFree(nodeVal);
        } else if (!xmlStrcmp(node->name, (const xmlChar*)"uniqueid")) {
            xmlChar* nodeVal = xmlNodeListGetString(docPtr, node->xmlChildrenNode, 1);
            host.uuid = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
            xmlFree(nodeVal);
        } else if (!xmlStrcmp(node->name, (const xmlChar*)"mac")) {
            xmlChar* nodeVal = xmlNodeListGetString(docPtr, node->xmlChildrenNode, 1);
            host.mac = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
            xmlFree(nodeVal);
        } else if (!xmlStrcmp(node->name, (const xmlChar*)"PairStatus")) {
            xmlChar* nodeVal = xmlNodeListGetString(docPtr, node->xmlChildrenNode, 1);
            host.pairState = [[[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding] isEqualToString:@"1"] ? PairStatePaired : PairStateUnpaired;
            xmlFree(nodeVal);
        }
        
        node = node->next;
    }
    
    xmlFreeDoc(docPtr);
    
    return true;
}

- (NSArray*) getAppList {
    xmlDocPtr docPtr = xmlParseMemory([self.responseData bytes], (int)[self.responseData length]);
    
    if (docPtr == NULL) {
        NSLog(@"ERROR: An error occured trying to parse xml.");
        return nil;
    }
    
    xmlNodePtr node = xmlDocGetRootElement(docPtr);
    
    // Check root status_code
    if (![self isStatusOk]) {
        NSLog(@"ERROR: Request returned with failure status");
        xmlFreeDoc(docPtr);
        return nil;
    }
    
    // Skip the root node
    node = node->children;
    
    NSMutableArray* appList = [[NSMutableArray alloc] init];
    
    while (node != NULL) {
        //NSLog(@"node: %s", node->name);
        if (!xmlStrcmp(node->name, (const xmlChar*)"App")) {
            xmlNodePtr appInfoNode = node->xmlChildrenNode;
            NSString* appName;
            NSString* appId;
            BOOL appIsRunning = NO;
            while (appInfoNode != NULL) {
                //NSLog(@"appInfoNode: %s", appInfoNode->name);
                if (!xmlStrcmp(appInfoNode->name, (const xmlChar*)"AppTitle")) {
                    xmlChar* nodeVal = xmlNodeListGetString(docPtr, appInfoNode->xmlChildrenNode, 1);
                    appName = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
                    xmlFree(nodeVal);
                } else if (!xmlStrcmp(appInfoNode->name, (const xmlChar*)"ID")) {
                    xmlChar* nodeVal = xmlNodeListGetString(docPtr, appInfoNode->xmlChildrenNode, 1);
                    appId = [[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding];
                    xmlFree(nodeVal);
                } else if (!xmlStrcmp(appInfoNode->name, (const xmlChar*)"IsRunning")) {
                    xmlChar* nodeVal = xmlNodeListGetString(docPtr, appInfoNode->xmlChildrenNode, 1);
                    appIsRunning = [[[NSString alloc] initWithCString:(const char*)nodeVal encoding:NSUTF8StringEncoding] isEqualToString:@"1"];
                    xmlFree(nodeVal);
                }
                appInfoNode = appInfoNode->next;
            }
            App* app = [[App alloc] init];
            app.appName = appName;
            app.appId = appId;
            app.isRunning = appIsRunning;
            [appList addObject:app];
        }
        node = node->next;
    }
    
    xmlFreeDoc(docPtr);
    
    return appList;
}


@end
