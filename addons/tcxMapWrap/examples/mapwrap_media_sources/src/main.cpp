// =============================================================================
// mapwrap_media_sources — Source system demo
// =============================================================================
//
// Demonstrates all source types and source management:
//   1. Image source — static image loaded from file
//   2. Video source — video file playback
//   3. FBO source — dynamic scene rendered to an FBO
//   4. Generated source — callback-driven dynamic content
//   5. Built-in calibration patterns (cycle with TAB)
//   6. Source clock display
//   7. Missing media validation display
//   8. Relink source demo
//   9. Multiple sources on different surfaces at once
//
// Keyboard shortcuts:
//   TAB     → Cycle calibration pattern on Pattern surface
//   R       → Relink source (re-assign image source)
//   V       → Validate project (show missing sources)
//   Space   → Play/Pause video
//   1/2     → Presentation / Surface Edit
//   Escape  → Deselect
// =============================================================================

#include <TrussC.h>
#include <tcxMapWrap.h>
#include "../../shared/MapWrapDemoDraw.h"

#include <cmath>

using namespace std;
using namespace tc;
using namespace tcx::mapwrap;
using MapRect = tcx::mapwrap::Rect;

class MapWrapSourcesApp : public tc::App {
public:
    MapWrapEngine engine;
    SourceId imageSourceId;
    SourceId videoSourceId;
    SourceId videoAlphaSourceId;
    SourceId videoSecondSourceId;
    SourceId videoThirdSourceId;
    SourceId fboSourceId;
    SourceId generatedSourceId;
    SourceId calibrationSourceId;
    SourceId alphaMaskSourceId;
    tc::Fbo dynamicFbo;
    float fboTime = 0.0f;

    int currentPatternIndex = 0;
    static constexpr int kPatternCount = 13;
    BuiltinPatternKind patterns[kPatternCount] = {
        BuiltinPatternKind::Checkerboard,
        BuiltinPatternKind::Grid,
        BuiltinPatternKind::FineGrid,
        BuiltinPatternKind::Crosshair,
        BuiltinPatternKind::CornerLabels,
        BuiltinPatternKind::UVGradient,
        BuiltinPatternKind::ColorBars,
        BuiltinPatternKind::LumaRamp,
        BuiltinPatternKind::EdgeBlendRamp,
        BuiltinPatternKind::AlphaRadial,
        BuiltinPatternKind::NumberedCells,
        BuiltinPatternKind::SafeArea,
        BuiltinPatternKind::SolidColor
    };

    SurfaceId imageSurfaceId;
    SurfaceId videoSurfaceId;
    SurfaceId fboSurfaceId;
    SurfaceId generatedSurfaceId;
    SurfaceId calibrationSurfaceId;

