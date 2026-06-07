#import <Cocoa/Cocoa.h>

#include "include/cef_application_mac.h"

#import <objc/runtime.h>

namespace tcxCEF {
void installMacApplicationHooks();
}

namespace {

const void* kHandlingSendEventKey = &kHandlingSendEventKey;
bool gInstalled = false;

} // namespace

@interface NSApplication (tcxCEF_CefAppProtocol) <CefAppProtocol>
- (void)tcxcef_sendEvent:(NSEvent*)event;
@end

@implementation NSApplication (tcxCEF_CefAppProtocol)

- (BOOL)isHandlingSendEvent {
    NSNumber* value = objc_getAssociatedObject(self, kHandlingSendEventKey);
    return value.boolValue;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
    objc_setAssociatedObject(
        self,
        kHandlingSendEventKey,
        @(handlingSendEvent),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (void)tcxcef_sendEvent:(NSEvent*)event {
    const BOOL previous = [self isHandlingSendEvent];
    [self setHandlingSendEvent:YES];
    [self tcxcef_sendEvent:event];
    [self setHandlingSendEvent:previous];
}

@end

namespace tcxCEF {

void installMacApplicationHooks() {
    if (gInstalled) {
        return;
    }

    Class appClass = [NSApplication class];
    class_addProtocol(appClass, @protocol(CefAppProtocol));

    Method original = class_getInstanceMethod(appClass, @selector(sendEvent:));
    Method replacement = class_getInstanceMethod(appClass, @selector(tcxcef_sendEvent:));
    if (original && replacement) {
        method_exchangeImplementations(original, replacement);
    }

    gInstalled = true;
}

} // namespace tcxCEF
