#pragma once

#include "Types.h"

#include <functional>
#include <string>
#include <vector>

namespace tcx::ios {

struct StoreProduct {
    std::string identifier;
    std::string localizedTitle;
    std::string localizedDescription;
    std::string price;
    std::string currencyCode;
};

struct StorePurchaseResult {
    std::string productIdentifier;
    bool purchased = false;
};

enum class StoreTransactionState {
    Unknown,
    Purchasing,
    Purchased,
    Restored,
    Failed,
    Deferred,
    Revoked
};

struct StoreTransactionUpdate {
    std::string productIdentifier;
    std::string transactionIdentifier;
    StoreTransactionState state = StoreTransactionState::Unknown;
    std::string errorMessage;
};

using StoreProductsHandler = Completion<std::vector<StoreProduct>>;
using StoreTransactionUpdateHandler = std::function<void(const StoreTransactionUpdate&)>;

class StoreKit {
public:
    bool canMakePayments() const;
    void requestProducts(const std::vector<std::string>& productIdentifiers, StoreProductsHandler done);
    void purchase(const std::string& productIdentifier, Completion<StorePurchaseResult> done);
    void restorePurchases(Completion<std::vector<StoreTransactionUpdate>> done);
    void setTransactionUpdateHandler(StoreTransactionUpdateHandler handler);
    void clearTransactionUpdateHandler();
};

StoreKit& storeKit();

std::string toString(StoreTransactionState state);

} // namespace tcx::ios
