#include "TCXIOSBridgeSupport.h"

#import <ARKit/ARKit.h>
#import <Contacts/Contacts.h>
#import <ContactsUI/ContactsUI.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreImage/CoreImage.h>
#import <CoreML/CoreML.h>
#import <GameController/GameController.h>
#import <MultipeerConnectivity/MultipeerConnectivity.h>
#import <PencilKit/PencilKit.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>
#import <Vision/Vision.h>

#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

@interface TCXIOSContactPickerDelegate : NSObject <CNContactPickerDelegate>
- (instancetype)initWithCompletion:(tcx::ios::Completion<tcx::ios::PickedContact>)completion;
@end

@implementation TCXIOSContactPickerDelegate {
    tcx::ios::Completion<tcx::ios::PickedContact> completion_;
}

- (instancetype)initWithCompletion:(tcx::ios::Completion<tcx::ios::PickedContact>)completion {
    self = [super init];
    if (self) completion_ = std::move(completion);
    return self;
}

- (void)contactPickerDidCancel:(CNContactPickerViewController*)picker {
    TCXIOSReleaseDelegate(self);
    TCXIOSFinish(std::move(completion_), tcx::ios::Result<tcx::ios::PickedContact>::failure({
        tcx::ios::ErrorCode::Cancelled,
        "Contact picker was cancelled.",
        0
    }));
}

- (void)contactPicker:(CNContactPickerViewController*)picker didSelectContact:(CNContact*)contact {
    tcx::ios::PickedContact picked;
    picked.identifier = TCXIOSStr(contact.identifier);
    picked.givenName = TCXIOSStr(contact.givenName);
    picked.familyName = TCXIOSStr(contact.familyName);
    for (CNLabeledValue<CNPhoneNumber*>* item in contact.phoneNumbers) {
        picked.phoneNumbers.push_back(TCXIOSStr(item.value.stringValue));
    }
    for (CNLabeledValue<NSString*>* item in contact.emailAddresses) {
        picked.emailAddresses.push_back(TCXIOSStr(item.value));
    }
    TCXIOSReleaseDelegate(self);
    TCXIOSFinish(std::move(completion_), tcx::ios::Result<tcx::ios::PickedContact>::success(std::move(picked)));
}

@end

namespace tcx::ios::detail {
void platformPickContact(Completion<PickedContact> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* presenter = TCXIOSPresenter();
        if (!presenter) {
            TCXIOSFinish(std::move(done), Result<PickedContact>::failure({ErrorCode::InvalidState, "No active presenter for ContactsUI.", 0}));
            return;
        }
        CNContactPickerViewController* picker = [[CNContactPickerViewController alloc] init];
        TCXIOSContactPickerDelegate* delegate = [[TCXIOSContactPickerDelegate alloc] initWithCompletion:std::move(done)];
        picker.delegate = delegate;
        TCXIOSRetainDelegate(delegate);
        [presenter presentViewController:picker animated:YES completion:nil];
    });
}

PermissionState platformContactsPermissionStatus() {
    return TCXIOSContactPermissionState([CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts]);
}

void platformRequestContactsPermission(Completion<PermissionState> done) {
    CNContactStore* store = [[CNContactStore alloc] init];
    [store requestAccessForEntityType:CNEntityTypeContacts completionHandler:^(BOOL granted, NSError* error) {
        if (error) {
            TCXIOSFinish(std::move(done), Result<PermissionState>::failure(TCXIOSNativeError(error, "Contacts authorization failed.")));
            return;
        }
        TCXIOSFinish(std::move(done), Result<PermissionState>::success(granted ? PermissionState::Authorized : PermissionState::Denied));
    }];
}

} // namespace tcx::ios::detail
