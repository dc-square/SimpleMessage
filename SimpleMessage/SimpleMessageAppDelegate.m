//
//  SimpleMessageAppDelegate.m
//  SimpleMessage
//
//  Created by Dominik Zajac 
//  Copyright 2012 Dominik Zajac (dc-square GmbH)
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#import "SimpleMessageAppDelegate.h"
#import "MQTTClient.h"
#import "ThreadSliderController.h"
#import <dispatch/dispatch.h>
#import <stdlib.h>
#import "QueueController.h"
#import "QualityOfServiceController.h"

@implementation SimpleMessageAppDelegate;

@synthesize brokerTextField;
@synthesize parallelThreadsSlider;
@synthesize parallelTextField;
@synthesize window;
@synthesize targetField;
@synthesize messageField;
@synthesize threadSliderController = _threadSliderController;
@synthesize ThreadCountTextField;
@synthesize resumeButton;
@synthesize appendCounterCheckBox;
@synthesize retainMessageCheckBox;
@synthesize queueController = _queueController;
@synthesize qualityOfServiceController = _qualityOfServiceController;

#define CLIENTID    "SimpleMessage"
#define TIMEOUT     10000L


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
        
    //Slider instance
    ThreadSliderController *athreadSliderControler = [[ThreadSliderController alloc] init];
    [self setThreadSliderController:athreadSliderControler];
    
    
    //QueueController for dispatch queue
    QueueController *aqueueController = [[QueueController alloc] init];
    [self setQueueController:aqueueController];
    
    //QualityOfServiceController holding Quality of Service Level
    QualityOfServiceController *aqualityOfServiceController = [[QualityOfServiceController alloc] init];
    [self setQualityOfServiceController:aqualityOfServiceController];
    
}

volatile MQTTClient_deliveryToken deliveredtoken;

- (IBAction)sendButton:(id)sender {
    int i;
    
    if([[targetField stringValue] length] != 0 && [[messageField stringValue] length] != 0  ) {
                
        for (i=1; i <= [ThreadCountTextField intValue]; i++){
            dispatch_async([self.queueController senderQueue], ^{[self sendMQTTMessage:i];});
        }
        
        }else{
           NSLog(@"No Value");
       }
}


- (IBAction)quitButtonAction:(id)sender {
    [[NSApplication sharedApplication] terminate:nil];
}

- (IBAction)takeValueFromThreadsSlider:(id)sender {        
    [self.threadSliderController setThreadCount:[sender intValue]];
    [self updateUserInterface];
}

- (IBAction)pauseButtonAction:(id)sender {
    [sendButton setEnabled:false];
    dispatch_suspend([self.queueController senderQueue]);
    [resumeButton setEnabled:true];
}

- (IBAction)resumeButtonAction:(id)sender {
    dispatch_resume([self.queueController senderQueue]);
    [resumeButton setEnabled:false];
    [sendButton setEnabled:true];
}

- (void)sendMQTTMessage:(int)queueIdentifier {

    int i;
    //NSString *message;
    Boolean appendmessage;
    
    //TODO Add Mac Adrees or System UID to Client ID
    char *client_id = [@"SimpleMessage-" stringByAppendingString:[NSString stringWithFormat:@"%d",queueIdentifier]];
        
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    
    char *chartopic = [[targetField stringValue] UTF8String];
    
    MQTTClient_create(&client, [[brokerTextField stringValue] UTF8String], CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    
    MQTTClient_setCallbacks(client, NULL, NULL, NULL, NULL);
    
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        NSLog(@"Failed to connect!");
    }    
            
    
    NSString *message = [messageField stringValue];
    
    //Append counter to message if checkbox is enabled
    if ([appendCounterCheckBox state] == NSOnState){
        appendmessage = true;
    } else {
        appendmessage = false;
    }
    
    //Set Qualit of Service Level
    pubmsg.qos = [self.qualityOfServiceController qualityOfServiceLevel];
    
    
    //Set retain message if checkbox is enabled
    if ([retainMessageCheckBox state] == NSOnState){
        pubmsg.retained = 1;
    } else {
        pubmsg.retained = 0;
    }
    
    deliveredtoken = 0;
    
    
    for (i=1; i <= [self.threadSliderController threadCount]; i++){
    
        if(appendmessage == true){
            
            pubmsg.payload = [[[message stringByAppendingString:[NSString stringWithFormat:@"%s"," - "]] stringByAppendingString:[NSString stringWithFormat:@"%d",i]] UTF8String];
            
            pubmsg.payloadlen = (int)[[[message stringByAppendingString:[NSString stringWithFormat:@"%s"," - "]] stringByAppendingString:[NSString stringWithFormat:@"%d",i]] length];

            
        } else {
            pubmsg.payload = [message UTF8String];
            pubmsg.payloadlen = (int)[message length];
            
        }
        
        MQTTClient_publishMessage(client, chartopic, &pubmsg, &token);
                
    
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    
}


- (void)updateUserInterface {
    
    [self.parallelTextField setIntValue:[self.threadSliderController threadCount]];
    [self.parallelThreadsSlider setIntValue:[self.threadSliderController threadCount]];
    
}

- (IBAction)updateRetainMessageCheckBox:(id)sender {
}



- (IBAction)updateQualityOfService:(id)sender {


    switch ([[sender selectedCell] tag]) {
        case 0:
            [self.qualityOfServiceController setQualityOfServiceLevel:0];
            break;
        case 1:
            [self.qualityOfServiceController setQualityOfServiceLevel:1];
           break;
        case 2:
            [self.qualityOfServiceController setQualityOfServiceLevel:2];
            break;
    }
    
}


@end
