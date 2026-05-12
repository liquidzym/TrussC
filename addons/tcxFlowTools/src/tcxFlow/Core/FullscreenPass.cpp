#include "FullscreenPass.h"

namespace tcx::flow {

void FullscreenPass::drawSolid(const tc::Color& color) {
    tc::setColor(color);
    tc::drawRect(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
}

} // namespace tcx::flow
