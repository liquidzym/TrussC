#pragma once

#include "FlowTypes.h"

namespace tcx::flow {

TextureFormat chooseRenderableFlowFormat(bool preferFloat = true);
const char* textureFormatName(TextureFormat format);

} // namespace tcx::flow
