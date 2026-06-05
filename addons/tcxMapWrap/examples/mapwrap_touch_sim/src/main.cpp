// =============================================================================
// mapwrap_touch_sim — Touch input simulation demo
// =============================================================================
//
// Demonstrates how to use tcxMapWrap with touch-like input on desktop:
//   1. Mouse simulates touch pointer (toggle with F1)
//   2. Switch between mouse/touch hit radius (F2)
//   3. Show large handles in touch mode
//   4. No keyboard needed for core operations (click = select/drag)
//   5. Simulate two-finger pan/zoom (Alt+drag)
//   6. Display current hit radius and input mode
//
// This example mirrors the iOS touch workflow on desktop for testing.
// =============================================================================

#include <TrussC.h>
#include <tcxMapWrap.h>
#include "../../shared/MapWrapDemoDraw.h"

using namespace std;
using namespace tc;
using namespace tcx::mapwrap;

class MapWrapTouchApp : public tc::App {
public:
    MapWrapEngine engine;
    SourceId imageSourceId;
    bool touchMode = false;         // F1 toggle
    bool largeHandles = false;      // auto-set with touchMode
    float mouseHitRadius = 8.0f;    // default desktop hit radius
    float touchHitRadius = 24.0f;   // iOS/touch hit radius

    // Two-finger simulation state
    bool twoFingerActive = false;
    Vec2 twoFingerStartCenter;
    float twoFingerStartZoom;
    Vec2 twoFingerStartPan;

    void setup() override {
        MapWrapI18n::instance().detectAndSetLanguage();

        imageSourceId = engine.sources().addBuiltinPattern(
            "Touch Fine Grid", BuiltinPatternKind::FineGrid, Vec2(1920, 1080));

        // Create a quad surface
        auto quad = engine.document().createQuadSurface("Touch Quad");
        quad->setSource(imageSourceId);
        engine.document().addSurface(quad);

        // Create a grid surface for more handles
        auto grid = engine.document().createGridSurface(4, 4, "Touch Grid");
        grid->setSource(imageSourceId);
        for (int row = 0; row <= grid->rows(); ++row) {
            float v = float(row) / float(grid->rows());
            for (int col = 0; col <= grid->cols(); ++col) {
                float u = float(col) / float(grid->cols());
                grid->setGridPoint(col, row, Vec2(0.05f + 0.90f * u, 0.40f + 0.55f * v));
            }
        }
        engine.document().addSurface(grid);

        engine.setCanvasSize(Vec2(getWidth(), getHeight()));
        engine.editor().setMode(EditMode::SurfaceEdit);
    }

    void update() override {
        float dt = static_cast<float>(getDeltaTime());
        engine.update(dt);
    }

    void draw() override {
        clear(0.06f, 0.06f, 0.08f);

        mapwrap_demo::DrawOptions drawOptions;
        drawOptions.showOutputBounds = true;
        drawOptions.handleRadius = touchMode ? 9.0f : 5.0f;
        mapwrap_demo::drawDemo(engine, getWidth(), getHeight(), drawOptions);

        drawTouchOverlay();
    }

    void drawTouchOverlay() {
        // 6. Display current hit radius and input mode
        int y = 20;
        float hitRadius = touchMode ? touchHitRadius : mouseHitRadius;

        // Mode indicator
        if (touchMode) {
            setColor(0.2f, 0.8f, 1.0f, 0.9f);
            mapwrap_demo::drawBitmapText("INPUT: TOUCH", 12, y);
        } else {
            setColor(0.7f, 0.7f, 0.7f, 0.9f);
            mapwrap_demo::drawBitmapText("INPUT: MOUSE", 12, y);
        }
        y += 18;

        setColor(0.9f, 0.9f, 0.9f, 0.8f);
        mapwrap_demo::drawBitmapText("Hit radius: " + to_string((int)hitRadius) + " px", 12, y);
        y += 18;

        mapwrap_demo::drawBitmapText("Handles: " + string(largeHandles ? "LARGE (touch)" : "NORMAL (mouse)"), 12, y);
        y += 18;

        // Two-finger status
        if (twoFingerActive) {
            setColor(1.0f, 0.8f, 0.2f, 0.9f);
            mapwrap_demo::drawBitmapText("TWO-FINGER: Active (pan/zoom)", 12, y);
        }
        y += 18;

        // Viewport info
        auto& vp = engine.editor().viewport();
        mapwrap_demo::drawBitmapText("Zoom: " + to_string((int)(vp.zoom * 100)) + "%  Pan: (" + to_string((int)vp.panPixels.x) + ", " + to_string((int)vp.panPixels.y) + ")", 12, y);

        // Help
        setColor(0.5f, 0.5f, 0.5f, 0.7f);
        mapwrap_demo::drawBitmapText("[F1] Toggle touch/mouse  [F2] Toggle hit radius  [Alt+Drag] Two-finger pan/zoom", 12, getHeight() - 16);

        // Draw touch handle radius indicator near mouse
        if (touchMode) {
            float mx = getMouseX();
            float my = getMouseY();
            setColor(0.2f, 0.8f, 1.0f, 0.3f);
            drawCircle(mx, my, touchHitRadius);
        }
    }

