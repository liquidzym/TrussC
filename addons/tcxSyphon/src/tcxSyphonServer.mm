#include "tcxSyphonServer.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <Syphon/SyphonMetalServer.h>

#include <TrussC.h>

using namespace std;
using namespace tc;

namespace tcx {

struct SyphonServer::Impl {
    SyphonMetalServer *server = nil;
    id<MTLCommandQueue> queue = nil;
    string name;
    int width = 0;
    int height = 0;
    uint64_t frameCount = 0;
};

SyphonServer::SyphonServer() : impl_(make_unique<Impl>()) {}
SyphonServer::~SyphonServer() { close(); }
SyphonServer::SyphonServer(SyphonServer &&) noexcept = default;
SyphonServer &SyphonServer::operator=(SyphonServer &&) noexcept = default;

bool SyphonServer::setup(const string &name) {
    close();

    id<MTLDevice> device = (__bridge id<MTLDevice>)const_cast<void *>(sg_mtl_device());
    if (!device) {
        logError() << "[tcxSyphon] SyphonServer::setup - Metal device unavailable";
        return false;
    }

    NSString *nsName = [NSString stringWithUTF8String:name.c_str()];
    impl_->server = [[SyphonMetalServer alloc] initWithName:nsName device:device options:nil];
    if (!impl_->server) {
        logError() << "[tcxSyphon] SyphonServer::setup - failed to create SyphonMetalServer";
        return false;
    }
    impl_->queue = [device newCommandQueue];
    impl_->name = name;
    return true;
}

void SyphonServer::close() {
    if (impl_->server) {
        [impl_->server stop];
        impl_->server = nil;
    }
    impl_->queue = nil;
    impl_->width = 0;
    impl_->height = 0;
    impl_->frameCount = 0;
}

bool SyphonServer::isSetup() const { return impl_->server != nil; }

bool SyphonServer::publish(const Texture &texture) {
    if (!impl_->server || !texture.isAllocated()) return false;

    int w = texture.getWidth();
    int h = texture.getHeight();

    sg_mtl_image_info info = sg_mtl_query_image_info(texture.getImage());
    id<MTLTexture> mtlTex = (__bridge id<MTLTexture>)const_cast<void *>(info.tex[0]);
    if (!mtlTex) return false;

    id<MTLCommandBuffer> cb = [impl_->queue commandBuffer];
    // Syphon converts to its BGRA8 surface internally (fast blit when formats
    // match, sampling-shader render otherwise). TrussC/Metal textures have a
    // top-left origin, while Syphon's shared surface is bottom-left, so we
    // publish flipped:YES — this is what standard Syphon clients expect. (Our
    // own SyphonClient receives a bottom-left surface; example-client draws it
    // with drawFlippedY to bring it back upright.)
    [impl_->server publishFrameTexture:mtlTex
                       onCommandBuffer:cb
                           imageRegion:NSMakeRect(0, 0, w, h)
                               flipped:YES];
    [cb commit];

    impl_->width = w;
    impl_->height = h;
    impl_->frameCount++;
    return true;
}

bool SyphonServer::publish(Fbo &fbo) {
    return publish(fbo.getTexture());
}

string SyphonServer::getName() const { return impl_->name; }
int SyphonServer::getWidth() const { return impl_->width; }
int SyphonServer::getHeight() const { return impl_->height; }
uint64_t SyphonServer::getFrameCount() const { return impl_->frameCount; }

bool SyphonServer::hasClients() const {
    return impl_->server ? (bool)[impl_->server hasClients] : false;
}

} // namespace tcx
