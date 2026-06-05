// =============================================================================
// mapwrap_editor — Full-featured tcxMapWrap editor example
// =============================================================================
//
// Demonstrates the complete MapWrapEditor API:
//   1. Add surfaces: Q=Quad, G=Grid/Bilinear, B=Bezier, T=Triangle, C=Circle, P=Polygon
//   2. Delete (Delete key), Duplicate (D key)
//   3. Layer ordering: [ = send backward, ] = bring forward
//   4. Toggle lock (L) and visibility (V) on selected surface
//   5. Undo (Z) / Redo (Shift+Z)
//   6. Mask editing: M = add mask to selected surface
//   7. Modes: 1=Presentation, 2=Surface, 3=Texture, 4=Source, 5=Mask, 6=Output
//   8. Snap toggle (S key)
//   9. Numeric debug inspector display
//  10. Copy/paste geometry and UV
//  11. Geometry warning overlay
//  12. Debug overlay: FPS, selected, mode, source, dirty/missing/invalid counts
//  13. Language toggle: F9 switches zh ↔ en
//  14. Control points: mouse/TAB/arrows select, Shift+arrows nudges current point
//  15. Warp controls: X converts Quad/Grid/Bezier, O toggles Quad perspective,
//      punctuation shortcuts edit lattice and mesh resolution
// =============================================================================

#include <TrussC.h>
#include <tcxMapWrap.h>
#include "../../shared/MapWrapDemoDraw.h"
#include <cmath>
#include <sstream>

using namespace std;
using namespace tc;
using namespace tcx::mapwrap;

class MapWrapEditorApp : public tc::App {
public:
    MapWrapEngine engine;
    SourceId defaultImageSource;
    bool snapEnabled = false;
    bool showDebugInspector = false;

    static string handleKindName(HandleKind kind) {
        switch (kind) {
            case HandleKind::Vertex: return "Vertex";
            case HandleKind::TextureVertex: return "UV";
            case HandleKind::GridPoint: return "Control";
            case HandleKind::RotationHandle: return "Rotation";
            default: return "-";
        }
    }

    void cycleWarpKind() {
        auto sel = engine.editor().selectedSurface();
        if (sel.empty()) return;
        auto s = engine.document().getSurface(sel);
        if (!s) return;

        SurfaceKind next = SurfaceKind::Quad;
        switch (s->kind()) {
            case SurfaceKind::Quad: next = SurfaceKind::Grid; break;
            case SurfaceKind::Grid: next = SurfaceKind::Bezier; break;
            case SurfaceKind::Bezier: next = SurfaceKind::Quad; break;
            default: next = SurfaceKind::Quad; break;
        }

        auto result = engine.editor().convertSelectedTo(next);
        if (!result.ok) logWarning("Convert failed: " + result.message);
        else logNotice("Converted to " + string(mapwrap_demo::surfaceKindName(next)));
    }

    void toggleQuadPerspective() {
        auto sel = engine.editor().selectedSurface();
        if (sel.empty()) return;
        auto s = engine.document().getSurface(sel);
        if (!s || s->kind() != SurfaceKind::Quad) return;
        auto& quad = static_cast<SurfaceQuad&>(*s);
        quad.setPerspectiveCorrection(!quad.perspectiveCorrection());
        logNotice(string("Perspective: ") + (quad.perspectiveCorrection() ? "ON" : "OFF"));
    }

    void setup() override {
        MapWrapI18n::instance().detectAndSetLanguage();

        // Register a default visible source. The example should be useful even
        // when no data/ folder exists.
        defaultImageSource = engine.sources().addBuiltinPattern(
            "Demo Checkerboard", BuiltinPatternKind::Checkerboard, Vec2(1920, 1080));

        // Add a calibration pattern source
        engine.sources().addBuiltinPattern("Calibration", BuiltinPatternKind::Checkerboard, Vec2(1920, 1080));

        createDefaultScene();

        engine.setCanvasSize(Vec2(getWidth(), getHeight()));
        engine.editor().setMode(EditMode::SurfaceEdit);
    }

