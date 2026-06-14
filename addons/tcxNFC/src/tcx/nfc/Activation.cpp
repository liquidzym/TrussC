#include "tcx/nfc/Activation.h"

#include <algorithm>
#include <utility>

namespace tcx::nfc {

void PluginHost::add(IActivationPlugin* plugin) {
    if (plugin == nullptr) {
        return;
    }
    if (std::find(plugins_.begin(), plugins_.end(), plugin) == plugins_.end()) {
        plugins_.push_back(plugin);
    }
}

void PluginHost::dispatch(const ActivationEvent& event) const {
    for (auto* plugin : plugins_) {
        if (plugin != nullptr) {
            plugin->onActivation(event);
        }
    }
}

size_t PluginHost::size() const {
    return plugins_.size();
}

Result<ActivationResult> ActivationRuntime::runOnce(const ActivationConfig& config, IReader& reader, ITokenProvider& tokenProvider, PluginHost* plugins) {
    auto uid = reader.readUid();
    if (!uid.ok) {
        return Result<ActivationResult>::failure(uid.error);
    }

    auto token = tokenProvider.prepareToken(uid.value.uidHex);
    if (!token.ok) {
        return Result<ActivationResult>::failure(token.error);
    }

    auto write = reader.writeUrlRawNtag(token.value.url, config.ntagMaxUserBytes, config.ntagStartPage);
    if (!write.ok) {
        return Result<ActivationResult>::failure(write.error);
    }

    ActivationResult result;
    result.uid = uid.value.uidHex;
    result.token = token.value.token;
    result.url = token.value.url;
    result.urlMode = token.value.urlMode;
    result.writeStrategy = write.value.writeStrategy;
    result.verificationLevel = write.value.verificationLevel;
    result.pagesWritten = write.value.pagesWritten;

    if (plugins != nullptr) {
        ActivationEvent event;
        event.deviceId = config.deviceId;
        event.readerId = config.readerId;
        event.uid = result.uid;
        event.token = result.token;
        event.url = result.url;
        event.urlMode = result.urlMode;
        event.writeStrategy = result.writeStrategy;
        event.verificationLevel = result.verificationLevel;
        plugins->dispatch(event);
    }

    return Result<ActivationResult>::success(std::move(result));
}

} // namespace tcx::nfc
