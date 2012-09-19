//
//  QualityOfServiceController.m
//  SimpleMessage
//
//  Created by Dominik Zajac on 17.09.12.
//  Copyright (c) 2012 dc-square GmbH. All rights reserved.
//

#import "QualityOfServiceController.h"

@implementation QualityOfServiceController

@synthesize qualityOfServiceLevel = _qualityOfServiceLevel;

- (id)init
{
    self = [super init];
    if (self) {
        // Initialization code here.
        
        _qualityOfServiceLevel = 0;
        
    }
    
    return self;
}



@end
