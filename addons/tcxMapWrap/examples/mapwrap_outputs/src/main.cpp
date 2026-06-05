// =============================================================================
// mapwrap_outputs — Output system demo
// =============================================================================
//
// Demonstrates the MapWrapOutput and stage output API:
//   1. Default output — the main output with full-canvas region
//   2. Output canvas region editing — adjust the region of the canvas
//      that maps to the physical display
//   3. Output test pattern — show a built-in pattern on the output
//   4. Output color correction — brightness, contrast, gamma per output
//   5. Output mask — apply a mask to an output
//   6. Output bounds overlay — visualize output regions on the canvas
//
// Keyboard shortcuts:
//   T       → Toggle test pattern on output
//   B       → Toggle output bounds overlay
//   R       → Reset output canvas region to full
//   C       → Cycle color correction presets
//   M       → Add mask to output
//   + / -   → Adjust output brightness
//   [ / ]   → Shrink / Expand output canvas region width
//   Escape  → Deselect
// =============================================================================

#include <TrussC.h>
#include <tcxMapWrap.h>
#include "../../shared/MapWrapDemoDraw.h"

using namespace std;
using namespace tc;
using namespace tcx::mapwrap;

class MapWrapOutputsApp : public tc::App {
public:
    MapWrapEngine engine;
    SourceId imageSourceId;
    SourceId calibrationSourceId;
    int colorPresetIndex = 0;
    bool showBoundsOverlay = true;

    void setup() override {
        MapWrapI18n::instance().detectAndSetLanguage();

        imageSourceId = engine.sources().addBuiltinPattern(
            "Mapped Color Bars", BuiltinPatternKind::ColorBars, Vec2(1920, 1080));
        calibrationSourceId = engine.sources().addBuiltinPattern(
            "Calibration", BuiltinPatternKind::ColorBars, Vec2(1920, 1080));

        engine.setCanvasSize(Vec2(getWidth(), getHeight()));

        // Create a surface mapped to the image
        auto quad = engine.document().createQuadSurface("Mapped Surface");
        quad->setSource(imageSourceId);
        engine.document().addSurface(quad);

        // 1. Default output — ensure it exists
        auto& defaultOutput = engine.document().stage().ensureDefaultOutput();
        defaultOutput.name = "Main Output";
        defaultOutput.pixelSize = Vec2(1920, 1080);
        defaultOutput.canvasRegionNorm = tcx::mapwrap::Rect(0, 0, 1, 1);

        // 4. Set initial color correction on output
        defaultOutput.colorCorrection.enabled = true;
        defaultOutput.colorCorrection.brightness = 1.0f;
        defaultOutput.colorCorrection.contrast = 1.0f;
        defaultOutput.colorCorrection.saturation = 1.0f;
    }

    void update() override {
        float dt = static_cast<float>(getDeltaTime());
        engine.update(dt);
    }

    void draw() override {
        clear(0.05f, 0.05f, 0.07f);

        mapwrap_demo::DrawOptions drawOptions;
        drawOptions.showOutputBounds = showBoundsOverlay;
        mapwrap_demo::drawDemo(engine, getWidth(), getHeight(), drawOptions);

        drawOutputOverlay();
    }

    void drawOutputOverlay() {
        int y = 20;
        auto& outputs = engine.document().stage().outputs();

        setColor(1.0f, 1.0f, 1.0f, 0.9f);
        mapwrap_demo::drawBitmapText("Outputs: " + to_string(outputs.size()), 12, y);
        y += 18;

        for (auto& out : outputs) {
            // Output name and region
            auto& r = out.canvasRegionNorm;
            char regionStr[128];
            snprintf(regionStr, sizeof(regionStr), "%.2f,%.2f %.2fx%.2f",
                     r.x, r.y, r.w, r.h);

            setColor(0.8f, 0.9f, 1.0f, 0.9f);
            mapwrap_demo::drawBitmapText(out.name + " [" + out.id + "]  Region: " + string(regionStr), 12, y);
            y += 16;

            // Test pattern status
            mapwrap_demo::drawBitmapText("  Test pattern: " + string(out.showTestPattern ? "ON" : "OFF"), 12, y);
            y += 16;

            // Color correction
            auto& cc = out.colorCorrection;
            mapwrap_demo::drawBitmapText("  Brightness: " + to_string(cc.brightness).substr(0, 5)
                + "  Contrast: " + to_string(cc.contrast).substr(0, 5)
                + "  Saturation: " + to_string(cc.saturation).substr(0, 5), 12, y);
            y += 16;

            // Masks
            mapwrap_demo::drawBitmapText("  Masks: " + to_string(out.masks.size()), 12, y);
            y += 20;
        }

        // 6. Output bounds overlay info
        if (showBoundsOverlay) {
            setColor(0.5f, 1.0f, 0.5f, 0.7f);
            mapwrap_demo::drawBitmapText("Output bounds overlay: ON", 12, y);
        } else {
            setColor(0.5f, 0.5f, 0.5f, 0.7f);
            mapwrap_demo::drawBitmapText("Output bounds overlay: OFF", 12, y);
        }
        y += 18;

        // Help
        setColor(0.5f, 0.5f, 0.5f, 0.7f);
        mapwrap_demo::drawBitmapText("[T]est pattern  [B]ounds  [R]eset region  [C]olor preset  [M]ask  [+/-]Bright  [/]Region width", 12, getHeight() - 16);
    }

