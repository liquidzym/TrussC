#pragma once
// =============================================================================
// tcxMapWrap — CalibrationPatterns
// =============================================================================

#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class CalibrationPatternSource : public Source {
public:
    SourceKind kind() const override { return SourceKind::BuiltinPattern; }
    SourceId id() const override;
    std::string name() const override;
    Vec2 size() const override;

    void setPattern(BuiltinPatternKind pattern);
    BuiltinPatternKind pattern() const;

    void setSize(Vec2 size) override;
    void setLineThickness(float thickness);
    void setCells(int cols, int rows);
    uint64_t revision() const;

    std::string kindName() const override;

    // Get localized pattern name
    std::string patternName() const;

    void setId(const SourceId& id) override { id_ = id; }
    void setName(const std::string& name) override { name_ = name; }

    // Generate pixel data for the current pattern configuration.
    // `pixels` must point to a buffer of width*height*4 bytes (RGBA8).
    // Returns true on success, false if the pattern is not generatable.
    bool generatePixels(uint8_t* pixels, int width, int height) const;

private:
    SourceId id_;
    std::string name_;
    BuiltinPatternKind pattern_ = BuiltinPatternKind::Grid;
    Vec2 size_ = Vec2(1920, 1080);
    float lineThickness_ = 1.0f;
    int cellsX_ = 16;
    int cellsY_ = 9;
    uint64_t revision_ = 1;
};

// ===========================================================================
// Standalone pattern generation functions
// ===========================================================================
// Each writes RGBA8 pixel data into the provided buffer.
// `pixels` must point to width*height*4 bytes.

/// Checkerboard: alternating black/white cells
void generateCheckerboard(uint8_t* pixels, int width, int height,
                          int cellsX, int cellsY);

/// Grid: white lines on black background
void generateGrid(uint8_t* pixels, int width, int height,
                  int cellsX, int cellsY, int lineThickness);

/// Fine grid: same as grid with higher cell count (64×64)
void generateFineGrid(uint8_t* pixels, int width, int height,
                      int lineThickness);

/// Crosshair: center cross with circle
void generateCrosshair(uint8_t* pixels, int width, int height);

/// Corner labels: "TL" "TR" "BL" "BR" in corners
void generateCornerLabels(uint8_t* pixels, int width, int height);

/// UV gradient: R=U, G=V, B=0
void generateUVGradient(uint8_t* pixels, int width, int height);

/// SMPTE-style color bars
void generateColorBars(uint8_t* pixels, int width, int height);

/// Luminance ramp: left=black, right=white
void generateLumaRamp(uint8_t* pixels, int width, int height);

/// Edge blend ramp: white center, black edges (4-way gradient)
void generateEdgeBlendRamp(uint8_t* pixels, int width, int height);

/// Alpha radial: visible colored radial mask with transparent edges
void generateAlphaRadial(uint8_t* pixels, int width, int height);

/// Numbered cells: each cell gets a unique index number
void generateNumberedCells(uint8_t* pixels, int width, int height,
                           int cellsX, int cellsY);

/// Safe area: action/title safe area markers
void generateSafeArea(uint8_t* pixels, int width, int height);

/// Solid color fill
void generateSolidColor(uint8_t* pixels, int width, int height,
                        uint8_t r, uint8_t g, uint8_t b);

} // namespace mapwrap
} // namespace tcx