    void createDefaultScene() {
        // Quad / perspective-style four-corner editing.
        auto quad = engine.document().createQuadSurface("Quad Perspective");
        quad->setSource(defaultImageSource);
        auto& q = quad->destinationPoints();
        q = {{ Vec2(0.04f, 0.08f), Vec2(0.31f, 0.05f), Vec2(0.33f, 0.35f), Vec2(0.06f, 0.38f) }};
        engine.document().addSurface(quad);

        // Bilinear/grid warp, deliberately bowed so the mesh effect is visible.
        auto grid = engine.document().createGridSurface(4, 3, "Bilinear Grid");
        grid->setSource(defaultImageSource);
        grid->setCurvedInterpolation(true);
        grid->setMeshResolution(5);
        for (int row = 0; row <= grid->rows(); ++row) {
            for (int col = 0; col <= grid->cols(); ++col) {
                float u = float(col) / float(grid->cols());
                float v = float(row) / float(grid->rows());
                float bow = std::sin(u * (float)M_PI) * std::sin(v * (float)M_PI);
                grid->setGridPoint(col, row, Vec2(0.37f + u * 0.26f,
                                                  0.07f + v * 0.31f - bow * 0.045f));
            }
        }
        engine.document().addSurface(grid);

        // Bezier surface: 4x4 control lattice sampled into a dense mesh.
        auto bezier = engine.document().createBezierSurface(4, 4, "Bezier Surface");
        bezier->setSource(defaultImageSource);
        bezier->setMeshResolution(28);
        for (int row = 0; row < bezier->controlRows(); ++row) {
            for (int col = 0; col < bezier->controlCols(); ++col) {
                float u = float(col) / float(bezier->controlCols() - 1);
                float v = float(row) / float(bezier->controlRows() - 1);
                float center = std::sin(u * (float)M_PI) * std::sin(v * (float)M_PI);
                float twist = (v - 0.5f) * std::sin(u * (float)M_PI) * 0.035f;
                bezier->setControlPoint(col, row, Vec2(0.69f + u * 0.27f + twist,
                                                       0.07f + v * 0.31f - center * 0.07f));
            }
        }
        engine.document().addSurface(bezier);

        auto tri = engine.document().createTriangleSurface("Triangle");
        tri->setSource(defaultImageSource);
        auto& t = tri->destinationPoints();
        t = {{ Vec2(0.07f, 0.57f), Vec2(0.31f, 0.49f), Vec2(0.25f, 0.86f) }};
        engine.document().addSurface(tri);

        auto circle = engine.document().createCircleSurface("Circle");
        circle->setSource(defaultImageSource);
        circle->setCenter(Vec2(0.50f, 0.68f));
        circle->setRadiusX(0.15f);
        circle->setRadiusY(0.19f);
        circle->setRotation(12.0f);
        engine.document().addSurface(circle);

        auto polygon = engine.document().createPolygonSurface({
            Vec2(0.73f, 0.51f), Vec2(0.94f, 0.58f), Vec2(0.89f, 0.86f),
            Vec2(0.74f, 0.88f), Vec2(0.67f, 0.70f)
        }, "Polygon");
        polygon->setSource(defaultImageSource);
        engine.document().addSurface(polygon);

        engine.editor().selectSurface(bezier->id());
    }

    void update() override {
        float dt = static_cast<float>(getDeltaTime());
        engine.update(dt);
    }

    void draw() override {
        clear(0.06f, 0.06f, 0.08f);

        mapwrap_demo::DrawOptions drawOptions;
        drawOptions.showOutputBounds = true;
        mapwrap_demo::drawDemo(engine, getWidth(), getHeight(), drawOptions);

        // 11. Geometry warning overlay
        drawGeometryWarnings();

        drawCompactStatus();

        // 9 & 12. Debug overlay
        if (showDebugInspector) {
            drawDebugOverlay();
        }
        drawHelpBar();
    }

    void drawGeometryWarnings() {
        int y = getHeight() - 60;
        setColor(1.0f, 0.3f, 0.2f, 0.9f);
        for (auto& surf : engine.document().surfaces()) {
            auto validation = surf->validateGeometry();
            if (!validation.valid) {
                string warning = surf->name() + ": " + validation.message;
                mapwrap_demo::drawBitmapText("WARNING: " + warning, 12, y);
                y -= 18;
            }
        }
    }