    // --- Mouse ---

    void mousePressed(const MouseEventArgs& e) override {
        PointerEvent pe = PointerEvent::mouse(Vec2(e.pos.x, e.pos.y), e.button);
        pe.type = PointerEvent::Type::Down;
        engine.editor().pointerDown(pe);
        redraw();
    }

    void mouseDragged(const MouseDragEventArgs& e) override {
        PointerEvent pe = PointerEvent::mouse(Vec2(e.pos.x, e.pos.y), e.button);
        pe.type = PointerEvent::Type::Move;
        engine.editor().pointerMove(pe);
        redraw();
    }

    void mouseReleased(const MouseEventArgs& e) override {
        PointerEvent pe = PointerEvent::mouse(Vec2(e.pos.x, e.pos.y), e.button);
        pe.type = PointerEvent::Type::Up;
        engine.editor().pointerUp(pe);
        redraw();
    }

    // --- Keyboard ---

    void keyPressed(int key) override {
        auto& outputs = engine.document().stage().outputs();
        if (outputs.empty()) return;
        auto& out = outputs[0];

        switch (key) {
            // 3. Toggle test pattern
            case 'T': {
                out.showTestPattern = !out.showTestPattern;
                logNotice(string("Test pattern: ") + (out.showTestPattern ? "ON" : "OFF"));
                break;
            }

            // 6. Toggle bounds overlay
            case 'B': {
                showBoundsOverlay = !showBoundsOverlay;
                logNotice(string("Bounds overlay: ") + (showBoundsOverlay ? "ON" : "OFF"));
                break;
            }

            // 2. Reset canvas region to full
            case 'R': {
                out.canvasRegionNorm = tcx::mapwrap::Rect(0, 0, 1, 1);
                logNotice("Output region reset to full canvas");
                break;
            }

            // 4. Cycle color correction presets
            case 'C': {
                colorPresetIndex = (colorPresetIndex + 1) % 4;
                switch (colorPresetIndex) {
                    case 0: // Normal
                        out.colorCorrection.brightness = 1.0f;
                        out.colorCorrection.contrast = 1.0f;
                        out.colorCorrection.saturation = 1.0f;
                        out.colorCorrection.gamma = Vec3(1, 1, 1);
                        break;
                    case 1: // Bright
                        out.colorCorrection.brightness = 1.3f;
                        out.colorCorrection.contrast = 1.1f;
                        out.colorCorrection.saturation = 1.0f;
                        break;
                    case 2: // High contrast
                        out.colorCorrection.brightness = 1.0f;
                        out.colorCorrection.contrast = 1.5f;
                        out.colorCorrection.saturation = 1.2f;
                        break;
                    case 3: // Desaturated
                        out.colorCorrection.brightness = 1.0f;
                        out.colorCorrection.contrast = 1.0f;
                        out.colorCorrection.saturation = 0.3f;
                        break;
                }
                logNotice("Color preset: " + to_string(colorPresetIndex));
                break;
            }

            // 5. Add mask to output
            case 'M': {
                MapWrapMask mask;
                mask.kind = MaskKind::Ellipse;
                mask.operation = MaskOperation::Add;
                mask.enabled = true;
                mask.name = "Output Vignette";
                mask.rect = tcx::mapwrap::Rect(0.1f, 0.1f, 0.8f, 0.8f);
                mask.featherNorm = 0.05f;
                out.masks.push_back(mask);
                logNotice("Output mask added");
                break;
            }

            // 4. Adjust brightness
            case '=': case '+': {
                out.colorCorrection.brightness = min(2.0f, out.colorCorrection.brightness + 0.1f);
                logNotice("Brightness: " + to_string(out.colorCorrection.brightness));
                break;
            }
            case '-': {
                out.colorCorrection.brightness = max(0.0f, out.colorCorrection.brightness - 0.1f);
                logNotice("Brightness: " + to_string(out.colorCorrection.brightness));
                break;
            }

            // 2. Adjust canvas region width
            case '[': {
                auto& r = out.canvasRegionNorm;
                float shrink = 0.05f;
                r.x += shrink;
                r.w = max(0.1f, r.w - shrink * 2);
                logNotice("Region width shrunk");
                break;
            }
            case ']': {
                auto& r = out.canvasRegionNorm;
                float expand = 0.05f;
                r.x = max(0.0f, r.x - expand);
                r.w = min(1.0f - r.x, r.w + expand * 2);
                logNotice("Region width expanded");
                break;
            }

            case 256: engine.editor().deselect(); break;
        }
        redraw();
    }
};

int main() {
    WindowSettings settings;
    settings.width = 1280;
    settings.height = 720;
    settings.title = "mapwrap_outputs";
    return TC_RUN_APP(MapWrapOutputsApp, settings);
}
