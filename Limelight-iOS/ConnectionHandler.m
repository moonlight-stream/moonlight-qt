//
//  ConnectionHandler.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/27/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "ConnectionHandler.h"
#import "AppDelegate.h"
#import <AdSupport/ASIdentifierManager.h>
#import "MainFrameViewController.h"

#include <libxml/xmlreader.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

@implementation ConnectionHandler
static const int PAIR_MODE = 0x1;
static const int STREAM_MODE = 0x2;

static MainFrameViewController* viewCont;

+ (void)streamWithHost:(NSString *)host viewController:(MainFrameViewController *)viewController
{
    viewCont = viewController;
    ConnectionHandler* streamHandler = [[ConnectionHandler alloc] initForStream:host];
    [[AppDelegate getMainOpQueue] addOperation:streamHandler];
}

+ (void)pairWithHost:(NSString *)host
{
    ConnectionHandler* pairHandler = [[ConnectionHandler alloc] initForPair:host];
    [[AppDelegate getMainOpQueue]addOperation:pairHandler];
}

+ (NSString*) resolveHost:(NSString*)host
{
    struct hostent *hostent;
    
    if (inet_addr([host UTF8String]) != INADDR_NONE)
    {
        // Already an IP address
        return host;
    }
    else
    {
        hostent = gethostbyname([host UTF8String]);
        if (hostent != NULL)
        {
            char* ipstr = inet_ntoa(*(struct in_addr*)hostent->h_addr_list[0]);
            NSLog(@"Resolved %@ -> %s", host, ipstr);
            return [NSString stringWithUTF8String:ipstr];
        }
        else
        {
            NSLog(@"Failed to resolve host: %d", h_errno);
            return nil;
        }
    }
}

+ (NSString*) getId
{
    // we need to generate some unique id
    NSUUID* uniqueId = [ASIdentifierManager sharedManager].advertisingIdentifier;
    NSString* idString = [NSString stringWithString:[uniqueId UUIDString]];
    
    //use just the last 12 digits because GameStream truncates id's
    return [idString substringFromIndex:24];
}

+ (NSString*) getName
{
    NSString* name = [UIDevice currentDevice].name;
    NSLog(@"Device Name: %@", name);
    
    //sanitize name
    name = [name stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    return name;
}

- (id) initForStream:(NSString*)host
{
    self = [super init];
    self.hostName = host;
    self.mode = STREAM_MODE;
    return self;
}

- (id) initForPair:(NSString*)host
{
    self = [super init];
    self.hostName = host;
    self.mode = PAIR_MODE;
    return self;
}

- (void) main
{
    switch (self.mode)
    {
        case STREAM_MODE:
        {
            bool pairState;
            int err = [self pairState:&pairState];
            if (err) {
                NSLog(@"ERROR: Pair state error: %d", err);
                break;
            }
            
            if (!pairState)
            {
                NSLog(@"WARN: Not paired with host!");
                UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"Not Paired"
                                                                message:@"This device is not paired with the host."
                                                               delegate:nil
                                                      cancelButtonTitle:@"Okay"
                                                      otherButtonTitles:nil, nil];
                [self showAlert:alert];
                return;
            }
            
            //TODO: parse this from http request
            unsigned int appId = 67339056;
            
            NSData* applist = [self httpRequest:[NSString stringWithFormat:@"/applist?uniqueid=%@", [ConnectionHandler getId]]];
            
            NSString* streamString = [NSString stringWithFormat:@"http://%@:47989/launch?uniqueid=%@&appid=%u", [ConnectionHandler resolveHost:self.hostName], [ConnectionHandler getId], appId];
            NSLog(@"host: %@", self.hostName);
            
            NSURL* url = [[NSURL alloc] initWithString:streamString];
            
            NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
            [request setHTTPMethod:@"GET"];
            
            NSURLResponse* response = nil;
            NSData *response1 = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:NULL];
            
            
            [viewCont performSelector:@selector(segueIntoStream) onThread:[NSThread mainThread] withObject:nil waitUntilDone:NO];
            break;
        }
        case PAIR_MODE:
        {
            NSLog(@"Trying to pair to %@...", self.hostName);
            
            bool pairState;
            int err = [self pairState:&pairState];
            if (err) {
                NSLog(@"ERROR: Pair state error: %d", err);
                break;
            }
            
            NSLog(@"Pair state: %i", pairState);
            
            if(pairState)
            {
                UIAlertView* alert = [[UIAlertView alloc]initWithTitle:@"Pairing"
                                                               message:[NSString stringWithFormat:@"Already paired to:\n %@", self.hostName]
                                                              delegate:nil
                                                     cancelButtonTitle:@"Okay"
                                                     otherButtonTitles:nil, nil];
                [self showAlert:alert];
                NSLog(@"Showing alertview");
                break;
            }
            
            // this will hang until it pairs successfully or unsuccessfully
            NSData* pairResponse = [self httpRequest:[NSString stringWithFormat:@"pair?uniqueid=%@&devicename=%@", [ConnectionHandler getId], [ConnectionHandler getName]]];
            
            void* buffer = [self getXMLBufferFromData:pairResponse];
            if (buffer == NULL) {
                NSLog(@"ERROR: Unable to allocate pair buffer.");
                UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"Pairing"
                                                                message:[NSString stringWithFormat:@"Failed to pair with host:\n %@", self.hostName]
                                                               delegate:nil
                                                      cancelButtonTitle:@"Okay"
                                                      otherButtonTitles:nil, nil];
                [self showAlert:alert];
                break;
            }
            xmlDocPtr doc = xmlParseMemory(buffer, [pairResponse length]-1);
            
            if (doc == NULL) {
                NSLog(@"ERROR: An error occured trying to parse pair response.");
                break;
            }
            
            xmlNodePtr node = xmlDocGetRootElement(doc);
            node = node->xmlChildrenNode;
            while (node != NULL) {
                NSLog(@"Node: %s", node->name);
                if (!xmlStrcmp(node->name, (const xmlChar*)"sessionid"))
                {
                    xmlChar* sessionid = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    
                    if (xmlStrcmp(sessionid, (xmlChar*)"0")) {
                        UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"Pairing"
                                                                        message:[NSString stringWithFormat:@"Successfully paired to host:\n %@", self.hostName]
                                                                       delegate:nil
                                                              cancelButtonTitle:@"Okay"
                                                              otherButtonTitles:nil, nil];
                        [self showAlert:alert];
                        
                    } else {
                        UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"Pairing"
                                                                        message:[NSString stringWithFormat:@"Pairing was declined by host:\n %@", self.hostName]
                                                                       delegate:nil
                                                              cancelButtonTitle:@"Okay"
                                                              otherButtonTitles:nil, nil];
                        [self showAlert:alert];
                    }
                    
                    xmlFree(sessionid);
                    goto cleanup;
                }
                node = node->next;
            }
            
            NSLog(@"ERROR: Could not find \"sessionid\" element in XML");
            
        cleanup:
            xmlFreeNode(node);
            break;
        }
        default:
        {
            NSLog(@"Invalid connection mode specified!!!");
        }
    }
}

