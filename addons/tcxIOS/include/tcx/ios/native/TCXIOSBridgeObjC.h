#pragma once

#ifndef __OBJC__
#error "TCXIOSBridgeObjC.h is an Objective-C/Objective-C++ integration header."
#endif

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#ifdef __cplusplus
extern "C" {
#endif

void TCXIOSRegisterScenePresenter(NSString* identifier, UIViewController* viewController);
UIViewController* TCXIOSRegisteredScenePresenter(NSString* identifier);
void TCXIOSUnregisterScenePresenter(NSString* identifier);
void TCXIOSSetActiveSceneIdentifier(NSString* identifier);
UIViewController* TCXIOSCurrentPresenter(void);

void TCXIOSHandleBackgroundURLSessionEvents(NSString* identifier,
                                            void (^completionHandler)(void));

#ifdef __cplusplus
} // extern "C"
#endif
