//
//  SimpleMessageAppDelegate.h
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

#import <Cocoa/Cocoa.h>
#import "MQTTClient.h"

@class ThreadSliderController;
@class QueueController;

@interface SimpleMessageAppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow *window;
    NSTextField *targetField;
    NSTextField *messageField;
    NSButton *sendButton;
    NSSlider *parallelThreadsSlider;
    NSTextField *parallelTextField;
    NSTextField *ThreadCountTextField;
    NSButton *resumeButton;
    NSTextField *brokerTextField;
}

@property (assign) Boolean stopped;
@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTextField *targetField;
@property (assign) IBOutlet NSTextField *messageField;
@property (assign) IBOutlet NSTextField *brokerLink;
@property (assign) IBOutlet NSTextField *brokerTextField;

@property (assign) IBOutlet NSSlider *parallelThreadsSlider;
@property (assign) IBOutlet NSTextField *parallelTextField;
@property (assign) ThreadSliderController *threadSliderController;
@property (assign) QueueController *queueController;

@property (assign) IBOutlet NSTextField *ThreadCountTextField;
@property (assign) IBOutlet NSButton *resumeButton;
- (IBAction)stopButtonAction:(id)sender;
- (IBAction)quitButtonAction:(id)sender;

- (IBAction)takeValueFromThreadsSlider:(id)sender;

- (IBAction)sendButton:(id)sender;

- (IBAction)pauseButtonAction:(id)sender;

- (IBAction)resumeButtonAction:(id)sender;

- (void)sendMQTTMessage;

- (void)updateUserInterface;

@end
