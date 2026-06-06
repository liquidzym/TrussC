#pragma once

#include "Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tcx::ios {

enum class KeychainAccessibility {
    WhenUnlocked,
    AfterFirstUnlock
};

struct KeychainItem {
    std::string service = "tcxIOS";
    std::string account;
    std::vector<std::uint8_t> data;
    KeychainAccessibility accessibility = KeychainAccessibility::WhenUnlocked;
};

class Keychain {
public:
    Result<void> set(const KeychainItem& item);
    Result<std::vector<std::uint8_t>> get(const std::string& service,
                                          const std::string& account) const;
    Result<std::string> getString(const std::string& service,
                                  const std::string& account) const;
    Result<void> setString(const std::string& service,
                           const std::string& account,
                           const std::string& value,
                           KeychainAccessibility accessibility = KeychainAccessibility::WhenUnlocked);
    Result<void> remove(const std::string& service, const std::string& account);
};

enum class AuthenticationPolicy {
    DeviceOwnerAuthentication,
    DeviceOwnerAuthenticationWithBiometrics
};

struct AuthenticationAvailability {
    bool available = false;
    std::string biometryType;
    Error error;
};

struct AuthenticationRequest {
    std::string reason = "Authenticate";
    AuthenticationPolicy policy = AuthenticationPolicy::DeviceOwnerAuthentication;
};

struct AuthenticationResult {
    bool authenticated = false;
    std::string biometryType;
};

class LocalAuthentication {
public:
    AuthenticationAvailability availability(AuthenticationPolicy policy) const;
    void evaluate(const AuthenticationRequest& request, Completion<AuthenticationResult> done);
};

Keychain& keychain();
LocalAuthentication& localAuthentication();

std::string toString(KeychainAccessibility accessibility);
std::string toString(AuthenticationPolicy policy);

} // namespace tcx::ios
