#include "TCXIOSPlatform.h"
#include "tcx/ios/EventQueue.h"

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

namespace {

NSString* TCXIOSV03Ns(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

std::string TCXIOSV03Str(NSString* value) {
    return value ? std::string(value.UTF8String) : std::string();
}

tcx::ios::Error TCXIOSV03NativeError(NSError* error, const std::string& fallback) {
    tcx::ios::Error mapped = !error ? tcx::ios::Error{tcx::ios::ErrorCode::NativeError, fallback, 0}
        : tcx::ios::Error{
        tcx::ios::ErrorCode::NativeError,
        TCXIOSV03Str(error.localizedDescription),
        static_cast<int>(error.code)
    };
    tcx::ios::logger().error("tcxIOS.native", fallback, mapped);
    return mapped;
}

template <typename T>
void TCXIOSV03Finish(tcx::ios::Completion<T> done, tcx::ios::Result<T> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

void TCXIOSV03FinishVoid(tcx::ios::Completion<void> done, tcx::ios::Result<void> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

NSMutableSet* TCXIOSV03Delegates() {
    static NSMutableSet* delegates = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        delegates = [NSMutableSet set];
    });
    return delegates;
}

void TCXIOSV03Retain(id delegate) {
    @synchronized (TCXIOSV03Delegates()) {
        [TCXIOSV03Delegates() addObject:delegate];
    }
}

void TCXIOSV03Release(id delegate) {
    @synchronized (TCXIOSV03Delegates()) {
        [TCXIOSV03Delegates() removeObject:delegate];
    }
}

UIViewController* TCXIOSV03TopViewController(UIViewController* root) {
    UIViewController* current = root;
    while (current.presentedViewController) current = current.presentedViewController;
    if ([current isKindOfClass:UINavigationController.class]) {
        UIViewController* visible = ((UINavigationController*)current).visibleViewController;
        if (visible) return TCXIOSV03TopViewController(visible);
    }
    if ([current isKindOfClass:UITabBarController.class]) {
        UIViewController* selected = ((UITabBarController*)current).selectedViewController;
        if (selected) return TCXIOSV03TopViewController(selected);
    }
    return current;
}

UIViewController* TCXIOSV03Presenter() {
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) continue;
        UIWindowScene* windowScene = (UIWindowScene*)scene;
        if (windowScene.activationState != UISceneActivationStateForegroundActive) continue;
        for (UIWindow* window in windowScene.windows) {
            if (window.isKeyWindow && window.rootViewController) {
                return TCXIOSV03TopViewController(window.rootViewController);
            }
        }
    }
    return nil;
}

NSArray<CBUUID*>* TCXIOSV03CBUUIDs(const std::vector<std::string>& uuids) {
    NSMutableArray<CBUUID*>* out = [NSMutableArray array];
    for (const auto& uuid : uuids) {
        [out addObject:[CBUUID UUIDWithString:TCXIOSV03Ns(uuid)]];
    }
    return out.count > 0 ? out : nil;
}

std::string TCXIOSV03PeripheralIdentifier(CBPeripheral* peripheral) {
    return TCXIOSV03Str(peripheral.identifier.UUIDString);
}

std::string TCXIOSV03CharacteristicKey(const tcx::ios::BLECharacteristicRef& ref) {
    return ref.peripheralIdentifier + "|" + ref.serviceUUID + "|" + ref.characteristicUUID;
}

std::string TCXIOSV03CharacteristicKey(CBPeripheral* peripheral, CBCharacteristic* characteristic) {
    return TCXIOSV03PeripheralIdentifier(peripheral) + "|" +
           TCXIOSV03Str(characteristic.service.UUID.UUIDString) + "|" +
           TCXIOSV03Str(characteristic.UUID.UUIDString);
}

tcx::ios::BluetoothState TCXIOSV03BluetoothState(CBManagerState state) {
    switch (state) {
        case CBManagerStateUnsupported: return tcx::ios::BluetoothState::Unsupported;
        case CBManagerStateUnauthorized: return tcx::ios::BluetoothState::Unauthorized;
        case CBManagerStatePoweredOff: return tcx::ios::BluetoothState::PoweredOff;
        case CBManagerStatePoweredOn: return tcx::ios::BluetoothState::PoweredOn;
        case CBManagerStateResetting:
        case CBManagerStateUnknown: return tcx::ios::BluetoothState::Unknown;
    }
    return tcx::ios::BluetoothState::Unknown;
}

tcx::ios::PermissionState TCXIOSV03BluetoothPermissionState(tcx::ios::BluetoothState state) {
    switch (state) {
        case tcx::ios::BluetoothState::Unsupported:
            return tcx::ios::PermissionState::Restricted;
        case tcx::ios::BluetoothState::Unauthorized:
            return tcx::ios::PermissionState::Denied;
        case tcx::ios::BluetoothState::PoweredOff:
        case tcx::ios::BluetoothState::PoweredOn:
            return tcx::ios::PermissionState::Authorized;
        case tcx::ios::BluetoothState::Unknown:
            return tcx::ios::PermissionState::Unknown;
    }
    return tcx::ios::PermissionState::Unknown;
}

