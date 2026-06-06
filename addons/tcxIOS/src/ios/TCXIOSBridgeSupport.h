#pragma once

#include "TCXIOSPlatform.h"
#include "tcx/ios/EventQueue.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

NSString* TCXIOSNs(const std::string& value);
std::string TCXIOSStr(NSString* value);
tcx::ios::Error TCXIOSNativeError(NSError* error, const std::string& fallback);

namespace tcx::ios::detail {

template <typename T>
void TCXIOSFinish(tcx::ios::Completion<T> done, tcx::ios::Result<T> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

void TCXIOSFinishVoid(tcx::ios::Completion<void> done, tcx::ios::Result<void> result);

} // namespace tcx::ios::detail

using tcx::ios::detail::TCXIOSFinish;
using tcx::ios::detail::TCXIOSFinishVoid;

void TCXIOSRetainDelegate(id delegate);
void TCXIOSReleaseDelegate(id delegate);
void TCXIOSSetScenePresenter(NSString* identifier, UIViewController* viewController);
UIViewController* TCXIOSScenePresenter(NSString* identifier);
void TCXIOSRemoveScenePresenter(NSString* identifier);
UIViewController* TCXIOSTopViewController(UIViewController* root);
UIWindow* TCXIOSActiveWindow();
UIViewController* TCXIOSPresenter();
NSURL* TCXIOSTemporaryCopyURL(NSURL* sourceURL, NSString* directoryName, NSError** error);
bool TCXIOSHasUsageDescription(tcx::ios::Permission permission, tcx::ios::Completion<tcx::ios::PermissionState>& done);
NSArray<UTType*>* TCXIOSContentTypes(const std::vector<std::string>& identifiers);
