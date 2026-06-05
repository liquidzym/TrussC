// =============================================================================
// mapwrap_basic — Minimal tcxMapWrap example
// =============================================================================
//
// Demonstrates the essential workflow:
//   1. Register a built-in pattern source
//   2. Create a quad surface
//   3. Drag the 4 corner handles with the mouse
//   4. Keyboard shortcuts for surface editing and persistence
//   5. Calibration pattern source
//   6. Toggle test pattern overlay (T key)
//   7. Output bounds overlay
//   8. Simple polygon mask demo
//   9. Auto-detect language at startup
//  10. Display ASCII mode name and stats in overlay
//
// Keyboard shortcuts:
//   1       → Presentation mode
//   2       → SurfaceEdit mode
//   S       → Save project
//   L       → Load project
//   T       → Toggle test pattern on selected surface
//   M       → Add a polygon mask to selected surface
//   Escape  → Deselect
// =============================================================================

#include <TrussC.h>
#include <tcxMapWrap.h>
#include "../../shared/MapWrapDemoDraw.h"

#include <filesystem>

using namespace std;
using namespace tc;
using namespace tcx::mapwrap;

class MapWrapBasicApp : public tc::App {
public:
    MapWrapEngine engine;
    SourceId imageSourceId;
    SourceId calibrationSourceId;
    bool showTestPattern = false;

    void setup() override {
        // 9. Auto-detect language at startup
        MapWrapI18n::instance().detectAndSetLanguage();

        // 1. Register a self-contained visible source. No external data file
        // is required for the default demo scene.
        imageSourceId = engine.sources().addBuiltinPattern(
            "Demo Color Bars", BuiltinPatternKind::ColorBars, Vec2(1920, 1080));

        // 5. Calibration pattern source
        calibrationSourceId = engine.sources().addBuiltinPattern(
            "Calibration Grid", BuiltinPatternKind::Grid, Vec2(1920, 1080));

        // 2. Create a quad surface
        auto quad = engine.document().createQuadSurface("Main Quad");
        quad->setSource(imageSourceId);
        engine.document().addSurface(quad);

        // 7. Ensure default output with bounds overlay enabled
        auto& output = engine.document().stage().ensureDefaultOutput();
        output.showTestPattern = false;

        // Set canvas size
        engine.setCanvasSize(Vec2(getWidth(), getHeight()));
    }

    void update() override {
        float dt = static_cast<float>(getDeltaTime());
        engine.update(dt);
    }

    void draw() override {
        clear(0.08f, 0.08f, 0.1f);

        mapwrap_demo::DrawOptions drawOptions;
        drawOptions.showOutputBounds = true;
        mapwrap_demo::drawDemo(engine, getWidth(), getHeight(), drawOptions);

        // 10. Display ASCII mode name and stats overlay
        drawOverlay();
    }

    void drawOverlay() {
        setColor(1.0f, 1.0f, 1.0f, 0.9f);
        mapwrap_demo::drawBitmapText("Mode: " + string(mapwrap_demo::modeName(engine.editor().mode())), 12, 20);

        // Stats
        auto stats = engine.renderer().stats();
        string statsStr = "Surfaces: " + to_string(engine.document().surfaces().size())
            + "  Drawn: " + to_string(stats.drawnSurfaceCount)
            + "  Missing: " + to_string(stats.missingSourceCount)
            + "  Invalid: " + to_string(stats.invalidSurfaceCount);
        mapwrap_demo::drawBitmapText(statsStr, 12, 38);

        // Language indicator
        mapwrap_demo::drawBitmapText("Lang: " + MapWrapI18n::instance().language(), 12, 56);

        // Keyboard help
        setColor(0.6f, 0.6f, 0.6f, 0.7f);
        mapwrap_demo::drawBitmapText("[1] Presentation  [2] Surface Edit  [S]ave [L]oad  [T]est pattern  [M]ask  [Esc]Deselect", 12, getHeight() - 16);
    }

    // --- Mouse: forward to editor for corner dragging ---

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

    // --- Keyboard shortcuts ---

    void keyPressed(int key) override {
        switch (key) {
            // 4. Mode switching
            case '1': engine.editor().setMode(EditMode::Presentation); break;
            case '2': engine.editor().setMode(EditMode::SurfaceEdit); break;
            case '3':
            case '4':
                logNotice("This example exposes Presentation and Surface Edit only");
                break;

            // Save
            case 'S': {
                std::filesystem::create_directories("data");
                auto result = MapWrapSerialization::saveToFile(
                    engine.document(), engine.sources(), "data/mapwrap_basic.json");
                if (result.ok) logNotice("Project saved");
                else logWarning("Save failed: " + result.message);
                break;
            }

            // Load
            case 'L': {
                auto result = MapWrapSerialization::loadFromFile(
                    engine.document(), engine.sources(), "data/mapwrap_basic.json");
                if (result.ok) logNotice("Project loaded");
                else logWarning("Load failed: " + result.message);
                break;
            }

            // 6. Toggle test pattern
            case 'T': {
                showTestPattern = !showTestPattern;
                auto& outputs = engine.document().stage().outputs();
                if (!outputs.empty()) {
                    outputs[0].showTestPattern = showTestPattern;
                }
                logNotice("Test pattern: " + string(showTestPattern ? "ON" : "OFF"));
                break;
            }

            // 8. Simple polygon mask demo
            case 'M': {
                auto sel = engine.editor().selectedSurface();
                if (!sel.empty()) {
                    auto surf = engine.document().getSurface(sel);
                    if (surf) {
                        MapWrapMask mask;
                        mask.kind = MaskKind::Polygon;
                        mask.operation = MaskOperation::Add;
                        mask.enabled = true;
                        // Diamond-shaped mask
                        mask.points = {
                            Vec2(0.5f, 0.0f),
                            Vec2(1.0f, 0.5f),
                            Vec2(0.5f, 1.0f),
                            Vec2(0.0f, 0.5f)
                        };
                        surf->masks().push_back(mask);
                        logNotice("Polygon mask added");
                    }
                } else {
                    logWarning("Select a surface first (click on it in SurfaceEdit mode)");
                }
                break;
            }

            // Deselect
            case 256: // Escape
                engine.editor().deselect();
                break;
        }
        redraw();
    }
};

int main() {
    WindowSettings settings;
    settings.width = 1280;
    settings.height = 720;
    settings.title = "mapwrap_basic";
    return TC_RUN_APP(MapWrapBasicApp, settings);
}
