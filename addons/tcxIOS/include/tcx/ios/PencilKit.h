#pragma once

#include "Types.h"

#include <cstdint>
#include <vector>

namespace tcx::ios {

struct PencilCanvasRequest {
    bool showToolPicker = true;
};

struct PencilDrawingData {
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> png;
    int pixelWidth = 0;
    int pixelHeight = 0;
};

class PencilCanvas {
public:
    void present(const PencilCanvasRequest& request, Completion<void> done);
    void dismiss();
    Result<PencilDrawingData> capture() const;
    void clear();
};

PencilCanvas& pencilCanvas();

} // namespace tcx::ios
