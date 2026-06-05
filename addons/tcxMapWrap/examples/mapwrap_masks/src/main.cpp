// =============================================================================
// mapwrap_masks — Mask system demo
// =============================================================================
//
// Demonstrates the MapWrapMask API:
//   1. Quad surface with polygon mask
//   2. Ellipse mask
//   3. Inverted mask
//   4. Subtract mask
//   5. Alpha texture mask with a soft radial alpha source
//   6. Mask save/load
//   7. Touch-sim mask editing
//   8. Mask validation warning
//
// Keyboard shortcuts:
//   1-6     → Switch between demo surfaces (each has different mask type)
//   M       → Add polygon mask to selected surface
//   B       → Add Bezier mask to selected surface
//   E       → Add ellipse mask to selected surface
//   A       → Add alpha texture mask to selected surface
//   I       → Toggle inversion on last mask of selected surface
//   X       → Cycle mask operation (Add → Subtract → Intersect)
//   S       → Save project (includes masks)
//   L       → Load project
//   F1      → Toggle touch mode (larger mask handles)
//   Escape  → Deselect
// =============================================================================

#include <TrussC.h>
#include <tcxMapWrap.h>
#include "../../shared/MapWrapDemoDraw.h"

#include <cmath>
#include <filesystem>

using namespace std;
using namespace tc;
using namespace tcx::mapwrap;

class MapWrapMasksApp : public tc::App {
public:
    MapWrapEngine engine;
    SourceId imageSourceId;
    SourceId colorBarsSourceId;
    SourceId alphaMaskSourceId;
    bool touchMode = false;

    SurfaceId surfacePolygon;
    SurfaceId surfaceEllipse;
    SurfaceId surfaceInverted;
    SurfaceId surfaceSubtract;
    SurfaceId surfaceMulti;
    SurfaceId surfaceBezier;

