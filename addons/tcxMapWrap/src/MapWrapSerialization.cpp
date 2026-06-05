// =============================================================================
// tcxMapWrap — MapWrapSerialization.cpp Implementation
// =============================================================================

#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/MapWrapI18n.h"
#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/SurfaceTriangle.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/SurfaceGroup.h"
#include "tcxMapWrap/MapWrapCue.h"
#include "tcxMapWrap/MapWrapOutput.h"
#include "tcxMapWrap/MapWrapStage.h"
#include "tcxMapWrap/EditorViewport.h"
#include "tcxMapWrap/BlendMode.h"
#include "tcxMapWrap/ColorCorrection.h"
#include "tcxMapWrap/SourceRegistry.h"

#include "nlohmann/json.hpp"

#include <fstream>
#include <sstream>
#include <cstring>
#include <memory>

namespace tcx {
namespace mapwrap {

using Json = nlohmann::json;

// =============================================================================
// Helper: enum ↔ string
// =============================================================================

static const char* surfaceKindToString(SurfaceKind kind) {
    switch (kind) {
        case SurfaceKind::Quad:     return "quad";
        case SurfaceKind::Grid:     return "grid";
        case SurfaceKind::Bezier:   return "bezier";
        case SurfaceKind::Triangle: return "triangle";
        case SurfaceKind::Circle:   return "circle";
        case SurfaceKind::Polygon:  return "polygon";
    }
    return "quad";
}

static bool stringToSurfaceKind(const std::string& s, SurfaceKind& kind) {
    if (s == "quad")          { kind = SurfaceKind::Quad;     return true; }
    if (s == "grid")          { kind = SurfaceKind::Grid;     return true; }
    if (s == "bezier")        { kind = SurfaceKind::Bezier;   return true; }
    if (s == "triangle")      { kind = SurfaceKind::Triangle; return true; }
    if (s == "circle")        { kind = SurfaceKind::Circle;   return true; }
    if (s == "polygon")       { kind = SurfaceKind::Polygon;  return true; }
    return false;
}

static const char* maskKindToString(MaskKind kind) {
    switch (kind) {
        case MaskKind::Rectangle:    return "rectangle";
        case MaskKind::Ellipse:      return "ellipse";
        case MaskKind::Polygon:      return "polygon";
        case MaskKind::Bezier:       return "bezier";
        case MaskKind::Freehand:     return "freehand";
        case MaskKind::AlphaTexture: return "alpha_texture";
    }
    return "polygon";
}

static bool stringToMaskKind(const std::string& s, MaskKind& kind) {
    if (s == "rectangle")    { kind = MaskKind::Rectangle;    return true; }
    if (s == "ellipse")      { kind = MaskKind::Ellipse;      return true; }
    if (s == "polygon")      { kind = MaskKind::Polygon;      return true; }
    if (s == "bezier")       { kind = MaskKind::Bezier;       return true; }
    if (s == "freehand")     { kind = MaskKind::Freehand;     return true; }
    if (s == "alpha_texture"){ kind = MaskKind::AlphaTexture; return true; }
    return false;
}

static const char* maskOperationToString(MaskOperation op) {
    switch (op) {
        case MaskOperation::Add:      return "add";
        case MaskOperation::Subtract: return "subtract";
        case MaskOperation::Intersect:return "intersect";
    }
    return "add";
}

static bool stringToMaskOperation(const std::string& s, MaskOperation& op) {
    if (s == "add")       { op = MaskOperation::Add;       return true; }
    if (s == "subtract")  { op = MaskOperation::Subtract;  return true; }
    if (s == "intersect") { op = MaskOperation::Intersect; return true; }
    return false;
}

static const char* maskSpaceToString(MaskSpace sp) {
    switch (sp) {
        case MaskSpace::SurfaceLocal: return "surface_local";
        case MaskSpace::SourceUV:     return "source_uv";
        case MaskSpace::Canvas:       return "canvas";
        case MaskSpace::Output:       return "output";
    }
    return "surface_local";
}

static bool stringToMaskSpace(const std::string& s, MaskSpace& sp) {
    if (s == "surface_local") { sp = MaskSpace::SurfaceLocal; return true; }
    if (s == "source_uv")     { sp = MaskSpace::SourceUV;     return true; }
    if (s == "canvas")        { sp = MaskSpace::Canvas;       return true; }
    if (s == "output")        { sp = MaskSpace::Output;       return true; }
    return false;
}

static const char* editModeToString(EditMode mode) {
    switch (mode) {
        case EditMode::Presentation:  return "presentation";
        case EditMode::SurfaceEdit:   return "surface_edit";
        case EditMode::TextureEdit:   return "texture_edit";
        case EditMode::SourceAssign:  return "source_assign";
        case EditMode::MaskEdit:      return "mask_edit";
        case EditMode::OutputEdit:    return "output_edit";
    }
    return "presentation";
}

static bool stringToEditMode(const std::string& s, EditMode& mode) {
    if (s == "presentation")  { mode = EditMode::Presentation;  return true; }
    if (s == "surface_edit")  { mode = EditMode::SurfaceEdit;   return true; }
    if (s == "texture_edit")  { mode = EditMode::TextureEdit;   return true; }
    if (s == "source_assign") { mode = EditMode::SourceAssign;  return true; }
    if (s == "mask_edit")     { mode = EditMode::MaskEdit;      return true; }
    if (s == "output_edit")   { mode = EditMode::OutputEdit;    return true; }
    return false;
}

static const char* sourceKindToString(SourceKind kind) {
    switch (kind) {
        case SourceKind::None:           return "none";
        case SourceKind::Texture:        return "texture";
        case SourceKind::Fbo:            return "fbo";
        case SourceKind::Video:          return "video";
        case SourceKind::Image:          return "image";
        case SourceKind::Generated:      return "generated";
        case SourceKind::BuiltinPattern: return "builtin_pattern";
    }
    return "none";
}

static bool stringToSourceKind(const std::string& s, SourceKind& kind) {
    if (s == "none")            { kind = SourceKind::None;           return true; }
    if (s == "texture")         { kind = SourceKind::Texture;        return true; }
    if (s == "fbo")             { kind = SourceKind::Fbo;            return true; }
    if (s == "video")           { kind = SourceKind::Video;          return true; }
    if (s == "image")           { kind = SourceKind::Image;          return true; }
    if (s == "generated")       { kind = SourceKind::Generated;      return true; }
    if (s == "builtin_pattern") { kind = SourceKind::BuiltinPattern; return true; }
    return false;
}

static const char* builtinPatternToString(BuiltinPatternKind kind) {
    switch (kind) {
        case BuiltinPatternKind::Checkerboard:   return "checkerboard";
        case BuiltinPatternKind::Grid:           return "grid";
        case BuiltinPatternKind::FineGrid:       return "fine_grid";
        case BuiltinPatternKind::Crosshair:      return "crosshair";
        case BuiltinPatternKind::CornerLabels:   return "corner_labels";
        case BuiltinPatternKind::UVGradient:     return "uv_gradient";
        case BuiltinPatternKind::ColorBars:      return "color_bars";
        case BuiltinPatternKind::LumaRamp:       return "luma_ramp";
        case BuiltinPatternKind::EdgeBlendRamp:  return "edge_blend_ramp";
        case BuiltinPatternKind::AlphaRadial:    return "alpha_radial";
        case BuiltinPatternKind::NumberedCells:  return "numbered_cells";
        case BuiltinPatternKind::SafeArea:       return "safe_area";
        case BuiltinPatternKind::SolidColor:     return "solid_color";
    }
    return "grid";
}

static bool stringToBuiltinPattern(const std::string& s, BuiltinPatternKind& kind) {
    if (s == "checkerboard")    { kind = BuiltinPatternKind::Checkerboard;  return true; }
    if (s == "grid")            { kind = BuiltinPatternKind::Grid;          return true; }
    if (s == "fine_grid")       { kind = BuiltinPatternKind::FineGrid;      return true; }
    if (s == "crosshair")       { kind = BuiltinPatternKind::Crosshair;     return true; }
    if (s == "corner_labels")   { kind = BuiltinPatternKind::CornerLabels;  return true; }
    if (s == "uv_gradient")     { kind = BuiltinPatternKind::UVGradient;    return true; }
    if (s == "color_bars")      { kind = BuiltinPatternKind::ColorBars;     return true; }
    if (s == "luma_ramp")       { kind = BuiltinPatternKind::LumaRamp;      return true; }
    if (s == "edge_blend_ramp") { kind = BuiltinPatternKind::EdgeBlendRamp; return true; }
    if (s == "alpha_radial")    { kind = BuiltinPatternKind::AlphaRadial;   return true; }
    if (s == "numbered_cells")  { kind = BuiltinPatternKind::NumberedCells; return true; }
    if (s == "safe_area")       { kind = BuiltinPatternKind::SafeArea;      return true; }
    if (s == "solid_color")     { kind = BuiltinPatternKind::SolidColor;    return true; }
    return false;
}

static const char* playbackModeToString(SourcePlaybackMode mode) {
    switch (mode) {
        case SourcePlaybackMode::Manual:            return "manual";
        case SourcePlaybackMode::FollowGlobalClock: return "follow_global_clock";
        case SourcePlaybackMode::FreeRun:           return "free_run";
    }
    return "free_run";
}

static bool stringToPlaybackMode(const std::string& s, SourcePlaybackMode& mode) {
    if (s == "manual")              { mode = SourcePlaybackMode::Manual;            return true; }
    if (s == "follow_global_clock") { mode = SourcePlaybackMode::FollowGlobalClock; return true; }
    if (s == "free_run")            { mode = SourcePlaybackMode::FreeRun;           return true; }
    return false;
}

// =============================================================================
// Helper: Vec2 / Vec3 / Vec4 / Rect ↔ JSON
// =============================================================================

static Json vec2ToJson(Vec2 v) { return Json::array({v.x, v.y}); }
static Vec2 jsonToVec2(const Json& j, Vec2 fallback = Vec2(0,0)) {
    if (j.is_array() && j.size() >= 2) return Vec2(j[0].get<float>(), j[1].get<float>());
    return fallback;
}

static Json vec3ToJson(Vec3 v) { return Json::array({v.x, v.y, v.z}); }
static Vec3 jsonToVec3(const Json& j, Vec3 fallback = Vec3(0,0,0)) {
    if (j.is_array() && j.size() >= 3) return Vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    return fallback;
}

static Json vec4ToJson(Vec4 v) { return Json::array({v.x, v.y, v.z, v.w}); }
static Vec4 jsonToVec4(const Json& j, Vec4 fallback = Vec4(0,0,0,0)) {
    if (j.is_array() && j.size() >= 4) return Vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    return fallback;
}

static Json rectToJson(Rect r) { return Json::array({r.x, r.y, r.w, r.h}); }
static Rect jsonToRect(const Json& j, Rect fallback = Rect(0,0,0,0)) {
    if (j.is_array() && j.size() >= 4) return Rect(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    return fallback;
}

// =============================================================================
// Helper: BlendSettings / ColorCorrection ↔ JSON
// =============================================================================

static Json blendSettingsToJson(const BlendSettings& b) {
    return Json{
        {"opacity",    b.opacity},
        {"brightness", b.brightness},
        {"edge",       vec4ToJson(b.edge)},
        {"gamma",      vec3ToJson(b.gamma)},
        {"luminance",  vec3ToJson(b.luminance)},
        {"exponent",   b.exponent},
        {"enabled",    b.enabled}
    };
}

static BlendSettings jsonToBlendSettings(const Json& j) {
    BlendSettings b;
    if (!j.is_object()) return b;
    if (j.contains("opacity"))    b.opacity    = j["opacity"].get<float>();
    if (j.contains("brightness")) b.brightness = j["brightness"].get<float>();
    if (j.contains("edge"))       b.edge       = jsonToVec4(j["edge"], b.edge);
    if (j.contains("gamma"))      b.gamma      = jsonToVec3(j["gamma"], b.gamma);
    if (j.contains("luminance"))  b.luminance  = jsonToVec3(j["luminance"], b.luminance);
    if (j.contains("exponent"))   b.exponent   = j["exponent"].get<float>();
    if (j.contains("enabled"))    b.enabled    = j["enabled"].get<bool>();
    return b;
}

static Json colorCorrectionToJson(const ColorCorrection& c) {
    return Json{
        {"enabled",             c.enabled},
        {"opacity",             c.opacity},
        {"brightness",          c.brightness},
        {"contrast",            c.contrast},
        {"saturation",          c.saturation},
        {"gamma",               vec3ToJson(c.gamma)},
        {"lift",                vec3ToJson(c.lift)},
        {"gain",                vec3ToJson(c.gain)},
        {"blackLevel",          c.blackLevel},
        {"whiteLevel",          c.whiteLevel},
        {"premultipliedAlpha",  c.premultipliedAlpha}
    };
}

static ColorCorrection jsonToColorCorrection(const Json& j) {
    ColorCorrection c;
    if (!j.is_object()) return c;
    if (j.contains("enabled"))            c.enabled            = j["enabled"].get<bool>();
    if (j.contains("opacity"))            c.opacity            = j["opacity"].get<float>();
    if (j.contains("brightness"))         c.brightness         = j["brightness"].get<float>();
    if (j.contains("contrast"))           c.contrast           = j["contrast"].get<float>();
    if (j.contains("saturation"))         c.saturation         = j["saturation"].get<float>();
    if (j.contains("gamma"))              c.gamma              = jsonToVec3(j["gamma"], c.gamma);
    if (j.contains("lift"))               c.lift               = jsonToVec3(j["lift"], c.lift);
    if (j.contains("gain"))               c.gain               = jsonToVec3(j["gain"], c.gain);
    if (j.contains("blackLevel"))         c.blackLevel         = j["blackLevel"].get<float>();
    if (j.contains("whiteLevel"))         c.whiteLevel         = j["whiteLevel"].get<float>();
    if (j.contains("premultipliedAlpha")) c.premultipliedAlpha = j["premultipliedAlpha"].get<bool>();
    return c;
}

// =============================================================================
// Helper: Source ↔ JSON
// =============================================================================

static Json sourceToJson(const std::shared_ptr<Source>& source) {
    if (!source) return Json();

    Json j = Json{
        {"id", source->id()},
        {"name", source->name()},
        {"kind", sourceKindToString(source->kind())},
        {"size", vec2ToJson(source->size())},
        {"colorCorrection", colorCorrectionToJson(source->colorCorrection())}
    };

    switch (source->kind()) {
        case SourceKind::Image: {
            auto image = std::dynamic_pointer_cast<SourceImage>(source);
            if (image) j["path"] = image->path();
            break;
        }
        case SourceKind::Video: {
            auto video = std::dynamic_pointer_cast<SourceVideo>(source);
            if (video) {
                j["path"] = video->path();
                j["loop"] = video->loop();
                j["volume"] = video->volume();
                j["playbackMode"] = playbackModeToString(video->playbackMode());
            }
            break;
        }
        case SourceKind::BuiltinPattern: {
            auto pattern = std::dynamic_pointer_cast<CalibrationPatternSource>(source);
            if (pattern) j["pattern"] = builtinPatternToString(pattern->pattern());
            break;
        }
        case SourceKind::None:
        case SourceKind::Texture:
        case SourceKind::Fbo:
        case SourceKind::Generated:
            break;
    }

    return j;
}

static Json sourcesToJson(const SourceRegistry& sources) {
    Json result = Json::array();
    for (const auto& source : sources.all()) {
        if (source) result.push_back(sourceToJson(source));
    }
    return result;
}

static std::shared_ptr<Source> jsonToSource(const Json& j, std::vector<std::string>& warnings) {
    if (!j.is_object()) {
        warnings.push_back("Invalid source entry");
        return nullptr;
    }

    SourceKind kind = SourceKind::None;
    if (j.contains("kind") && j["kind"].is_string()) {
        std::string kindStr = j["kind"].get<std::string>();
        if (!stringToSourceKind(kindStr, kind)) {
            warnings.push_back("Unknown source kind: " + kindStr);
            return nullptr;
        }
    } else {
        warnings.push_back("Source missing kind");
        return nullptr;
    }

    Vec2 size = jsonToVec2(j.value("size", Json::array()), Vec2(1920, 1080));
    std::shared_ptr<Source> source;

    switch (kind) {
        case SourceKind::Texture: {
            auto s = std::make_shared<SourceTexture>();
            s->setTexture(nullptr, size);
            source = s;
            break;
        }
        case SourceKind::Fbo: {
            auto s = std::make_shared<SourceFbo>();
            s->setFbo(nullptr, size);
            source = s;
            break;
        }
        case SourceKind::Video: {
            auto s = std::make_shared<SourceVideo>();
            if (j.contains("path") && j["path"].is_string())
                s->setPath(j["path"].get<std::string>());
            s->setSize(size);
            if (j.contains("loop")) s->setLoop(j["loop"].get<bool>());
            if (j.contains("volume")) s->setVolume(j["volume"].get<float>());
            if (j.contains("playbackMode") && j["playbackMode"].is_string()) {
                SourcePlaybackMode mode = SourcePlaybackMode::FreeRun;
                if (stringToPlaybackMode(j["playbackMode"].get<std::string>(), mode))
                    s->setPlaybackMode(mode);
            }
            source = s;
            break;
        }
        case SourceKind::Image: {
            auto s = std::make_shared<SourceImage>();
            if (j.contains("path") && j["path"].is_string())
                s->setPath(j["path"].get<std::string>());
            s->setSize(size);
            source = s;
            break;
        }
        case SourceKind::Generated: {
            auto s = std::make_shared<SourceGenerated>();
            s->setSize(size);
            source = s;
            break;
        }
        case SourceKind::BuiltinPattern: {
            auto s = std::make_shared<CalibrationPatternSource>();
            BuiltinPatternKind pattern = BuiltinPatternKind::Grid;
            if (j.contains("pattern") && j["pattern"].is_string()) {
                if (!stringToBuiltinPattern(j["pattern"].get<std::string>(), pattern))
                    warnings.push_back("Unknown builtin pattern: " + j["pattern"].get<std::string>());
            }
            s->setPattern(pattern);
            s->setSize(size);
            source = s;
            break;
        }
        case SourceKind::None:
            return nullptr;
    }

    if (!source) return nullptr;
    if (j.contains("id") && j["id"].is_string()) source->setId(j["id"].get<std::string>());
    if (j.contains("name") && j["name"].is_string()) source->setName(j["name"].get<std::string>());
    if (j.contains("colorCorrection") && j["colorCorrection"].is_object())
        source->setColorCorrection(jsonToColorCorrection(j["colorCorrection"]));

    return source;
}

// =============================================================================
// Helper: Mask ↔ JSON
// =============================================================================

static Json maskToJson(const MapWrapMask& m) {
    Json j = Json{
        {"id",         m.id},
        {"name",       m.name},
        {"kind",       maskKindToString(m.kind)},
        {"operation",  maskOperationToString(m.operation)},
        {"space",      maskSpaceToString(m.space)},
        {"enabled",    m.enabled},
        {"inverted",   m.inverted},
        {"opacity",    m.opacity},
        {"featherPixels", m.featherPixels},
        {"featherNorm",   m.featherNorm}
    };

    if (!m.points.empty()) {
        Json pts = Json::array();
        for (const auto& p : m.points) pts.push_back(vec2ToJson(p));
        j["points"] = pts;
    }

    if (m.kind == MaskKind::Rectangle || m.kind == MaskKind::Ellipse) {
        j["rect"] = rectToJson(m.rect);
    }

    if (m.kind == MaskKind::AlphaTexture && !m.alphaTextureSource.empty()) {
        j["alphaTextureSource"] = m.alphaTextureSource;
    }

    return j;
}

static MapWrapMask jsonToMask(const Json& j, std::vector<std::string>& warnings) {
    MapWrapMask m;
    if (!j.is_object()) { warnings.push_back("Skipping non-object mask entry"); return m; }

    if (j.contains("id"))    m.id   = j["id"].get<std::string>();
    if (j.contains("name"))  m.name = j["name"].get<std::string>();

    if (j.contains("kind")) {
        std::string ks = j["kind"].get<std::string>();
        if (!stringToMaskKind(ks, m.kind))
            warnings.push_back("Unknown mask kind: " + ks);
    }
    if (j.contains("operation")) {
        std::string os = j["operation"].get<std::string>();
        if (!stringToMaskOperation(os, m.operation))
            warnings.push_back("Unknown mask operation: " + os);
    }
    if (j.contains("space")) {
        std::string ss = j["space"].get<std::string>();
        if (!stringToMaskSpace(ss, m.space))
            warnings.push_back("Unknown mask space: " + ss);
    }

    if (j.contains("enabled"))    m.enabled    = j["enabled"].get<bool>();
    if (j.contains("inverted"))   m.inverted   = j["inverted"].get<bool>();
    if (j.contains("opacity"))    m.opacity    = j["opacity"].get<float>();
    if (j.contains("featherPixels")) m.featherPixels = j["featherPixels"].get<float>();
    if (j.contains("featherNorm"))   m.featherNorm   = j["featherNorm"].get<float>();

    if (j.contains("points") && j["points"].is_array()) {
        for (const auto& pj : j["points"]) {
            m.points.push_back(jsonToVec2(pj));
        }
    }

    if (j.contains("rect")) {
        m.rect = jsonToRect(j["rect"]);
    }

    if (j.contains("alphaTextureSource")) {
        m.alphaTextureSource = j["alphaTextureSource"].get<std::string>();
    }

    return m;
}

// =============================================================================
// Helper: Output ↔ JSON
// =============================================================================

static Json outputToJson(const MapWrapOutput& o) {
    Json j = Json{
        {"id",               o.id},
        {"name",             o.name},
        {"canvasRegionNorm", rectToJson(o.canvasRegionNorm)},
        {"displayRegionPixels", rectToJson(o.displayRegionPixels)},
        {"pixelSize",        vec2ToJson(o.pixelSize)},
        {"contentScale",     o.contentScale},
        {"rotationDegrees",  o.rotationDegrees},
        {"enabled",          o.enabled},
        {"showTestPattern",  o.showTestPattern},
        {"blend",            blendSettingsToJson(o.blend)},
        {"colorCorrection",  colorCorrectionToJson(o.colorCorrection)}
    };

    if (!o.masks.empty()) {
        Json masks = Json::array();
        for (const auto& m : o.masks) masks.push_back(maskToJson(m));
        j["masks"] = masks;
    }

    return j;
}

static MapWrapOutput jsonToOutput(const Json& j, std::vector<std::string>& warnings) {
    MapWrapOutput o;
    if (!j.is_object()) { warnings.push_back("Skipping non-object output entry"); return o; }

    if (j.contains("id"))               o.id               = j["id"].get<std::string>();
    if (j.contains("name"))             o.name             = j["name"].get<std::string>();
    if (j.contains("canvasRegionNorm")) o.canvasRegionNorm = jsonToRect(j["canvasRegionNorm"], o.canvasRegionNorm);
    if (j.contains("displayRegionPixels")) o.displayRegionPixels = jsonToRect(j["displayRegionPixels"], o.displayRegionPixels);
    if (j.contains("pixelSize"))        o.pixelSize        = jsonToVec2(j["pixelSize"], o.pixelSize);
    if (j.contains("contentScale"))     o.contentScale     = j["contentScale"].get<float>();
    if (j.contains("rotationDegrees"))  o.rotationDegrees  = j["rotationDegrees"].get<float>();
    if (j.contains("enabled"))          o.enabled          = j["enabled"].get<bool>();
    if (j.contains("showTestPattern"))  o.showTestPattern  = j["showTestPattern"].get<bool>();
    if (j.contains("blend"))            o.blend            = jsonToBlendSettings(j["blend"]);
    if (j.contains("colorCorrection"))  o.colorCorrection  = jsonToColorCorrection(j["colorCorrection"]);

    if (j.contains("masks") && j["masks"].is_array()) {
        for (const auto& mj : j["masks"]) {
            o.masks.push_back(jsonToMask(mj, warnings));
        }
    }

    return o;
}

// =============================================================================
// Helper: Surface serialization
// =============================================================================

static Json surfaceToJson(const Surface& s) {
    Json j = Json{
        {"id",         s.id()},
        {"kind",       surfaceKindToString(s.kind())},
        {"name",       s.name()},
        {"visible",    s.isVisible()},
        {"locked",     s.isLocked()},
        {"source",     s.source()},
        {"opacity",    s.opacity()},
        {"sourceRect", rectToJson(s.sourceRect())},
        {"blend",      blendSettingsToJson(s.blend())},
        {"colorCorrection", colorCorrectionToJson(s.colorCorrection())}
    };

    // Masks
    if (!s.masks().empty()) {
        Json masks = Json::array();
        for (const auto& m : s.masks()) masks.push_back(maskToJson(m));
        j["masks"] = masks;
    }

    // SurfaceKind-specific
    switch (s.kind()) {
        case SurfaceKind::Quad: {
            const auto& quad = static_cast<const SurfaceQuad&>(s);
            Json dest = Json::array();
            for (const auto& pt : quad.destinationPoints()) dest.push_back(vec2ToJson(pt));
            j["destinationPoints"] = dest;
            Json uv = Json::array();
            for (const auto& pt : quad.uvPoints()) uv.push_back(vec2ToJson(pt));
            j["uvPoints"] = uv;
            j["perspectiveCorrection"] = quad.perspectiveCorrection();
            j["meshResolution"] = quad.meshResolution();
            break;
        }
        case SurfaceKind::Grid: {
            const auto& grid = static_cast<const SurfaceGrid&>(s);
            j["cols"] = grid.cols();
            j["rows"] = grid.rows();
            Json pts = Json::array();
            for (int r = 0; r <= grid.rows(); ++r) {
                for (int c = 0; c <= grid.cols(); ++c) {
                    pts.push_back(vec2ToJson(grid.gridPoint(c, r)));
                }
            }
            j["gridPoints"] = pts;
            j["curvedInterpolation"] = grid.curvedInterpolation();
            j["meshResolution"] = grid.meshResolution();
            break;
        }
        case SurfaceKind::Bezier: {
            const auto& bezier = static_cast<const SurfaceBezier&>(s);
            j["controlCols"] = bezier.controlCols();
            j["controlRows"] = bezier.controlRows();
            Json pts = Json::array();
            for (int r = 0; r < bezier.controlRows(); ++r) {
                for (int c = 0; c < bezier.controlCols(); ++c) {
                    pts.push_back(vec2ToJson(bezier.controlPoint(c, r)));
                }
            }
            j["controlPoints"] = pts;
            j["meshResolution"] = bezier.meshResolution();
            break;
        }
        case SurfaceKind::Triangle: {
            const auto& tri = static_cast<const SurfaceTriangle&>(s);
            Json dest = Json::array();
            for (const auto& pt : tri.destinationPoints()) dest.push_back(vec2ToJson(pt));
            j["destinationPoints"] = dest;
            Json uv = Json::array();
            for (const auto& pt : tri.uvPoints()) uv.push_back(vec2ToJson(pt));
            j["uvPoints"] = uv;
            break;
        }
        case SurfaceKind::Circle: {
            const auto& circ = static_cast<const SurfaceCircle&>(s);
            j["center"]  = vec2ToJson(circ.center());
            j["radiusX"] = circ.radiusX();
            j["radiusY"] = circ.radiusY();
            j["rotation"] = circ.rotation();
            j["segments"] = circ.segments();
            break;
        }
        case SurfaceKind::Polygon: {
            const auto& poly = static_cast<const SurfacePolygon&>(s);
            Json dest = Json::array();
            for (const auto& pt : poly.destinationPoints()) dest.push_back(vec2ToJson(pt));
            j["destinationPoints"] = dest;
            Json uv = Json::array();
            for (const auto& pt : poly.uvPoints()) uv.push_back(vec2ToJson(pt));
            j["uvPoints"] = uv;
            j["closed"] = poly.closed();
            break;
        }
    }

    return j;
}

static std::shared_ptr<Surface> jsonToSurface(const Json& j, std::vector<std::string>& warnings) {
    if (!j.is_object()) {
        warnings.push_back("Skipping non-object surface entry");
        return nullptr;
    }

    SurfaceKind kind = SurfaceKind::Quad;
    if (j.contains("kind")) {
        std::string ks = j["kind"].get<std::string>();
        if (!stringToSurfaceKind(ks, kind)) {
            warnings.push_back("Unknown surface kind: " + ks + ", skipping");
            return nullptr;
        }
    }

    // Create surface by kind
    std::shared_ptr<Surface> surface;
    switch (kind) {
        case SurfaceKind::Quad:
            surface = std::make_shared<SurfaceQuad>();
            break;
        case SurfaceKind::Grid: {
            int cols = 3, rows = 3;
            if (j.contains("cols")) cols = j["cols"].get<int>();
            if (j.contains("rows")) rows = j["rows"].get<int>();
            cols = std::max(2, cols);
            rows = std::max(2, rows);
            surface = std::make_shared<SurfaceGrid>(cols, rows);
            break;
        }
        case SurfaceKind::Bezier: {
            int controlCols = 4, controlRows = 4;
            if (j.contains("controlCols")) controlCols = j["controlCols"].get<int>();
            if (j.contains("controlRows")) controlRows = j["controlRows"].get<int>();
            controlCols = std::max(2, controlCols);
            controlRows = std::max(2, controlRows);
            surface = std::make_shared<SurfaceBezier>(controlCols, controlRows);
            break;
        }
        case SurfaceKind::Triangle:
            surface = std::make_shared<SurfaceTriangle>();
            break;
        case SurfaceKind::Circle:
            surface = std::make_shared<SurfaceCircle>();
            break;
        case SurfaceKind::Polygon:
            surface = std::make_shared<SurfacePolygon>();
            break;
    }

    // Common properties
    if (j.contains("id"))     surface->setId(j["id"].get<std::string>());
    if (j.contains("name"))   surface->setName(j["name"].get<std::string>());
    if (j.contains("visible"))surface->setVisible(j["visible"].get<bool>());
    if (j.contains("locked")) surface->setLocked(j["locked"].get<bool>());
    if (j.contains("source")) surface->setSource(j["source"].get<std::string>());
    if (j.contains("opacity"))surface->setOpacity(j["opacity"].get<float>());
    if (j.contains("sourceRect")) surface->setSourceRect(jsonToRect(j["sourceRect"], surface->sourceRect()));
    if (j.contains("blend"))  surface->setBlend(jsonToBlendSettings(j["blend"]));
    if (j.contains("colorCorrection")) surface->setColorCorrection(jsonToColorCorrection(j["colorCorrection"]));

    // Masks
    if (j.contains("masks") && j["masks"].is_array()) {
        auto& masks = surface->masks();
        for (const auto& mj : j["masks"]) {
            masks.push_back(jsonToMask(mj, warnings));
        }
    }

    // SurfaceKind-specific
    switch (kind) {
        case SurfaceKind::Quad: {
            auto& quad = static_cast<SurfaceQuad&>(*surface);
            if (j.contains("destinationPoints") && j["destinationPoints"].is_array()) {
                auto& pts = quad.destinationPoints();
                const auto& arr = j["destinationPoints"];
                for (size_t i = 0; i < pts.size() && i < arr.size(); ++i) {
                    pts[i] = jsonToVec2(arr[i], pts[i]);
                }
            }
            if (j.contains("uvPoints") && j["uvPoints"].is_array()) {
                auto& pts = quad.uvPoints();
                const auto& arr = j["uvPoints"];
                for (size_t i = 0; i < pts.size() && i < arr.size(); ++i) {
                    pts[i] = jsonToVec2(arr[i], pts[i]);
                }
            }
            if (j.contains("perspectiveCorrection")) {
                quad.setPerspectiveCorrection(j["perspectiveCorrection"].get<bool>());
            }
            if (j.contains("meshResolution")) {
                quad.setMeshResolution(j["meshResolution"].get<int>());
            }
            break;
        }
        case SurfaceKind::Grid: {
            auto& grid = static_cast<SurfaceGrid&>(*surface);
            if (j.contains("gridPoints") && j["gridPoints"].is_array()) {
                const auto& arr = j["gridPoints"];
                int expectedCount = (grid.cols() + 1) * (grid.rows() + 1);
                for (int i = 0; i < expectedCount && i < (int)arr.size(); ++i) {
                    int c = i % (grid.cols() + 1);
                    int r = i / (grid.cols() + 1);
                    grid.setGridPoint(c, r, jsonToVec2(arr[i], grid.gridPoint(c, r)));
                }
                if ((int)arr.size() < expectedCount) {
                    warnings.push_back("Surface " + surface->id() + ": gridPoints has fewer entries than expected");
                }
            }
            if (j.contains("curvedInterpolation")) {
                grid.setCurvedInterpolation(j["curvedInterpolation"].get<bool>());
            }
            if (j.contains("meshResolution")) {
                grid.setMeshResolution(j["meshResolution"].get<int>());
            }
            break;
        }
        case SurfaceKind::Bezier: {
            auto& bezier = static_cast<SurfaceBezier&>(*surface);
            if (j.contains("controlPoints") && j["controlPoints"].is_array()) {
                const auto& arr = j["controlPoints"];
                int expectedCount = bezier.controlCols() * bezier.controlRows();
                for (int i = 0; i < expectedCount && i < (int)arr.size(); ++i) {
                    int c = i % bezier.controlCols();
                    int r = i / bezier.controlCols();
                    bezier.setControlPoint(c, r, jsonToVec2(arr[i], bezier.controlPoint(c, r)));
                }
                if ((int)arr.size() < expectedCount) {
                    warnings.push_back("Surface " + surface->id() + ": controlPoints has fewer entries than expected");
                }
            }
            if (j.contains("meshResolution")) {
                bezier.setMeshResolution(j["meshResolution"].get<int>());
            }
            break;
        }
        case SurfaceKind::Triangle: {
            auto& tri = static_cast<SurfaceTriangle&>(*surface);
            if (j.contains("destinationPoints") && j["destinationPoints"].is_array()) {
                auto& pts = tri.destinationPoints();
                const auto& arr = j["destinationPoints"];
                for (size_t i = 0; i < pts.size() && i < arr.size(); ++i) {
                    pts[i] = jsonToVec2(arr[i], pts[i]);
                }
            }
            if (j.contains("uvPoints") && j["uvPoints"].is_array()) {
                auto& pts = tri.uvPoints();
                const auto& arr = j["uvPoints"];
                for (size_t i = 0; i < pts.size() && i < arr.size(); ++i) {
                    pts[i] = jsonToVec2(arr[i], pts[i]);
                }
            }
            break;
        }
        case SurfaceKind::Circle: {
            auto& circ = static_cast<SurfaceCircle&>(*surface);
            if (j.contains("center"))  circ.setCenter(jsonToVec2(j["center"], circ.center()));
            if (j.contains("radiusX")) circ.setRadiusX(j["radiusX"].get<float>());
            if (j.contains("radiusY")) circ.setRadiusY(j["radiusY"].get<float>());
            if (j.contains("rotation"))circ.setRotation(j["rotation"].get<float>());
            if (j.contains("segments"))circ.setSegments(j["segments"].get<int>());
            break;
        }
        case SurfaceKind::Polygon: {
            auto& poly = static_cast<SurfacePolygon&>(*surface);
            if (j.contains("destinationPoints") && j["destinationPoints"].is_array()) {
                auto& pts = poly.destinationPoints();
                pts.clear();
                for (const auto& pj : j["destinationPoints"]) {
                    pts.push_back(jsonToVec2(pj));
                }
            }
            if (j.contains("uvPoints") && j["uvPoints"].is_array()) {
                auto& pts = poly.uvPoints();
                pts.clear();
                for (const auto& pj : j["uvPoints"]) {
                    pts.push_back(jsonToVec2(pj));
                }
            }
            if (j.contains("closed")) {
                poly.setClosed(j["closed"].get<bool>());
            }
            break;
        }
    }

    surface->markDirty();
    return surface;
}

// =============================================================================
// Helper: Group ↔ JSON
// =============================================================================

static Json groupToJson(const SurfaceGroup& g) {
    Json j = Json{
        {"id",       g.id},
        {"name",     g.name},
        {"visible",  g.visible},
        {"locked",   g.locked},
        {"opacity",  g.opacity},
        {"translate",vec2ToJson(g.translate)},
        {"rotation", g.rotation},
        {"scale",    vec2ToJson(g.scale)}
    };

    if (!g.surfaceIds.empty()) {
        j["surfaceIds"] = g.surfaceIds;
    }

    return j;
}

static std::shared_ptr<SurfaceGroup> jsonToGroup(const Json& j, std::vector<std::string>& warnings) {
    auto group = std::make_shared<SurfaceGroup>();
    if (!j.is_object()) { warnings.push_back("Skipping non-object group entry"); return group; }

    if (j.contains("id"))       group->id       = j["id"].get<std::string>();
    if (j.contains("name"))     group->name     = j["name"].get<std::string>();
    if (j.contains("visible"))  group->visible  = j["visible"].get<bool>();
    if (j.contains("locked"))   group->locked   = j["locked"].get<bool>();
    if (j.contains("opacity"))  group->opacity  = j["opacity"].get<float>();
    if (j.contains("translate"))group->translate= jsonToVec2(j["translate"], group->translate);
    if (j.contains("rotation")) group->rotation = j["rotation"].get<float>();
    if (j.contains("scale"))    group->scale    = jsonToVec2(j["scale"], group->scale);

    if (j.contains("surfaceIds") && j["surfaceIds"].is_array()) {
        for (const auto& sid : j["surfaceIds"]) {
            group->surfaceIds.push_back(sid.get<std::string>());
        }
    }

    return group;
}

// =============================================================================
// Helper: Cue ↔ JSON
// =============================================================================

static Json cueToJson(const MapWrapCue& c) {
    Json j = Json{
        {"id",                c.id},
        {"name",              c.name},
        {"transitionSeconds", c.transitionSeconds}
    };

    if (!c.surfacePatches.empty()) {
        Json patches = Json::array();
        for (const auto& p : c.surfacePatches) {
            // Try to parse the existing patchJson; if it's valid JSON, embed it directly
            // Otherwise store as string
            Json patchObj;
            try {
                patchObj = Json::parse(p.patchJson);
            } catch (...) {
                patchObj = p.patchJson;
            }
            patches.push_back(Json{
                {"surfaceId", p.surfaceId},
                {"patch",     patchObj}
            });
        }
        j["surfacePatches"] = patches;
    }

    if (!c.sourcePatches.empty()) {
        Json patches = Json::array();
        for (const auto& p : c.sourcePatches) {
            Json patchObj;
            try {
                patchObj = Json::parse(p.patchJson);
            } catch (...) {
                patchObj = p.patchJson;
            }
            patches.push_back(Json{
                {"sourceId", p.sourceId},
                {"patch",    patchObj}
            });
        }
        j["sourcePatches"] = patches;
    }

    if (!c.outputPatches.empty()) {
        Json patches = Json::array();
        for (const auto& p : c.outputPatches) {
            Json patchObj;
            try {
                patchObj = Json::parse(p.patchJson);
            } catch (...) {
                patchObj = p.patchJson;
            }
            patches.push_back(Json{
                {"outputId", p.outputId},
                {"patch",    patchObj}
            });
        }
        j["outputPatches"] = patches;
    }

    return j;
}

static std::shared_ptr<MapWrapCue> jsonToCue(const Json& j, std::vector<std::string>& warnings) {
    auto cue = std::make_shared<MapWrapCue>();
    if (!j.is_object()) { warnings.push_back("Skipping non-object cue entry"); return cue; }

    if (j.contains("id"))   cue->id   = j["id"].get<std::string>();
    if (j.contains("name")) cue->name = j["name"].get<std::string>();
    if (j.contains("transitionSeconds")) cue->transitionSeconds = j["transitionSeconds"].get<float>();

    if (j.contains("surfacePatches") && j["surfacePatches"].is_array()) {
        for (const auto& pj : j["surfacePatches"]) {
            SurfaceStatePatch patch;
            if (pj.contains("surfaceId")) patch.surfaceId = pj["surfaceId"].get<std::string>();
            if (pj.contains("patch")) {
                patch.patchJson = pj["patch"].dump();
            }
            cue->surfacePatches.push_back(std::move(patch));
        }
    }

    if (j.contains("sourcePatches") && j["sourcePatches"].is_array()) {
        for (const auto& pj : j["sourcePatches"]) {
            SourceStatePatch patch;
            if (pj.contains("sourceId")) patch.sourceId = pj["sourceId"].get<std::string>();
            if (pj.contains("patch")) {
                patch.patchJson = pj["patch"].dump();
            }
            cue->sourcePatches.push_back(std::move(patch));
        }
    }

    if (j.contains("outputPatches") && j["outputPatches"].is_array()) {
        for (const auto& pj : j["outputPatches"]) {
            OutputStatePatch patch;
            if (pj.contains("outputId")) patch.outputId = pj["outputId"].get<std::string>();
            if (pj.contains("patch")) {
                patch.patchJson = pj["patch"].dump();
            }
            cue->outputPatches.push_back(std::move(patch));
        }
    }

    return cue;
}

// =============================================================================
// Helper: Stage ↔ JSON
// =============================================================================

static Json stageToJson(const MapWrapStage& stage) {
    Json j;

    // Outputs
    Json outputs = Json::array();
    for (const auto& o : stage.outputs()) outputs.push_back(outputToJson(o));
    j["outputs"] = outputs;

    // Global masks
    Json globalMasks = Json::array();
    for (const auto& m : stage.globalMasks()) globalMasks.push_back(maskToJson(m));
    j["globalMasks"] = globalMasks;

    return j;
}

static void jsonToStage(MapWrapStage& stage, const Json& j, std::vector<std::string>& warnings) {
    if (!j.is_object()) return;

    // Outputs
    if (j.contains("outputs") && j["outputs"].is_array()) {
        auto& outputs = stage.outputs();
        outputs.clear();
        for (const auto& oj : j["outputs"]) {
            outputs.push_back(jsonToOutput(oj, warnings));
        }
    }

    // Global masks
    if (j.contains("globalMasks") && j["globalMasks"].is_array()) {
        auto& masks = stage.globalMasks();
        masks.clear();
        for (const auto& mj : j["globalMasks"]) {
            masks.push_back(jsonToMask(mj, warnings));
        }
    }
}

// =============================================================================
// Helper: Workspace ↔ JSON
// =============================================================================

static Json workspaceToJson(EditMode editMode, const SurfaceId& selectedSurface, const EditorViewport& vp) {
    return Json{
        {"editMode",         editModeToString(editMode)},
        {"selectedSurface",  selectedSurface},
        {"editorViewport",   Json{
            {"panPixels", vec2ToJson(vp.panPixels)},
            {"zoom",      vp.zoom}
        }}
    };
}

static void jsonToWorkspace(const Json& j, EditMode& editMode, SurfaceId& selectedSurface,
                            EditorViewport& vp, std::vector<std::string>& warnings) {
    if (!j.is_object()) return;

    if (j.contains("editMode")) {
        std::string emStr = j["editMode"].get<std::string>();
        if (!stringToEditMode(emStr, editMode)) {
            warnings.push_back("Unknown edit mode: " + emStr);
        }
    }

    if (j.contains("selectedSurface") && j["selectedSurface"].is_string()) {
        selectedSurface = j["selectedSurface"].get<std::string>();
    }

    if (j.contains("editorViewport") && j["editorViewport"].is_object()) {
        const auto& evj = j["editorViewport"];
        if (evj.contains("panPixels")) vp.panPixels = jsonToVec2(evj["panPixels"], vp.panPixels);
        if (evj.contains("zoom"))      vp.zoom      = evj["zoom"].get<float>();
    }
}

static Json documentToJson(const MapWrapDocument& document, const SourceRegistry* sources) {
    Json root;
    root["schema"]  = MapWrapSerialization::schemaName();
    root["version"] = MapWrapSerialization::schemaVersion();
    root["name"]    = document.name();

    // Design canvas size
    Vec2 canvasSize = document.designCanvasSize();
    root["designCanvasSize"] = Json::array({canvasSize.x, canvasSize.y});

    // Language
    root["language"] = MapWrapI18n::instance().language();

    // Stage
    root["stage"] = stageToJson(document.stage());

    root["sources"] = sources ? sourcesToJson(*sources) : Json::array();

    // Surfaces
    {
        Json surfaces = Json::array();
        for (const auto& s : document.surfaces()) {
            if (s) surfaces.push_back(surfaceToJson(*s));
        }
        root["surfaces"] = surfaces;
    }

    // Surface groups
    {
        Json groups = Json::array();
        for (const auto& g : document.groups()) {
            if (g) groups.push_back(groupToJson(*g));
        }
        root["surfaceGroups"] = groups;
    }

    // Cues
    {
        Json cues = Json::array();
        for (const auto& c : document.cues()) {
            if (c) cues.push_back(cueToJson(*c));
        }
        root["cues"] = cues;
    }

    // Workspace — default values (editor state is transient)
    {
        EditorViewport defaultViewport;
        root["workspace"] = workspaceToJson(EditMode::Presentation, "", defaultViewport);
    }

    return root;
}

// =============================================================================
// saveToString
// =============================================================================

std::string MapWrapSerialization::saveToString(const MapWrapDocument& document) {
    return documentToJson(document, nullptr).dump(2);
}

std::string MapWrapSerialization::saveToString(const MapWrapDocument& document,
                                               const SourceRegistry& sources) {
    return documentToJson(document, &sources).dump(2);
}

// =============================================================================
// loadFromString
// =============================================================================

LoadResult MapWrapSerialization::loadFromString(MapWrapDocument& document, const std::string& jsonStr) {
    std::vector<std::string> warnings;

    Json root;
    try {
        root = Json::parse(jsonStr);
    } catch (const Json::parse_error& e) {
        return LoadResult::error(std::string("JSON parse error: ") + e.what());
    }

    if (!root.is_object()) {
        return LoadResult::error("Root JSON is not an object");
    }

    // Schema check
    if (root.contains("schema")) {
        std::string schema = root["schema"].get<std::string>();
        if (schema != schemaName()) {
            warnings.push_back("Unexpected schema: " + schema + " (expected " + schemaName() + ")");
        }
    } else {
        warnings.push_back("Missing schema field");
    }

    // Version check
    if (root.contains("version")) {
        int version = root["version"].get<int>();
        if (version > schemaVersion()) {
            warnings.push_back("File version " + std::to_string(version) +
                " is newer than supported version " + std::to_string(schemaVersion()) +
                "; some data may be lost");
        }
    } else {
        warnings.push_back("Missing version field");
    }

    document.clear();

    // Name
    if (root.contains("name") && root["name"].is_string()) {
        document.setName(root["name"].get<std::string>());
    }

    // Design canvas size
    if (root.contains("designCanvasSize") && root["designCanvasSize"].is_array()) {
        Vec2 size = jsonToVec2(root["designCanvasSize"], document.designCanvasSize());
        document.setDesignCanvasSize(size);
    }

    // Language — restore i18n state if present
    if (root.contains("language") && root["language"].is_string()) {
        std::string lang = root["language"].get<std::string>();
        if (lang != "auto") {
            MapWrapI18n::instance().setLanguage(lang);
        }
    }

    // Stage
    if (root.contains("stage") && root["stage"].is_object()) {
        jsonToStage(document.stage(), root["stage"], warnings);
    } else {
        // Backward compat: missing stage → create default
        document.stage().ensureDefaultOutput();
        warnings.push_back("Missing stage section; created default output");
    }

    // Sources — currently a placeholder array, skip
    if (!root.contains("sources")) {
        warnings.push_back("Missing sources section");
    }

    // Surfaces
    if (root.contains("surfaces") && root["surfaces"].is_array()) {
        for (const auto& sj : root["surfaces"]) {
            auto surface = jsonToSurface(sj, warnings);
            if (surface) {
                document.addSurface(surface);
            }
        }
    } else {
        warnings.push_back("Missing or invalid surfaces section");
    }

    // Surface groups
    if (root.contains("surfaceGroups") && root["surfaceGroups"].is_array()) {
        for (const auto& gj : root["surfaceGroups"]) {
            auto group = jsonToGroup(gj, warnings);
            if (group) {
                document.addGroup(group);
            }
        }
    }
    // Backward compat: missing groups is fine — just no groups loaded

    // Cues
    if (root.contains("cues") && root["cues"].is_array()) {
        for (const auto& cj : root["cues"]) {
            auto cue = jsonToCue(cj, warnings);
            if (cue) {
                document.addCue(cue);
            }
        }
    }

    // Workspace — transient, no-op for document but would apply to editor if wired
    if (root.contains("workspace") && root["workspace"].is_object()) {
        EditMode editMode = EditMode::Presentation;
        SurfaceId selectedSurface;
        EditorViewport vp;
        jsonToWorkspace(root["workspace"], editMode, selectedSurface, vp, warnings);
        // Workspace state is editor-level, not document-level.
        // The caller (MapWrapEngine or MapWrapEditor) should extract this separately.
    }

    // Skip unknown top-level fields silently
    document.clearDirty();

    if (warnings.empty()) {
        return LoadResult::success();
    }
    return LoadResult::successWithWarnings(std::move(warnings));
}

LoadResult MapWrapSerialization::loadFromString(MapWrapDocument& document,
                                                SourceRegistry& sources,
                                                const std::string& jsonStr) {
    LoadResult result = loadFromString(document, jsonStr);
    if (!result.ok) return result;

    Json root;
    try {
        root = Json::parse(jsonStr);
    } catch (const Json::parse_error& e) {
        return LoadResult::error(std::string("JSON parse error: ") + e.what());
    }

    if (!root.contains("sources")) {
        result.warnings.push_back("Missing sources section");
    } else if (!root["sources"].is_array()) {
        result.warnings.push_back("Invalid sources section");
    } else {
        sources.clear();
        for (const auto& sj : root["sources"]) {
            auto source = jsonToSource(sj, result.warnings);
            if (source) {
                sources.add(source);
            }
        }
    }

    if (result.warnings.empty()) {
        return LoadResult::success();
    }
    return LoadResult::successWithWarnings(std::move(result.warnings));
}

// =============================================================================
// saveToFile / loadFromFile
// =============================================================================

Result MapWrapSerialization::saveToFile(const MapWrapDocument& document, const std::string& path) {
    std::string json = saveToString(document);

    std::ofstream ofs(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        return Result::error("Cannot open file for writing: " + path);
    }

    ofs.write(json.data(), json.size());
    if (ofs.fail()) {
        return Result::error("Write failed: " + path);
    }

    ofs.close();
    return Result::success();
}

Result MapWrapSerialization::saveToFile(const MapWrapDocument& document,
                                        const SourceRegistry& sources,
                                        const std::string& path) {
    std::string json = saveToString(document, sources);

    std::ofstream ofs(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        return Result::error("Cannot open file for writing: " + path);
    }

    ofs.write(json.data(), json.size());
    if (ofs.fail()) {
        return Result::error("Write failed: " + path);
    }

    ofs.close();
    return Result::success();
}

LoadResult MapWrapSerialization::loadFromFile(MapWrapDocument& document, const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        return LoadResult::error("Cannot open file for reading: " + path);
    }

    std::ostringstream ss;
    ss << ifs.rdbuf();
    if (ifs.fail() && !ifs.eof()) {
        return LoadResult::error("Read failed: " + path);
    }

    return loadFromString(document, ss.str());
}

LoadResult MapWrapSerialization::loadFromFile(MapWrapDocument& document,
                                              SourceRegistry& sources,
                                              const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        return LoadResult::error("Cannot open file for reading: " + path);
    }

    std::ostringstream ss;
    ss << ifs.rdbuf();
    if (ifs.fail() && !ifs.eof()) {
        return LoadResult::error("Read failed: " + path);
    }

    return loadFromString(document, sources, ss.str());
}

} // namespace mapwrap
} // namespace tcx