    void setup() override {
        MapWrapI18n::instance().detectAndSetLanguage();

        engine.setCanvasSize(Vec2(getWidth(), getHeight()));

        // --- 1. Image source ---
        imageSourceId = engine.sources().addImage("Photo", "data/test_image.png");

        // --- 2. Video sources ---
        // Drop files into these paths to exercise 3-4 real independent decoders.
        // Alpha-capable containers/codecs can be assigned to the Alpha Video slot.
        videoSourceId = engine.sources().addVideo("Opaque Video A", "data/media/opaque_a.mp4");
        videoAlphaSourceId = engine.sources().addVideo("Alpha Video", "data/media/alpha_overlay.mov");
        videoSecondSourceId = engine.sources().addVideo("Opaque Video B", "data/media/opaque_b.mp4");
        videoThirdSourceId = engine.sources().addVideo("Video C", "data/media/video_c.mp4");

        // --- 3. FBO source ---
        dynamicFbo.allocate(512, 512);
        fboSourceId = engine.sources().addFbo("Dynamic FBO", &dynamicFbo, Vec2(512, 512));

        // --- 4. Generated source callback ---
        generatedSourceId = engine.sources().addGenerated(
            "Generated",
            [](double timeSeconds, Vec2 size) {
                // This callback is invoked each frame.
                // In a full integration, you would draw into the current
                // FBO or update a texture here.
                // For demo purposes, we just log the call.
            },
            Vec2(512, 512)
        );

        // --- 5. Built-in calibration pattern source ---
        calibrationSourceId = engine.sources().addBuiltinPattern(
            "Calibration", BuiltinPatternKind::Grid, Vec2(1920, 1080));
        alphaMaskSourceId = engine.sources().addBuiltinPattern(
            "Radial Alpha Mask", BuiltinPatternKind::AlphaRadial, Vec2(512, 512));

        setAllVideosPlaying(true);

        // --- 9. Create surfaces, each with a different source ---
        auto setQuadRect = [](const shared_ptr<SurfaceQuad>& surf, MapRect r) {
            auto& d = surf->destinationPoints();
            d = {{ Vec2(r.x, r.y), Vec2(r.x + r.w, r.y),
                   Vec2(r.x + r.w, r.y + r.h), Vec2(r.x, r.y + r.h) }};
        };

        // Image surface
        auto imgSurf = engine.document().createQuadSurface("Image Surface");
        imgSurf->setSource(imageSourceId);
        setQuadRect(imgSurf, MapRect(0.02f, 0.18f, 0.225f, 0.26f));
        engine.document().addSurface(imgSurf);
        imageSurfaceId = imgSurf->id();

        // Video surfaces
        auto vidSurf = engine.document().createQuadSurface("Video Surface");
        vidSurf->setSource(videoSourceId);
        setQuadRect(vidSurf, MapRect(0.265f, 0.18f, 0.225f, 0.26f));
        engine.document().addSurface(vidSurf);
        videoSurfaceId = vidSurf->id();

        auto alphaVidSurf = engine.document().createQuadSurface("Alpha Video + Mask");
        alphaVidSurf->setSource(videoAlphaSourceId);
        setQuadRect(alphaVidSurf, MapRect(0.51f, 0.18f, 0.225f, 0.26f));
        addRadialAlphaMask(*alphaVidSurf);
        engine.document().addSurface(alphaVidSurf);

        auto vid2Surf = engine.document().createQuadSurface("Video B");
        vid2Surf->setSource(videoSecondSourceId);
        setQuadRect(vid2Surf, MapRect(0.755f, 0.18f, 0.225f, 0.26f));
        engine.document().addSurface(vid2Surf);

        // FBO surface
        auto fboSurf = engine.document().createQuadSurface("FBO Surface");
        fboSurf->setSource(fboSourceId);
        setQuadRect(fboSurf, MapRect(0.02f, 0.58f, 0.225f, 0.34f));
        engine.document().addSurface(fboSurf);
        fboSurfaceId = fboSurf->id();

        // Generated surface
        auto genSurf = engine.document().createQuadSurface("Generated Surface");
        genSurf->setSource(generatedSourceId);
        setQuadRect(genSurf, MapRect(0.265f, 0.58f, 0.225f, 0.34f));
        engine.document().addSurface(genSurf);
        generatedSurfaceId = genSurf->id();

        // Calibration surface
        auto calSurf = engine.document().createQuadSurface("Calibration Surface");
        calSurf->setSource(calibrationSourceId);
        setQuadRect(calSurf, MapRect(0.51f, 0.58f, 0.225f, 0.34f));
        engine.document().addSurface(calSurf);
        calibrationSurfaceId = calSurf->id();

        auto vid3Surf = engine.document().createQuadSurface("Video C");
        vid3Surf->setSource(videoThirdSourceId);
        setQuadRect(vid3Surf, MapRect(0.755f, 0.58f, 0.225f, 0.34f));
        engine.document().addSurface(vid3Surf);
    }

    void update() override {
        float dt = static_cast<float>(getDeltaTime());
        renderDynamicFbo(dt);
        engine.update(dt);
    }

    void draw() override {
        clear(0.05f, 0.05f, 0.07f);

        mapwrap_demo::drawDemo(engine, getWidth(), getHeight());

        drawSourceOverlay();
    }