    void setup() override {
        MapWrapI18n::instance().detectAndSetLanguage();

        imageSourceId = engine.sources().addBuiltinPattern(
            "Mask Demo Pattern", BuiltinPatternKind::Grid, Vec2(1920, 1080));
        colorBarsSourceId = engine.sources().addBuiltinPattern(
            "Color Bars", BuiltinPatternKind::ColorBars, Vec2(1920, 1080));
        alphaMaskSourceId = engine.sources().addBuiltinPattern(
            "Soft Alpha Mask", BuiltinPatternKind::AlphaRadial, Vec2(512, 512));

        engine.setCanvasSize(Vec2(getWidth(), getHeight()));
        engine.editor().setMode(EditMode::SurfaceEdit);

        // --- 1. Quad surface with polygon mask ---
        auto s1 = engine.document().createQuadSurface("Polygon Mask");
        s1->setSource(imageSourceId);
        auto& d1 = s1->destinationPoints();
        d1 = {{ Vec2(0.03f,0.08f), Vec2(0.31f,0.08f), Vec2(0.31f,0.42f), Vec2(0.03f,0.42f) }};
        MapWrapMask polyMask;
        polyMask.kind = MaskKind::Polygon;
        polyMask.operation = MaskOperation::Add;
        polyMask.enabled = true;
        polyMask.name = "Diamond";
        polyMask.points = {
            Vec2(0.5f, 0.05f), Vec2(0.95f, 0.5f),
            Vec2(0.5f, 0.95f), Vec2(0.05f, 0.5f)
        };
        s1->masks().push_back(polyMask);
        engine.document().addSurface(s1);
        surfacePolygon = s1->id();

        // --- 2. Ellipse mask ---
        auto s2 = engine.document().createQuadSurface("Ellipse Mask");
        s2->setSource(imageSourceId);
        auto& d2 = s2->destinationPoints();
        d2 = {{ Vec2(0.36f,0.08f), Vec2(0.64f,0.08f), Vec2(0.64f,0.42f), Vec2(0.36f,0.42f) }};
        MapWrapMask ellipseMask;
        ellipseMask.kind = MaskKind::Ellipse;
        ellipseMask.operation = MaskOperation::Add;
        ellipseMask.enabled = true;
        ellipseMask.name = "Center Ellipse";
        ellipseMask.rect = tcx::mapwrap::Rect(0.2f, 0.2f, 0.6f, 0.6f);
        ellipseMask.featherNorm = 0.08f;
        s2->masks().push_back(ellipseMask);
        engine.document().addSurface(s2);
        surfaceEllipse = s2->id();

        // --- 3. Inverted mask ---
        auto s3 = engine.document().createQuadSurface("Inverted Mask");
        s3->setSource(imageSourceId);
        auto& d3 = s3->destinationPoints();
        d3 = {{ Vec2(0.69f,0.08f), Vec2(0.97f,0.08f), Vec2(0.97f,0.42f), Vec2(0.69f,0.42f) }};
        MapWrapMask invMask;
        invMask.kind = MaskKind::Ellipse;
        invMask.operation = MaskOperation::Add;
        invMask.enabled = true;
        invMask.inverted = true;
        invMask.name = "Inverted Circle";
        invMask.rect = tcx::mapwrap::Rect(0.25f, 0.25f, 0.5f, 0.5f);
        s3->masks().push_back(invMask);
        engine.document().addSurface(s3);
        surfaceInverted = s3->id();

        // --- 4. Subtract mask ---
        auto s4 = engine.document().createQuadSurface("Subtract Mask");
        s4->setSource(imageSourceId);
        auto& d4 = s4->destinationPoints();
        d4 = {{ Vec2(0.03f,0.54f), Vec2(0.31f,0.54f), Vec2(0.31f,0.92f), Vec2(0.03f,0.92f) }};
        // Full-area add mask
        MapWrapMask addMask;
        addMask.kind = MaskKind::Rectangle;
        addMask.operation = MaskOperation::Add;
        addMask.enabled = true;
        addMask.name = "Full Area";
        addMask.rect = tcx::mapwrap::Rect(0.0f, 0.0f, 1.0f, 1.0f);
        s4->masks().push_back(addMask);
        // Subtract a center region
        MapWrapMask subMask;
        subMask.kind = MaskKind::Ellipse;
        subMask.operation = MaskOperation::Subtract;
        subMask.enabled = true;
        subMask.name = "Cutout";
        subMask.rect = tcx::mapwrap::Rect(0.3f, 0.3f, 0.4f, 0.4f);
        s4->masks().push_back(subMask);
        engine.document().addSurface(s4);
        surfaceSubtract = s4->id();

        // --- Alpha texture mask surface ---
        auto s5 = engine.document().createQuadSurface("Alpha Texture Mask");
        s5->setSource(colorBarsSourceId);
        auto& d5 = s5->destinationPoints();
        d5 = {{ Vec2(0.36f,0.54f), Vec2(0.64f,0.54f), Vec2(0.64f,0.92f), Vec2(0.36f,0.92f) }};
        MapWrapMask alphaMask;
        alphaMask.kind = MaskKind::AlphaTexture;
        alphaMask.operation = MaskOperation::Add;
        alphaMask.enabled = true;
        alphaMask.name = "Soft Radial Alpha";
        alphaMask.rect = tcx::mapwrap::Rect(0.04f, 0.04f, 0.92f, 0.92f);
        alphaMask.alphaTextureSource = alphaMaskSourceId;
        s5->masks().push_back(alphaMask);
        engine.document().addSurface(s5);
        surfaceMulti = s5->id();

        // --- Bezier mask on a Bezier surface ---
        auto s6 = engine.document().createBezierSurface(4, 4, "Bezier Mask");
        s6->setSource(imageSourceId);
        s6->setMeshResolution(24);
        for (int row = 0; row < s6->controlRows(); ++row) {
            for (int col = 0; col < s6->controlCols(); ++col) {
                float u = float(col) / float(s6->controlCols() - 1);
                float v = float(row) / float(s6->controlRows() - 1);
                float lift = std::sin(u * (float)M_PI) * std::sin(v * (float)M_PI) * 0.05f;
                s6->setControlPoint(col, row, Vec2(0.69f + u * 0.28f,
                                                   0.54f + v * 0.38f - lift));
            }
        }
        MapWrapMask bezierMask;
        bezierMask.kind = MaskKind::Bezier;
        bezierMask.operation = MaskOperation::Add;
        bezierMask.enabled = true;
        bezierMask.name = "Bezier Window";
        bezierMask.points = {
            Vec2(0.15f, 0.20f), Vec2(0.48f, 0.04f),
            Vec2(0.88f, 0.26f), Vec2(0.77f, 0.82f),
            Vec2(0.26f, 0.92f)
        };
        s6->masks().push_back(bezierMask);
        engine.document().addSurface(s6);
        surfaceBezier = s6->id();
        engine.editor().selectSurface(surfaceBezier);
    }

