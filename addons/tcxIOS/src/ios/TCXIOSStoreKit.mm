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
    update.productIdentifier = TCXIOSStr(transaction.payment.productIdentifier);
    update.transactionIdentifier = TCXIOSStr(transaction.transactionIdentifier);
    update.state = TCXIOSStoreTransactionState(transaction.transactionState);
    update.errorMessage = TCXIOSStr(transaction.error.localizedDescription);
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
            TCXIOSStr(product.productIdentifier),
            TCXIOSStr(product.localizedTitle),
            TCXIOSStr(product.localizedDescription),
            TCXIOSStr([formatter stringFromNumber:product.price]),
            TCXIOSStr([product.priceLocale objectForKey:NSLocaleCurrencyCode])
        });
    }
    TCXIOSReleaseDelegate(self);
    TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::StoreProduct>>::success(std::move(products)));
}

- (void)request:(SKRequest*)request didFailWithError:(NSError*)error {
    TCXIOSReleaseDelegate(self);
    TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::StoreProduct>>::failure(TCXIOSNativeError(error, "StoreKit products request failed.")));
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
        TCXIOSFinish(std::move(completion), tcx::ios::Result<tcx::ios::StorePurchaseResult>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "Request StoreKit products before purchasing.",
            0
        }));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        completions_[TCXIOSStr(productIdentifier)] = std::move(completion);
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
        const std::string product = TCXIOSStr(transaction.payment.productIdentifier);
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
            TCXIOSFinish(std::move(done), tcx::ios::Result<tcx::ios::StorePurchaseResult>::success({product, true}));
        } else {
            TCXIOSFinish(std::move(done), tcx::ios::Result<tcx::ios::StorePurchaseResult>::failure(
                TCXIOSNativeError(transaction.error, "StoreKit purchase failed.")));
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
        TCXIOSFinish(std::move(done), tcx::ios::Result<std::vector<tcx::ios::StoreTransactionUpdate>>::success(std::move(restored)));
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
        TCXIOSFinish(std::move(done), tcx::ios::Result<std::vector<tcx::ios::StoreTransactionUpdate>>::failure(
            TCXIOSNativeError(error, "StoreKit restore failed.")));
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
        [identifiers addObject:TCXIOSNs(identifier)];
    }
    if (identifiers.count == 0) {
        TCXIOSFinish(std::move(done), Result<std::vector<StoreProduct>>::failure({ErrorCode::InvalidArgument, "No product identifiers were provided.", 0}));
        return;
    }
    TCXIOSProductsDelegate* delegate = [[TCXIOSProductsDelegate alloc] initWithCompletion:std::move(done)];
    TCXIOSRetainDelegate(delegate);
    SKProductsRequest* request = [[SKProductsRequest alloc] initWithProductIdentifiers:identifiers];
    request.delegate = delegate;
    [request start];
}

void platformPurchaseStoreProduct(const std::string& productIdentifier,
                                  Completion<StorePurchaseResult> done) {
    if (productIdentifier.empty()) {
        TCXIOSFinish(std::move(done), Result<StorePurchaseResult>::failure({ErrorCode::InvalidArgument, "Product identifier is empty.", 0}));
        return;
    }
    [[TCXIOSPaymentObserver shared] purchase:TCXIOSNs(productIdentifier) completion:std::move(done)];
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