    void drawSourceOverlay() {
        int y = 20;

        // 6. Source clock display
        auto& clock = engine.sources().globalClock();
        double clockTime = clock.timeSeconds();
        setColor(0.8f, 0.9f, 1.0f, 0.9f);
        mapwrap_demo::drawBitmapText("Clock: " + formatTime(clockTime) + "  " + string(clock.isPlaying() ? "PLAYING" : "PAUSED"), 12, y);
        y += 18;

        // Source info
        mapwrap_demo::drawBitmapText("Sources: " + to_string(engine.sources().count()), 12, y); y += 18;

        // 7. Missing media validation
        auto report = engine.document().validateProject();
        if (!report.ok) {
            setColor(1.0f, 0.4f, 0.2f, 0.9f);
            for (auto& missing : report.missingSources) {
                mapwrap_demo::drawBitmapText("MISSING: " + missing, 12, y);
                y += 16;
            }
            for (auto& file : report.missingFiles) {
                mapwrap_demo::drawBitmapText("FILE NOT FOUND: " + file, 12, y);
                y += 16;
            }
        }

        // 5. Current calibration pattern
        setColor(0.6f, 0.8f, 0.6f, 0.8f);
        mapwrap_demo::drawBitmapText("Pattern: " + patternName(patterns[currentPatternIndex]), 12, y); y += 18;

        // Stats
        auto stats = engine.renderer().stats();
        mapwrap_demo::drawBitmapText("Draws: tex " + to_string(stats.texturedDrawCount) +
                                     "  placeholder " + to_string(stats.placeholderDrawCount) +
                                     "  active videos " + to_string(stats.activeVideoSourceCount),
                                     12, y);

        // Help
        setColor(0.5f, 0.5f, 0.5f, 0.7f);
        mapwrap_demo::drawBitmapText("[1]Present [2]Surface  [TAB]Pattern  [R]elink  [V]alidate  [Space]Play/Pause  [Esc]Deselect", 12, getHeight() - 16);
    }

    string formatTime(double seconds) {
        int mins = (int)(seconds / 60.0);
        double secs = seconds - mins * 60.0;
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d:%06.3f", mins, secs);
        return string(buf);
    }

    string patternName(BuiltinPatternKind kind) {
        switch (kind) {
            case BuiltinPatternKind::Checkerboard:   return "Checkerboard";
            case BuiltinPatternKind::Grid:           return "Grid";
            case BuiltinPatternKind::FineGrid:       return "Fine Grid";
            case BuiltinPatternKind::Crosshair:      return "Crosshair";
            case BuiltinPatternKind::CornerLabels:   return "Corner Labels";
            case BuiltinPatternKind::UVGradient:     return "UV Gradient";
            case BuiltinPatternKind::ColorBars:      return "Color Bars";
            case BuiltinPatternKind::LumaRamp:       return "Luma Ramp";
            case BuiltinPatternKind::EdgeBlendRamp:  return "Edge Blend Ramp";
            case BuiltinPatternKind::AlphaRadial:    return "Alpha Radial";
            case BuiltinPatternKind::NumberedCells:  return "Numbered Cells";
            case BuiltinPatternKind::SafeArea:       return "Safe Area";
            case BuiltinPatternKind::SolidColor:     return "Solid Color";
            default:                                 return "Unknown";
        }
    }

    void addRadialAlphaMask(Surface& surface) {
        MapWrapMask alphaMask;
        alphaMask.kind = MaskKind::AlphaTexture;
        alphaMask.operation = MaskOperation::Add;
        alphaMask.space = MaskSpace::SurfaceLocal;
        alphaMask.enabled = true;
        alphaMask.name = "Radial Alpha";
        alphaMask.rect = MapRect(0.02f, 0.02f, 0.96f, 0.96f);
        alphaMask.alphaTextureSource = alphaMaskSourceId;
        surface.masks().push_back(alphaMask);
    }

