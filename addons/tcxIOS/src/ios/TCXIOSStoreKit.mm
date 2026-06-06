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

namespace {

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

@interface TCXIOSProductsDelegate : NSObject <SKProductsRequestDelegate>
- (instancetype)initWithCompletion:(tcx::ios::StoreProductsHandler)completion;
@end

@implementation TCXIOSProductsDelegate {
    tcx::ios::StoreProductsHandler completion_;
}

- (instancetype)initWithCompletion:(tcx::ios::StoreProductsHandler)completion {
    self = [super init];
    if (self) completion_ = std::move(completion);
    return self;
}

- (void)productsRequest:(SKProductsRequest*)request didReceiveResponse:(SKProductsResponse*)response {
    std::vector<tcx::ios::StoreProduct> products;
    NSNumberFormatter* formatter = [[NSNumberFormatter alloc] init];
    formatter.numberStyle = NSNumberFormatterCurrencyStyle;
    for (SKProduct* product in response.products) {
        TCXIOSStoreProductCache()[product.productIdentifier] = product;
        formatter.locale = product.priceLocale;
        products.push_back({
            TCXIOSV03Str(product.productIdentifier),
            TCXIOSV03Str(product.localizedTitle),
            TCXIOSV03Str(product.localizedDescription),
            TCXIOSV03Str([formatter stringFromNumber:product.price]),
            TCXIOSV03Str([product.priceLocale objectForKey:NSLocaleCurrencyCode])
        });
    }
    TCXIOSV03Release(self);
    TCXIOSV03Finish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::StoreProduct>>::success(std::move(products)));
}

- (void)request:(SKRequest*)request didFailWithError:(NSError*)error {
    TCXIOSV03Release(self);
    TCXIOSV03Finish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::StoreProduct>>::failure(TCXIOSV03NativeError(error, "StoreKit products request failed.")));
}

@end

@interface TCXIOSPaymentObserver : NSObject <SKPaymentTransactionObserver>
+ (instancetype)shared;
- (void)purchase:(NSString*)productIdentifier completion:(tcx::ios::Completion<tcx::ios::StorePurchaseResult>)completion;
- (void)restore:(tcx::ios::Completion<std::vector<tcx::ios::StoreTransactionUpdate>>)completion;
- (void)setTransactionUpdateHandler:(tcx::ios::StoreTransactionUpdateHandler)handler;
- (void)clearTransactionUpdateHandler;
@end

@implementation TCXIOSPaymentObserver {
    std::mutex mutex_;
    std::map<std::string, tcx::ios::Completion<tcx::ios::StorePurchaseResult>> completions_;
    tcx::ios::Completion<std::vector<tcx::ios::StoreTransactionUpdate>> restoreCompletion_;
    std::vector<tcx::ios::StoreTransactionUpdate> restoredTransactions_;
    tcx::ios::StoreTransactionUpdateHandler transactionUpdateHandler_;
}

+ (instancetype)shared {
    static TCXIOSPaymentObserver* observer = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        observer = [[TCXIOSPaymentObserver alloc] init];
        [SKPaymentQueue.defaultQueue addTransactionObserver:observer];
    });
    return observer;
}

- (void)purchase:(NSString*)productIdentifier completion:(tcx::ios::Completion<tcx::ios::StorePurchaseResult>)completion {
    SKProduct* product = TCXIOSStoreProductCache()[productIdentifier];
    if (!product) {
        TCXIOSV03Finish(std::move(completion), tcx::ios::Result<tcx::ios::StorePurchaseResult>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "Request StoreKit products before purchasing.",
            0
        }));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        completions_[TCXIOSV03Str(productIdentifier)] = std::move(completion);
    }
    [SKPaymentQueue.defaultQueue addPayment:[SKPayment paymentWithProduct:product]];
}

- (void)restore:(tcx::ios::Completion<std::vector<tcx::ios::StoreTransactionUpdate>>)completion {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        restoreCompletion_ = std::move(completion);
        restoredTransactions_.clear();
    }
    [SKPaymentQueue.defaultQueue restoreCompletedTransactions];
}

- (void)setTransactionUpdateHandler:(tcx::ios::StoreTransactionUpdateHandler)handler {
    std::lock_guard<std::mutex> lock(mutex_);
    transactionUpdateHandler_ = std::move(handler);
}

- (void)clearTransactionUpdateHandler {
    std::lock_guard<std::mutex> lock(mutex_);
    transactionUpdateHandler_ = nullptr;
}