    // --- Pointer events with mode-aware dispatch ---

    void mousePressed(const MouseEventArgs& e) override {
        Vec2 pos(e.pos.x, e.pos.y);

        // 5. Alt+drag simulates two-finger pan/zoom
        if (e.alt) {
            twoFingerActive = true;
            twoFingerStartCenter = pos;
            twoFingerStartZoom = engine.editor().viewport().zoom;
            twoFingerStartPan = engine.editor().viewport().panPixels;
            redraw();
            return;
        }

        // 1. Dispatch as touch or mouse depending on mode
        if (touchMode) {
            PointerEvent pe = PointerEvent::touch(pos, 0);
            pe.type = PointerEvent::Type::Down;
            engine.editor().pointerDown(pe);
        } else {
            PointerEvent pe = PointerEvent::mouse(pos, e.button);
            pe.type = PointerEvent::Type::Down;
            engine.editor().pointerDown(pe);
        }
        redraw();
    }

    void mouseDragged(const MouseDragEventArgs& e) override {
        Vec2 pos(e.pos.x, e.pos.y);

        // 5. Two-finger simulation
        if (twoFingerActive) {
            auto& vp = engine.editor().viewport();

            // Pan: move the viewport by the drag delta
            Vec2 delta(pos.x - twoFingerStartCenter.x, pos.y - twoFingerStartCenter.y);
            vp.panPixels = Vec2(twoFingerStartPan.x + delta.x, twoFingerStartPan.y + delta.y);

            // Zoom: vertical drag scales zoom
            float zoomDelta = 1.0f + (twoFingerStartCenter.y - pos.y) * 0.003f;
            vp.zoom = twoFingerStartZoom * zoomDelta;
            if (vp.zoom < 0.1f) vp.zoom = 0.1f;
            if (vp.zoom > 10.0f) vp.zoom = 10.0f;

            redraw();
            return;
        }

        if (touchMode) {
            PointerEvent pe = PointerEvent::touch(pos, 0);
            pe.type = PointerEvent::Type::Move;
            engine.editor().pointerMove(pe);
        } else {
            PointerEvent pe = PointerEvent::mouse(pos, e.button);
            pe.type = PointerEvent::Type::Move;
            engine.editor().pointerMove(pe);
        }
        redraw();
    }

    void mouseReleased(const MouseEventArgs& e) override {
        Vec2 pos(e.pos.x, e.pos.y);

        if (twoFingerActive) {
            twoFingerActive = false;
            redraw();
            return;
        }

        if (touchMode) {
            PointerEvent pe = PointerEvent::touch(pos, 0);
            pe.type = PointerEvent::Type::Up;
            engine.editor().pointerUp(pe);
        } else {
            PointerEvent pe = PointerEvent::mouse(pos, e.button);
            pe.type = PointerEvent::Type::Up;
            engine.editor().pointerUp(pe);
        }
        redraw();
    }

    // --- Keyboard ---

    void keyPressed(int key) override {
        switch (key) {
            // 1. Toggle touch/mouse mode
            case 290: { // F1
                touchMode = !touchMode;
                largeHandles = touchMode;
                logNotice(string("Input mode: ") + (touchMode ? "TOUCH" : "MOUSE"));
                break;
            }

            // 2. Toggle hit radius
            case 291: { // F2
                if (touchMode) {
                    touchHitRadius = (touchHitRadius == 24.0f) ? 36.0f : 24.0f;
                    logNotice("Touch hit radius: " + to_string((int)touchHitRadius));
                } else {
                    mouseHitRadius = (mouseHitRadius == 8.0f) ? 16.0f : 8.0f;
                    logNotice("Mouse hit radius: " + to_string((int)mouseHitRadius));
                }
                break;
            }

            case 256: engine.editor().deselect(); break;
        }
        redraw();
    }
};

int main() {
    WindowSettings settings;
    settings.width = 1024;
    settings.height = 768;
    settings.title = "mapwrap_touch_sim";
    return TC_RUN_APP(MapWrapTouchApp, settings);
}