    void setAllVideosPlaying(bool playing) {
        const SourceId ids[] = {
            videoSourceId, videoAlphaSourceId, videoSecondSourceId, videoThirdSourceId
        };
        for (const auto& id : ids) {
            auto src = engine.sources().get(id);
            auto* video = src ? dynamic_cast<SourceVideo*>(src.get()) : nullptr;
            if (!video) continue;
            video->setLoop(true);
            video->setVolume(0.0f);
            if (playing) video->play();
            else video->pause();
        }
    }

    bool anyVideoPlaying() const {
        const SourceId ids[] = {
            videoSourceId, videoAlphaSourceId, videoSecondSourceId, videoThirdSourceId
        };
        for (const auto& id : ids) {
            auto src = engine.sources().get(id);
            auto* video = src ? dynamic_cast<SourceVideo*>(src.get()) : nullptr;
            if (video && video->isPlaying()) return true;
        }
        return false;
    }

    void renderDynamicFbo(float dt) {
        fboTime += dt;
        if (!dynamicFbo.isAllocated()) return;

        dynamicFbo.begin(0.02f, 0.04f, 0.06f, 1.0f);
        setColor(0.08f, 0.18f, 0.24f, 1.0f);
        drawRect(0, 0, 512, 512);
        for (int i = 0; i < 9; ++i) {
            float t = fboTime * (0.35f + i * 0.06f) + i * 0.7f;
            float x = 256.0f + std::cos(t) * (42.0f + i * 18.0f);
            float y = 256.0f + std::sin(t * 1.3f) * (38.0f + i * 14.0f);
            setColor(0.18f + i * 0.07f, 0.85f - i * 0.04f, 0.95f, 0.45f);
            drawCircle(x, y, 24.0f + i * 3.0f);
        }
        setColor(1.0f, 1.0f, 1.0f, 0.88f);
        drawBitmapString("Live FBO", 20, 32);
        dynamicFbo.end();
    }

    // --- Mouse forwarding ---

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
        switch (key) {
            // 5. Cycle calibration pattern
            case 258: { // TAB
                currentPatternIndex = (currentPatternIndex + 1) % kPatternCount;
                auto src = engine.sources().get(calibrationSourceId);
                if (src) {
                    auto* calSrc = dynamic_cast<CalibrationPatternSource*>(src.get());
                    if (calSrc) {
                        calSrc->setPattern(patterns[currentPatternIndex]);
                        logNotice("Pattern: " + patternName(patterns[currentPatternIndex]));
                    }
                }
                break;
            }

            // 8. Relink source demo — reassign image source to calibration surface
            case 'R': {
                auto calSurf = engine.document().getSurface(calibrationSurfaceId);
                if (calSurf) {
                    // Toggle between calibration and image source
                    if (calSurf->source() == calibrationSourceId) {
                        calSurf->setSource(imageSourceId);
                        logNotice("Relinked: Calibration surface → Image source");
                    } else {
                        calSurf->setSource(calibrationSourceId);
                        logNotice("Relinked: Calibration surface → Calibration source");
                    }
                }
                break;
            }

            // 7. Validate project
            case 'V': {
                auto report = engine.document().validateProject();
                if (report.ok) {
                    logNotice("Validation: All OK");
                } else {
                    for (auto& m : report.missingSources) logWarning("Missing source: " + m);
                    for (auto& f : report.missingFiles)   logWarning("Missing file: " + f);
                    for (auto& w : report.warnings)        logWarning("Warning: " + w);
                }
                break;
            }

            // Play/Pause video
            case ' ': {
                bool play = !anyVideoPlaying();
                setAllVideosPlaying(play);
                logNotice(play ? "Videos playing" : "Videos paused");
                break;
            }

            // Mode switching
            case '1': engine.editor().setMode(EditMode::Presentation); break;
            case '2': engine.editor().setMode(EditMode::SurfaceEdit); break;
            case '3':
            case '4':
                logNotice("This example exposes Presentation and Surface Edit only");
                break;

            case 256: engine.editor().deselect(); break;
        }
        redraw();
    }
};

int main() {
    WindowSettings settings;
    settings.width = 1280;
    settings.height = 720;
    settings.title = "mapwrap_media_sources";
    return TC_RUN_APP(MapWrapSourcesApp, settings);
}
