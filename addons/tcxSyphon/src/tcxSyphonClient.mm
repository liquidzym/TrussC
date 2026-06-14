#include "tcxSyphonClient.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <Syphon/SyphonMetalClient.h>
#import <Syphon/SyphonServerDirectory.h>

#include <TrussC.h>

#include <atomic>

using namespace std;
using namespace tc;

namespace tcx {

namespace {

string nsToStd(NSString *s) {
    return s ? string([s UTF8String]) : string();
}

// Find the live server description matching a UUID (preferred) or name/appName.
NSDictionary *findServerDescription(const string &uuid,
                                    const string &name,
                                    const string &appName) {
    NSArray *servers = [[SyphonServerDirectory sharedDirectory] servers];
    for (NSDictionary *desc in servers) {
        if (!uuid.empty()) {
            if (nsToStd(desc[SyphonServerDescriptionUUIDKey]) == uuid) return desc;
            continue;
        }
        if (nsToStd(desc[SyphonServerDescriptionNameKey]) == name) {
            if (appName.empty() ||
                nsToStd(desc[SyphonServerDescriptionAppNameKey]) == appName) {
                return desc;
            }
        }
    }
    return nil;
}

} // namespace

struct SyphonClient::Impl {
    SyphonMetalClient *client = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    string serverName;
    int width = 0;
    int height = 0;
    atomic<bool> newFrame{false};
};

SyphonClient::SyphonClient() : impl_(make_unique<Impl>()) {}
SyphonClient::~SyphonClient() { disconnect(); }
SyphonClient::SyphonClient(SyphonClient &&) noexcept = default;
SyphonClient &SyphonClient::operator=(SyphonClient &&) noexcept = default;

vector<SyphonServerInfo> SyphonClient::listServers() {
    vector<SyphonServerInfo> out;
    NSArray *servers = [[SyphonServerDirectory sharedDirectory] servers];
    for (NSDictionary *desc in servers) {
        SyphonServerInfo info;
        info.name = nsToStd(desc[SyphonServerDescriptionNameKey]);
        info.appName = nsToStd(desc[SyphonServerDescriptionAppNameKey]);
        info.uuid = nsToStd(desc[SyphonServerDescriptionUUIDKey]);
        out.push_back(std::move(info));
    }
    return out;
}

bool SyphonClient::connect(const SyphonServerInfo &server) {
    // Prefer the UUID for an exact match (handles duplicate names).
    return connectToDescription(
        (__bridge void *)findServerDescription(server.uuid, server.name, server.appName),
        server.name);
}

bool SyphonClient::connect(const string &name, const string &appName) {
    return connectToDescription((__bridge void *)findServerDescription("", name, appName), name);
}

bool SyphonClient::connectToDescription(void *descPtr, const string &wanted) {
    disconnect();

    NSDictionary *desc = (__bridge NSDictionary *)descPtr;
    if (!desc) {
        logWarning() << "[tcxSyphon] SyphonClient::connect - no server matching \"" << wanted << "\"";
        return false;
    }

    id<MTLDevice> device = (__bridge id<MTLDevice>)const_cast<void *>(sg_mtl_device());
    if (!device) {
        logError() << "[tcxSyphon] SyphonClient::connect - Metal device unavailable";
        return false;
    }

    impl_->device = device;
    impl_->queue = [device newCommandQueue];
    atomic<bool> *flag = &impl_->newFrame;
    impl_->client = [[SyphonMetalClient alloc] initWithServerDescription:desc
                                                                 device:device
                                                                options:nil
                                                        newFrameHandler:^(SyphonMetalClient *c) {
        (void)c;
        flag->store(true);
    }];
    if (!impl_->client) {
        logError() << "[tcxSyphon] SyphonClient::connect - failed to create SyphonMetalClient";
        return false;
    }
    impl_->serverName = nsToStd(desc[SyphonServerDescriptionNameKey]);
    return true;
}

void SyphonClient::disconnect() {
    if (impl_->client) {
        [impl_->client stop];
        impl_->client = nil;
    }
    impl_->queue = nil;
    impl_->device = nil;
    impl_->serverName.clear();
    impl_->width = 0;
    impl_->height = 0;
    impl_->newFrame.store(false);
}

bool SyphonClient::isConnected() const { return impl_->client != nil; }

bool SyphonClient::receive(Texture &texture) {
    if (!impl_->client) return false;

    id<MTLTexture> frame = [impl_->client newFrameImage];
    if (!frame) return false;

    int w = (int)frame.width;
    int h = (int)frame.height;

    // (Re)allocate destination as BGRA8 to match Syphon's surface: a
    // same-format blit is valid and the Metal sampler returns correct RGBA.
    if (!texture.isAllocated() || texture.getWidth() != w || texture.getHeight() != h) {
        texture.allocate(w, h, TextureFormat::BGRA8, TextureUsage::RenderTarget);
        // Syphon frames carry premultiplied alpha (the common convention, and
        // what TrussC's own Fbo produces). Drawing them straight-alpha would
        // wash out any semi-transparent pixels against the background.
        texture.setPremultipliedAlpha(true);
    }

    sg_mtl_image_info info = sg_mtl_query_image_info(texture.getImage());
    id<MTLTexture> dst = (__bridge id<MTLTexture>)const_cast<void *>(info.tex[0]);
    if (!dst) return false;

    id<MTLCommandBuffer> cb = [impl_->queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit copyFromTexture:frame
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(w, h, 1)
                toTexture:dst
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    [cb commit];
    [cb waitUntilCompleted];

    impl_->width = w;
    impl_->height = h;
    impl_->newFrame.store(true); // a fresh frame was just pulled
    return true;
}

bool SyphonClient::isFrameNew() const { return impl_->newFrame.load(); }

string SyphonClient::getServerName() const { return impl_->serverName; }
int SyphonClient::getWidth() const { return impl_->width; }
int SyphonClient::getHeight() const { return impl_->height; }

} // namespace tcx