    void drawDebugOverlay() {
        int x = getWidth() - 320;
        int y = 20;

        // Semi-transparent background
        setColor(0.0f, 0.0f, 0.0f, 0.6f);
        drawRect(x - 8, y - 8, 320, 220);

        setColor(0.4f, 1.0f, 0.4f, 0.9f);

        // FPS
        mapwrap_demo::drawBitmapText("FPS: " + to_string((int)getFrameRate()), x, y); y += 16;

        // Selected surface
        auto sel = engine.editor().selectedSurface();
        string selName = sel.empty() ? "(none)" : sel;
        if (!sel.empty()) {
            auto surf = engine.document().getSurface(sel);
            if (surf) selName = mapwrap_demo::surfaceLabel(*surf);
        }
        mapwrap_demo::drawBitmapText("Selected: " + selName, x, y); y += 16;

        mapwrap_demo::drawBitmapText("Mode: " + string(mapwrap_demo::modeName(engine.editor().mode())), x, y); y += 16;
        mapwrap_demo::drawBitmapText("Handle: " + handleKindName(engine.editor().selectedHandleKind()) +
                                     " " + to_string(engine.editor().selectedHandleIndex()),
                                     x, y); y += 16;

        // Source of selected
        if (!sel.empty()) {
            auto surf = engine.document().getSurface(sel);
            if (surf) {
                mapwrap_demo::drawBitmapText("Source: " + surf->source(), x, y); y += 16;
            }
        } else {
            mapwrap_demo::drawBitmapText("Source: -", x, y); y += 16;
        }

        // Stats
        auto stats = engine.renderer().stats();
        mapwrap_demo::drawBitmapText("Dirty: " + to_string(stats.rebuiltMeshCount), x, y); y += 16;
        mapwrap_demo::drawBitmapText("Missing: " + to_string(stats.missingSourceCount), x, y); y += 16;
        mapwrap_demo::drawBitmapText("Invalid: " + to_string(stats.invalidSurfaceCount), x, y); y += 16;

        // Surface count
        mapwrap_demo::drawBitmapText("Surfaces: " + to_string(engine.document().surfaces().size()), x, y); y += 16;

        // Snap
        mapwrap_demo::drawBitmapText("Snap: " + string(snapEnabled ? "ON" : "OFF"), x, y); y += 16;

        // Undo/Redo
        string undoDesc = engine.undoStack().canUndo() ? engine.undoStack().undoDescription() : "-";
        string redoDesc = engine.undoStack().canRedo() ? engine.undoStack().redoDescription() : "-";
        mapwrap_demo::drawBitmapText("Undo: " + undoDesc, x, y, "Undo: edit"); y += 16;
        mapwrap_demo::drawBitmapText("Redo: " + redoDesc, x, y, "Redo: edit"); y += 16;

        // Language
        mapwrap_demo::drawBitmapText("Lang: " + MapWrapI18n::instance().language(), x, y);

    }

    void drawCompactStatus() {
        auto sel = engine.editor().selectedSurface();
        string selName = sel.empty() ? "(none)" : sel;
        if (!sel.empty()) {
            auto surf = engine.document().getSurface(sel);
            if (surf) selName = mapwrap_demo::surfaceLabel(*surf);
        }

        setColor(0.0f, 0.0f, 0.0f, 0.36f);
        drawRect(8, 8, 650, 26);
        setColor(0.72f, 1.0f, 0.72f, 0.9f);
        mapwrap_demo::drawBitmapText("Mode: " + string(mapwrap_demo::modeName(engine.editor().mode())) +
                                     "  Selected: " + selName +
                                     "  Handle: " + handleKindName(engine.editor().selectedHandleKind()) +
                                     " " + to_string(engine.editor().selectedHandleIndex()),
                                     16, 26,
                                     "Mode: Surface Edit  Selected: Surface");
    }

    void drawHelpBar() {
        setColor(0.5f, 0.5f, 0.5f, 0.74f);
        mapwrap_demo::drawBitmapText("[1-6]Mode  [Q]uad [G]rid [B]ezier [T]ri [C]ircle [P]olygon  [Del]ete [D]up  [L]ock [V]is", 12, getHeight() - 34);
        mapwrap_demo::drawBitmapText("[Tab/Arrows]Point  [Shift+Arrows]Nudge  [X]Type [O]Perspective  [, .]Cols [; /]Rows [- =]Mesh", 12, getHeight() - 16);
    }