tcx::ios::PermissionState TCXIOSV03ContactPermissionState(CNAuthorizationStatus status) {
    switch (status) {
        case CNAuthorizationStatusNotDetermined:
            return tcx::ios::PermissionState::NotDetermined;
        case CNAuthorizationStatusRestricted:
            return tcx::ios::PermissionState::Restricted;
        case CNAuthorizationStatusDenied:
            return tcx::ios::PermissionState::Denied;
        case CNAuthorizationStatusAuthorized:
            return tcx::ios::PermissionState::Authorized;
        case CNAuthorizationStatusLimited:
            return tcx::ios::PermissionState::Limited;
    }
    return tcx::ios::PermissionState::Unknown;
}

std::vector<std::uint8_t> TCXIOSV03Bytes(NSData* data) {
    std::vector<std::uint8_t> bytes(data.length);
    if (data.length > 0) std::memcpy(bytes.data(), data.bytes, data.length);
    return bytes;
}

NSData* TCXIOSV03Data(const std::vector<std::uint8_t>& bytes) {
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

} // namespace

@interface TCXIOSPencilViewController : UIViewController
@property(nonatomic, strong) PKCanvasView* canvasView;
@property(nonatomic, strong) PKToolPicker* toolPicker;
@property(nonatomic, assign) BOOL showToolPicker;
@end

@implementation TCXIOSPencilViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    self.canvasView = [[PKCanvasView alloc] initWithFrame:self.view.bounds];
    self.canvasView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.canvasView.backgroundColor = UIColor.whiteColor;
    [self.view addSubview:self.canvasView];
    if (self.showToolPicker) {
        self.toolPicker = [[PKToolPicker alloc] init];
        [self.toolPicker addObserver:self.canvasView];
        [self.toolPicker setVisible:YES forFirstResponder:self.canvasView];
        [self.canvasView becomeFirstResponder];
    }
}

@end

namespace {

TCXIOSPencilViewController* gPencilController = nil;

NSMutableDictionary<NSString*, SKProduct*>* TCXIOSStoreProductCache() {
    static NSMutableDictionary<NSString*, SKProduct*>* products = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        products = [NSMutableDictionary dictionary];
    });
    return products;
}

tcx::ios::StoreTransactionState TCXIOSStoreTransactionState(SKPaymentTransactionState state) {
    switch (state) {
        case SKPaymentTransactionStatePurchasing: return tcx::ios::StoreTransactionState::Purchasing;
        case SKPaymentTransactionStatePurchased: return tcx::ios::StoreTransactionState::Purchased;
        case SKPaymentTransactionStateFailed: return tcx::ios::StoreTransactionState::Failed;
        case SKPaymentTransactionStateRestored: return tcx::ios::StoreTransactionState::Restored;
        case SKPaymentTransactionStateDeferred: return tcx::ios::StoreTransactionState::Deferred;
    }
    return tcx::ios::StoreTransactionState::Unknown;
}

tcx::ios::StoreTransactionUpdate TCXIOSStoreTransactionUpdate(SKPaymentTransaction* transaction) {
    tcx::ios::StoreTransactionUpdate update;
    update.productIdentifier = TCXIOSV03Str(transaction.payment.productIdentifier);
    update.transactionIdentifier = TCXIOSV03Str(transaction.transactionIdentifier);
    update.state = TCXIOSStoreTransactionState(transaction.transactionState);
    update.errorMessage = TCXIOSV03Str(transaction.error.localizedDescription);
    return update;
}

} // namespace

namespace tcx::ios::detail {
void platformPresentPencilCanvas(const PencilCanvasRequest& request, Completion<void> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* presenter = TCXIOSV03Presenter();
        if (!presenter) {
            TCXIOSV03FinishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidState, "No active presenter for PencilKit.", 0}));
            return;
        }
        gPencilController = [[TCXIOSPencilViewController alloc] init];
        gPencilController.showToolPicker = request.showToolPicker;
        gPencilController.modalPresentationStyle = UIModalPresentationFullScreen;
        [presenter presentViewController:gPencilController animated:YES completion:^{
            TCXIOSV03FinishVoid(std::move(done), Result<void>::success());
        }];
    });
}

void platformDismissPencilCanvas() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gPencilController dismissViewControllerAnimated:YES completion:nil];
        gPencilController = nil;
    });
}

Result<PencilDrawingData> platformCapturePencilDrawing() {
    if (!gPencilController || !gPencilController.canvasView) {
        return Result<PencilDrawingData>::failure({ErrorCode::InvalidState, "Pencil canvas is not presented.", 0});
    }
    PKDrawing* drawing = gPencilController.canvasView.drawing;
    NSData* drawingData = drawing.dataRepresentation;
    CGRect bounds = gPencilController.canvasView.bounds;
    CGFloat scale = UIScreen.mainScreen.scale;
    UIImage* image = [drawing imageFromRect:bounds scale:scale];
    NSData* png = UIImagePNGRepresentation(image);
    PencilDrawingData out;
    out.data = TCXIOSV03Bytes(drawingData);
    out.png = TCXIOSV03Bytes(png);
    out.pixelWidth = static_cast<int>(bounds.size.width * scale);
    out.pixelHeight = static_cast<int>(bounds.size.height * scale);
    return Result<PencilDrawingData>::success(std::move(out));
}

void platformClearPencilCanvas() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (gPencilController.canvasView) {
            gPencilController.canvasView.drawing = [[PKDrawing alloc] init];
        }
    });
}

} // namespace tcx::ios::detail