- (void)paymentQueue:(SKPaymentQueue*)queue updatedTransactions:(NSArray<SKPaymentTransaction*>*)transactions {
    for (SKPaymentTransaction* transaction in transactions) {
        tcx::ios::StoreTransactionUpdate update = TCXIOSStoreTransactionUpdate(transaction);
        tcx::ios::StoreTransactionUpdateHandler updateHandler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            updateHandler = transactionUpdateHandler_;
            if (transaction.transactionState == SKPaymentTransactionStateRestored) {
                restoredTransactions_.push_back(update);
            }
        }
        if (updateHandler) {
            tcx::ios::eventQueue().post([updateHandler, update]() mutable {
                updateHandler(update);
            });
        }

        if (transaction.transactionState == SKPaymentTransactionStatePurchasing) continue;
        const std::string product = TCXIOSV03Str(transaction.payment.productIdentifier);
        tcx::ios::Completion<tcx::ios::StorePurchaseResult> done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done = std::move(completions_[product]);
            completions_.erase(product);
        }
        if (!done) {
            [queue finishTransaction:transaction];
            continue;
        }
        if (transaction.transactionState == SKPaymentTransactionStatePurchased ||
            transaction.transactionState == SKPaymentTransactionStateRestored) {
            TCXIOSV03Finish(std::move(done), tcx::ios::Result<tcx::ios::StorePurchaseResult>::success({product, true}));
        } else {
            TCXIOSV03Finish(std::move(done), tcx::ios::Result<tcx::ios::StorePurchaseResult>::failure(
                TCXIOSV03NativeError(transaction.error, "StoreKit purchase failed.")));
        }
        [queue finishTransaction:transaction];
    }
}

- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue*)queue {
    (void)queue;
    tcx::ios::Completion<std::vector<tcx::ios::StoreTransactionUpdate>> done;
    std::vector<tcx::ios::StoreTransactionUpdate> restored;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(restoreCompletion_);
        restored = std::move(restoredTransactions_);
        restoredTransactions_.clear();
    }
    if (done) {
        TCXIOSV03Finish(std::move(done), tcx::ios::Result<std::vector<tcx::ios::StoreTransactionUpdate>>::success(std::move(restored)));
    }
}

- (void)paymentQueue:(SKPaymentQueue*)queue restoreCompletedTransactionsFailedWithError:(NSError*)error {
    (void)queue;
    tcx::ios::Completion<std::vector<tcx::ios::StoreTransactionUpdate>> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done = std::move(restoreCompletion_);
        restoredTransactions_.clear();
    }
    if (done) {
        TCXIOSV03Finish(std::move(done), tcx::ios::Result<std::vector<tcx::ios::StoreTransactionUpdate>>::failure(
            TCXIOSV03NativeError(error, "StoreKit restore failed.")));
    }
}

@end

namespace tcx::ios::detail {
bool platformStoreCanMakePayments() {
    return SKPaymentQueue.canMakePayments;
}

void platformRequestStoreProducts(const std::vector<std::string>& productIdentifiers,
                                  StoreProductsHandler done) {
    NSMutableSet<NSString*>* identifiers = [NSMutableSet set];
    for (const auto& identifier : productIdentifiers) {
        [identifiers addObject:TCXIOSV03Ns(identifier)];
    }
    if (identifiers.count == 0) {
        TCXIOSV03Finish(std::move(done), Result<std::vector<StoreProduct>>::failure({ErrorCode::InvalidArgument, "No product identifiers were provided.", 0}));
        return;
    }
    TCXIOSProductsDelegate* delegate = [[TCXIOSProductsDelegate alloc] initWithCompletion:std::move(done)];
    TCXIOSV03Retain(delegate);
    SKProductsRequest* request = [[SKProductsRequest alloc] initWithProductIdentifiers:identifiers];
    request.delegate = delegate;
    [request start];
}

void platformPurchaseStoreProduct(const std::string& productIdentifier,
                                  Completion<StorePurchaseResult> done) {
    if (productIdentifier.empty()) {
        TCXIOSV03Finish(std::move(done), Result<StorePurchaseResult>::failure({ErrorCode::InvalidArgument, "Product identifier is empty.", 0}));
        return;
    }
    [[TCXIOSPaymentObserver shared] purchase:TCXIOSV03Ns(productIdentifier) completion:std::move(done)];
}

void platformRestoreStorePurchases(Completion<std::vector<StoreTransactionUpdate>> done) {
    [[TCXIOSPaymentObserver shared] restore:std::move(done)];
}

void platformSetStoreTransactionUpdateHandler(StoreTransactionUpdateHandler handler) {
    [[TCXIOSPaymentObserver shared] setTransactionUpdateHandler:std::move(handler)];
}

void platformClearStoreTransactionUpdateHandler() {
    [[TCXIOSPaymentObserver shared] clearTransactionUpdateHandler];
}

} // namespace tcx::ios::detail