    // --- Mouse forwarding ---

    void mousePressed(const MouseEventArgs& e) override {
        PointerEvent pe = PointerEvent::mouse(Vec2(e.pos.x, e.pos.y), e.button);
        pe.type = PointerEvent::Type::Down;
        pe.shift = e.shift;
        pe.alt = e.alt;
        pe.ctrl = e.ctrl;
        engine.editor().pointerDown(pe);
        redraw();
    }

    void mouseDragged(const MouseDragEventArgs& e) override {
        PointerEvent pe = PointerEvent::mouse(Vec2(e.pos.x, e.pos.y), e.button);
        pe.type = PointerEvent::Type::Move;
        pe.shift = e.shift;
        pe.alt = e.alt;
        pe.ctrl = e.ctrl;
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

    void keyPressed(const KeyEventArgs& e) override {
        int key = e.key;
        bool shift = e.shift;

        if (key == 258) { // TAB
            engine.editor().cycleSelectedHandle(1);
            redraw();
            return;
        }

        if (key == 262 || key == 263 || key == 264 || key == 265) {
            Vec2 delta(0, 0);
            int dx = 0, dy = 0;
            if (key == 262) { delta.x = 1; dx = 1; }
            if (key == 263) { delta.x = -1; dx = -1; }
            if (key == 264) { delta.y = 1; dy = 1; }
            if (key == 265) { delta.y = -1; dy = -1; }
            if (shift) engine.editor().nudgeSelectedHandle(delta);
            else engine.editor().selectAdjacentHandle(dx, dy);
            redraw();
            return;
        }

        switch (key) {
            // 7. Mode switching
            case '1': engine.editor().setMode(EditMode::Presentation); break;
            case '2': engine.editor().setMode(EditMode::SurfaceEdit); break;
            case '3': engine.editor().setMode(EditMode::TextureEdit); break;
            case '4': engine.editor().setMode(EditMode::SourceAssign); break;
            case '5': engine.editor().setMode(EditMode::MaskEdit); break;
            case '6': engine.editor().setMode(EditMode::OutputEdit); break;

            // 1. Add surfaces
            case 'Q': {
                auto s = engine.document().createQuadSurface();
                s->setSource(defaultImageSource);
                engine.document().addSurface(s);
                engine.editor().selectSurface(s->id());
                logNotice("Added quad: " + s->id());
                break;
            }
            case 'G': {
                auto s = engine.document().createGridSurface(4, 4);
                s->setSource(defaultImageSource);
                engine.document().addSurface(s);
                engine.editor().selectSurface(s->id());
                logNotice("Added grid: " + s->id());
                break;
            }
            case 'B': {
                auto s = engine.document().createBezierSurface(4, 4);
                s->setSource(defaultImageSource);
                s->setMeshResolution(28);
                engine.document().addSurface(s);
                engine.editor().selectSurface(s->id());
                logNotice("Added bezier: " + s->id());
                break;
            }
            case 'T': {
                auto s = engine.document().createTriangleSurface();
                s->setSource(defaultImageSource);
                engine.document().addSurface(s);
                engine.editor().selectSurface(s->id());
                logNotice("Added triangle: " + s->id());
                break;
            }
            case 'C': {
                auto s = engine.document().createCircleSurface();
                s->setSource(defaultImageSource);
                engine.document().addSurface(s);
                engine.editor().selectSurface(s->id());
                logNotice("Added circle: " + s->id());
                break;
            }
            case 'P': {
                auto s = engine.document().createPolygonSurface({
                    Vec2(0.2f, 0.2f), Vec2(0.8f, 0.2f),
                    Vec2(0.7f, 0.8f), Vec2(0.3f, 0.8f)
                });
                s->setSource(defaultImageSource);
                engine.document().addSurface(s);
                engine.editor().selectSurface(s->id());
                logNotice("Added polygon: " + s->id());
                break;
            }

            // 2. Delete & Duplicate
            case 261: // Delete (sokol key)
                engine.editor().deleteSelected();
                logNotice("Deleted selected");
                break;
            case 'D':
                engine.editor().duplicateSelected();
                logNotice("Duplicated selected");
                break;

            // 14/15. Current point and warp controls
            case 'X':
                cycleWarpKind();
                break;
            case 'O':
                toggleQuadPerspective();
                break;
            case ',':
                engine.editor().removeColumnFromSelected();
                break;
            case '.':
                engine.editor().addColumnToSelected();
                break;
            case ';':
                engine.editor().removeRowFromSelected();
                break;
            case '/':
                engine.editor().addRowToSelected();
                break;
            case '-':
                engine.editor().adjustSelectedMeshResolution(-1);
                break;
            case '=':
                engine.editor().adjustSelectedMeshResolution(1);
                break;

            // 3. Layer ordering
            case '[':
                engine.editor().sendBackward();
                break;
            case ']':
                engine.editor().bringForward();
                break;

            // 4. Lock / Visibility
            case 'L': {
                auto sel = engine.editor().selectedSurface();
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s) {
                        s->setLocked(!s->isLocked());
                        logNotice(string("Locked: ") + (s->isLocked() ? "YES" : "NO"));
                    }
                }
                break;
            }
            case 'V': {
                auto sel = engine.editor().selectedSurface();
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s) {
                        s->setVisible(!s->isVisible());
                        logNotice(string("Visible: ") + (s->isVisible() ? "YES" : "NO"));
                    }
                }
                break;
            }

            // 5. Undo / Redo
            case 'Z': {
                if (shift) {
                    engine.undoStack().redo();
                    logNotice("Redo");
                } else {
                    engine.undoStack().undo();
                    logNotice("Undo");
                }
                break;
            }

            // 6. Mask editing
            case 'M': {
                auto sel = engine.editor().selectedSurface();
                if (!sel.empty()) {
                    auto s = engine.document().getSurface(sel);
                    if (s) {
                        MapWrapMask mask;
                        mask.kind = MaskKind::Polygon;
                        mask.operation = MaskOperation::Add;
                        mask.enabled = true;
                        mask.points = {
                            Vec2(0.5f, 0.1f), Vec2(0.9f, 0.5f),
                            Vec2(0.5f, 0.9f), Vec2(0.1f, 0.5f)
                        };
                        s->masks().push_back(mask);
                        logNotice("Mask added to " + sel);
                    }
                }
                break;
            }

            // 8. Snap toggle
            case 'S': {
                snapEnabled = !snapEnabled;
                auto snap = engine.editor().snapSettings();
                snap.enabled = snapEnabled;
                engine.editor().setSnapSettings(snap);
                logNotice(string("Snap: ") + (snapEnabled ? "ON" : "OFF"));
                break;
            }

            case 'H': {
                showDebugInspector = !showDebugInspector;
                break;
            }

            // 10. Copy / Paste geometry
            case 'C' & 0x1F: { // Ctrl+C
                engine.editor().copySelectedGeometry();
                logNotice("Geometry copied");
                break;
            }
            case 'V' & 0x1F: { // Ctrl+V
                engine.editor().pasteGeometryToSelected();
                logNotice("Geometry pasted");
                break;
            }

            // 10b. Copy / Paste UV
            case 'U': {
                if (shift) {
                    engine.editor().pasteUVToSelected();
                    logNotice("UV pasted");
                } else {
                    engine.editor().copySelectedUV();
                    logNotice("UV copied");
                }
                break;
            }

            // 13. Language toggle (F9)
            case 293: { // F9
                auto& i18n = MapWrapI18n::instance();
                if (i18n.isChinese()) {
                    i18n.setLanguage("en");
                    logNotice("Language: English");
                } else {
                    i18n.setLanguage("zh");
                    logNotice("Language: 中文");
                }
                break;
            }

            // Escape: deselect
            case 256:
                engine.editor().deselect();
                break;
        }
        redraw();
    }
};

int main() {
    WindowSettings settings;
    settings.width = 1440;
    settings.height = 900;
    settings.title = "mapwrap_editor";
    return TC_RUN_APP(MapWrapEditorApp, settings);
}