    void update() override {
        float dt = static_cast<float>(getDeltaTime());
        engine.update(dt);
    }

    void draw() override {
        clear(0.06f, 0.06f, 0.08f);

        mapwrap_demo::DrawOptions drawOptions;
        drawOptions.handleRadius = touchMode ? 9.0f : 5.0f;
        mapwrap_demo::drawDemo(engine, getWidth(), getHeight(), drawOptions);

        drawMaskOverlay();
    }

    void drawMaskOverlay() {
        int y = 20;

        auto sel = engine.editor().selectedSurface();
        setColor(1.0f, 1.0f, 1.0f, 0.9f);
        mapwrap_demo::drawBitmapText("Selected: " + (sel.empty() ? string("(none)") : sel), 12, y);
        y += 18;

        // 8. Mask validation warning
        if (!sel.empty()) {
            auto surf = engine.document().getSurface(sel);
            if (surf) {
                int maskIdx = 0;
                for (auto& mask : surf->masks()) {
                    auto val = mask.validateGeometry();
                    if (!val.valid) {
                        setColor(1.0f, 0.3f, 0.2f, 0.9f);
                        mapwrap_demo::drawBitmapText("Mask " + to_string(maskIdx) + " INVALID: " + val.message, 12, y);
                        y += 16;
                    }
                    maskIdx++;
                }
                setColor(0.7f, 0.9f, 0.7f, 0.8f);
                mapwrap_demo::drawBitmapText("Masks: " + to_string(surf->masks().size()), 12, y);
                y += 16;
            }
        }

        // Input mode indicator
        if (touchMode) {
            setColor(0.2f, 0.8f, 1.0f, 0.9f);
            mapwrap_demo::drawBitmapText("TOUCH MODE", 12, y);
        }
        y += 18;

        // Help
        setColor(0.5f, 0.5f, 0.5f, 0.7f);
        mapwrap_demo::drawBitmapText("[1-6]Select  [M]Polygon  [B]Bezier  [E]Ellipse  [A]Alpha  [I]Invert  [X]Op  [S]ave [L]oad  [F1]Touch", 12, getHeight() - 16);
    }

    // --- Mouse ---

    void mousePressed(const MouseEventArgs& e) override {
        Vec2 pos(e.pos.x, e.pos.y);
        PointerEvent pe = touchMode ? PointerEvent::touch(pos, 0) : PointerEvent::mouse(pos, e.button);
        pe.type = PointerEvent::Type::Down;
        engine.editor().pointerDown(pe);
        redraw();
    }

    void mouseDragged(const MouseDragEventArgs& e) override {
        Vec2 pos(e.pos.x, e.pos.y);
        PointerEvent pe = touchMode ? PointerEvent::touch(pos, 0) : PointerEvent::mouse(pos, e.button);
        pe.type = PointerEvent::Type::Move;
        engine.editor().pointerMove(pe);
        redraw();
    }

    void mouseReleased(const MouseEventArgs& e) override {
        Vec2 pos(e.pos.x, e.pos.y);
        PointerEvent pe = touchMode ? PointerEvent::touch(pos, 0) : PointerEvent::mouse(pos, e.button);
        pe.type = PointerEvent::Type::Up;
        engine.editor().pointerUp(pe);
        redraw();
    }

    // --- Keyboard ---

    void keyPressed(int key) override {
        auto sel = engine.editor().selectedSurface();

        switch (key) {
            // Quick-select demo surfaces
            case '1': engine.editor().selectSurface(surfacePolygon); break;
            case '2': engine.editor().selectSurface(surfaceEllipse); break;
            case '3': engine.editor().selectSurface(surfaceInverted); break;
            case '4': engine.editor().selectSurface(surfaceSubtract); break;
            case '5': engine.editor().selectSurface(surfaceMulti); break;
            case '6': engine.editor().selectSurface(surfaceBezier); break;

            // Add polygon mask
            case 'M': {
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s) {
                        MapWrapMask mask;
                        mask.kind = MaskKind::Polygon;
                        mask.operation = MaskOperation::Add;
                        mask.enabled = true;
                        mask.name = "Polygon Mask";
                        mask.points = {
                            Vec2(0.5f, 0.1f), Vec2(0.9f, 0.3f),
                            Vec2(0.8f, 0.9f), Vec2(0.2f, 0.9f),
                            Vec2(0.1f, 0.3f)
                        };
                        s->masks().push_back(mask);
                        s->markDirty();
                        engine.document().markDirty();
                        logNotice("Polygon mask added");
                    }
                }
                break;
            }