- (int) pairState: (bool*)paired
{
    int err;
    NSData* pairData = [self httpRequest:[NSString stringWithFormat:@"pairstate?uniqueid=%@", [ConnectionHandler getId]]];
    
    void* buffer = [self getXMLBufferFromData:pairData];
    
    if (buffer == NULL) {
        NSLog(@"ERROR: Unable to allocate pair state buffer.");
        UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"Pairing"
                                                        message:[NSString stringWithFormat:@"Failed to get pair state with host:\n %@", self.hostName]
                                                       delegate:nil
                                              cancelButtonTitle:@"Okay"
                                              otherButtonTitles:nil, nil];
        [self showAlert:alert];
        return -1;
    }
    
    xmlDocPtr doc = xmlParseMemory(buffer, [pairData length]-1);
    
    if (doc == NULL) {
        NSLog(@"ERROR: An error occured trying to parse pair state.");
        return -1;
    }
    
    xmlNodePtr node = xmlDocGetRootElement(doc);
    while (node != NULL) {
        NSLog(@"Node: %s", node->name);
        if (!xmlStrcmp(node->name, (const xmlChar*)"paired"))
        {
            xmlChar* pairState = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
            *paired = !xmlStrcmp(pairState, (const xmlChar*)"1");
            err = 0;
            
            xmlFree(pairState);
            goto cleanup;
        }
        node = node->xmlChildrenNode;
    }
    
    NSLog(@"ERROR: Could not find \"paired\" element in XML");
    err = -2;
    
cleanup:
    xmlFreeNode(node);
    return err;
}

- (NSData*) httpRequest:(NSString*)args
{
    NSString* requestString = [NSString stringWithFormat:@"http://%@:47989/%@", [ConnectionHandler resolveHost:self.hostName], args];
    
    NSLog(@"Making HTTP request: %@", requestString);
    
    NSURL* url = [[NSURL alloc] initWithString:requestString];
    
    NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
    [request setHTTPMethod:@"GET"];
    [request setTimeoutInterval:7];
    
    NSURLResponse* response = nil;
    NSError* error = nil;
    NSData* responseData = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:&error];
    
    if (error != nil) {
        NSLog(@"An error occured making HTTP request: %@\n Caused by: %@", [error localizedDescription], [error localizedFailureReason]);
    }
    return responseData;
}


- (void*) getXMLBufferFromData:(NSData*)data
{
    if (data == nil) {
        NSLog(@"ERROR: No data to get XML from!");
        return NULL;
    }
    char* buffer = malloc([data length] + 1);
    if (buffer == NULL) {
        NSLog(@"ERROR: Unable to allocate XML buffer.");
        return NULL;
    }
    [data getBytes:buffer length:[data length]];
    buffer[[data length]] = 0;
    
    NSLog(@"Buffer: %s", buffer);
    //#allthejank
    void* newBuffer = (void*)[[[NSString stringWithUTF8String:buffer]stringByReplacingOccurrencesOfString:@"UTF-16" withString:@"UTF-8" options:NSCaseInsensitiveSearch range:NSMakeRange(0, [data length])] UTF8String];
    
    NSLog(@"New Buffer: %s", newBuffer);
    free(buffer);
    return newBuffer;
}

- (void) showAlert:(UIAlertView*) alert
{
    [alert performSelector:@selector(show) onThread:[NSThread mainThread] withObject:nil waitUntilDone:NO];
}

@end