            // Add Bezier mask
            case 'B': {
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s) {
                        MapWrapMask mask;
                        mask.kind = MaskKind::Bezier;
                        mask.operation = MaskOperation::Add;
                        mask.enabled = true;
                        mask.name = "Bezier Mask";
                        mask.points = {
                            Vec2(0.12f, 0.18f), Vec2(0.48f, 0.08f),
                            Vec2(0.90f, 0.30f), Vec2(0.78f, 0.86f),
                            Vec2(0.24f, 0.88f)
                        };
                        s->masks().push_back(mask);
                        s->markDirty();
                        engine.document().markDirty();
                        logNotice("Bezier mask added");
                    }
                }
                break;
            }

            // Add ellipse mask
            case 'E': {
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s) {
                        MapWrapMask mask;
                        mask.kind = MaskKind::Ellipse;
                        mask.operation = MaskOperation::Add;
                        mask.enabled = true;
                        mask.name = "Ellipse Mask";
                        mask.rect = tcx::mapwrap::Rect(0.2f, 0.2f, 0.6f, 0.6f);
                        s->masks().push_back(mask);
                        s->markDirty();
                        engine.document().markDirty();
                        logNotice("Ellipse mask added");
                    }
                }
                break;
            }

            // Add alpha texture mask
            case 'A': {
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s) {
                        MapWrapMask mask;
                        mask.kind = MaskKind::AlphaTexture;
                        mask.operation = MaskOperation::Add;
                        mask.enabled = true;
                        mask.name = "Alpha Texture Mask";
                        mask.rect = tcx::mapwrap::Rect(0.05f, 0.05f, 0.90f, 0.90f);
                        mask.alphaTextureSource = alphaMaskSourceId;
                        s->masks().push_back(mask);
                        s->markDirty();
                        engine.document().markDirty();
                        logNotice("Alpha texture mask added");
                    }
                }
                break;
            }

            // Toggle invert on last mask
            case 'I': {
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s && !s->masks().empty()) {
                        auto& mask = s->masks().back();
                        mask.inverted = !mask.inverted;
                        s->markDirty();
                        engine.document().markDirty();
                        logNotice(string("Mask inverted: ") + (mask.inverted ? "YES" : "NO"));
                    }
                }
                break;
            }

            // Cycle operation on last mask
            case 'X': {
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s && !s->masks().empty()) {
                        auto& mask = s->masks().back();
                        switch (mask.operation) {
                            case MaskOperation::Add:       mask.operation = MaskOperation::Subtract; break;
                            case MaskOperation::Subtract:  mask.operation = MaskOperation::Intersect; break;
                            case MaskOperation::Intersect: mask.operation = MaskOperation::Add; break;
                        }
                        s->markDirty();
                        engine.document().markDirty();
                        logNotice("Mask operation: " + mask.operationName());
                    }
                }
                break;
            }

            // 6. Save project (includes masks)
            case 'S': {
                std::filesystem::create_directories("data");
                auto result = MapWrapSerialization::saveToFile(
                    engine.document(), engine.sources(), "data/mapwrap_masks.json");
                if (result.ok) logNotice("Project saved");
                else logWarning("Save failed: " + result.message);
                break;
            }

            // Load project
            case 'L': {
                auto result = MapWrapSerialization::loadFromFile(
                    engine.document(), engine.sources(), "data/mapwrap_masks.json");
                if (result.ok) logNotice("Project loaded");
                else logWarning("Load failed: " + result.message);
                break;
            }

            // 7. Toggle touch mode
            case 290: { // F1
                touchMode = !touchMode;
                logNotice(string("Touch mode: ") + (touchMode ? "ON" : "OFF"));
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
    settings.title = "mapwrap_masks";
    return TC_RUN_APP(MapWrapMasksApp, settings);
}
