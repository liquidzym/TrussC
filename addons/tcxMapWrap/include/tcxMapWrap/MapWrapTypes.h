#pragma once
// =============================================================================
// tcxMapWrap — Core Types
// =============================================================================

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <memory>
#include <functional>
#include <optional>
#include <algorithm>

// JSON — include BEFORE namespace to avoid namespace pollution
#include "nlohmann/json.hpp"

// ===========================================================================
// Math types — use TrussC core types when available, otherwise fallback
// ===========================================================================
#if __has_include(<tcMath.h>)
    // TrussC integration: use core math types
    #include <tcMath.h>
    #include <tc/types/tcRectangle.h>
    namespace tcx {
    namespace mapwrap {
        using Vec2 = trussc::Vec2;
        using Vec3 = trussc::Vec3;
        using Vec4 = trussc::Vec4;

        // Rect wrapper: TrussC Rect uses width/height; we provide w/h aliases
        struct Rect {
            float x = 0.0f, y = 0.0f;
            float w = 0.0f, h = 0.0f;
            Rect() = default;
            Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
            // Implicit conversion to/from trussc::Rect
            operator trussc::Rect() const { return trussc::Rect(x, y, w, h); }
            Rect(const trussc::Rect& r) : x(r.x), y(r.y), w(r.width), h(r.height) {}
        };
    } // namespace mapwrap
    } // namespace tcx
#else
    // Standalone build: define our own types
    namespace tcx {
    namespace mapwrap {
        struct Vec2 {
            float x = 0.0f;
            float y = 0.0f;
            Vec2() = default;
            Vec2(float x_, float y_) : x(x_), y(y_) {}
            Vec2 operator+(const Vec2& v) const { return Vec2(x+v.x, y+v.y); }
            Vec2 operator-(const Vec2& v) const { return Vec2(x-v.x, y-v.y); }
            Vec2 operator*(float s) const { return Vec2(x*s, y*s); }
            Vec2 operator/(float s) const { return Vec2(x/s, y/s); }
            Vec2 operator-() const { return Vec2(-x, -y); }
            Vec2& operator+=(const Vec2& v) { x+=v.x; y+=v.y; return *this; }
            bool operator==(const Vec2& v) const { return x==v.x && y==v.y; }
        };
        struct Vec3 {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            Vec3() = default;
            Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
        };
        struct Vec4 {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            float w = 0.0f;
            Vec4() = default;
            Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
        };
        struct Rect {
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
            Rect() = default;
            Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
        };
    } // namespace mapwrap
    } // namespace tcx
#endif

// Mat3 is always our own (TrussC has Mat4 but no Mat3)
namespace tcx {
namespace mapwrap {

struct Mat3 {
    float m[9] = {1,0,0, 0,1,0, 0,0,1};
};

struct IVec2 {
    int x = 0;
    int y = 0;
    IVec2() = default;
    IVec2(int x_, int y_) : x(x_), y(y_) {}
};

// ---------------------------------------------------------------------------
// JSON type alias
// ---------------------------------------------------------------------------
using MapWrapJson = nlohmann::json;

// ---------------------------------------------------------------------------
// Result types
// ---------------------------------------------------------------------------
struct Result {
    bool ok = false;
    std::string message;
    static Result success() { return {true, ""}; }
    static Result error(const std::string& msg) { return {false, msg}; }
};

template <typename T>
struct ResultT {
    bool ok = false;
    T value{};
    std::string message;
    static ResultT success(const T& v) { return {true, v, ""}; }
    static ResultT error(const std::string& msg) { return {false, T{}, msg}; }
};

struct LoadResult {
    bool ok = false;
    std::string message;
    std::vector<std::string> warnings;
    static LoadResult success() { return {true, "", {}}; }
    static LoadResult successWithWarnings(std::vector<std::string> w) { return {true, "", std::move(w)}; }
    static LoadResult error(const std::string& msg) { return {false, msg, {}}; }
};

// ---------------------------------------------------------------------------
// ID types
// ---------------------------------------------------------------------------
using SurfaceId = std::string;
using SourceId  = std::string;
using LayerId   = std::string;
using OutputId  = std::string;
using MaskId    = std::string;
using GroupId   = std::string;
using CueId     = std::string;

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------
enum class SurfaceKind {
    Quad,
    Grid,
    Bezier,
    Triangle,
    Circle,
    Polygon
};

enum class WarpKind {
    None,
    Perspective,
    Grid,
    PerspectiveGrid
};

enum class SourceKind {
    None,
    Texture,
    Fbo,
    Video,
    Image,
    Generated,
    BuiltinPattern
};

enum class EditMode {
    Presentation,
    SurfaceEdit,
    TextureEdit,
    SourceAssign,
    MaskEdit,
    OutputEdit
};

enum class HandleKind {
    None,
    Vertex,
    Edge,
    GridPoint,
    Body,
    TextureVertex,
    SourceItem,
    MaskPoint,
    MaskEdge,
    OutputCorner,
    RotationHandle
};

enum class MaskKind {
    Rectangle,
    Ellipse,
    Polygon,
    Bezier,
    Freehand,
    AlphaTexture
};

enum class MaskOperation {
    Add,
    Subtract,
    Intersect
};

enum class MaskSpace {
    SurfaceLocal,
    SourceUV,
    Canvas,
    Output
};

enum class BlendMode {
    Normal,
    Add,
    Multiply,
    Screen,
    Lighten,
    Darken,
    AlphaMask
};

enum class PerformanceMode {
    Quality,
    Balanced,
    LowPower
};

enum class BuiltinPatternKind {
    Checkerboard,
    Grid,
    FineGrid,
    Crosshair,
    CornerLabels,
    UVGradient,
    ColorBars,
    LumaRamp,
    EdgeBlendRamp,
    AlphaRadial,
    NumberedCells,
    SafeArea,
    SolidColor
};

enum class SourcePlaybackMode {
    Manual,
    FollowGlobalClock,
    FreeRun
};

enum class PropertyKind {
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Rect,
    Enum,
    Color
};

// ---------------------------------------------------------------------------
// Data structs
// ---------------------------------------------------------------------------
struct BlendSettings {
    float opacity = 1.0f;
    float brightness = 1.0f;
    Vec4 edge = Vec4(0, 0, 0, 0);       // left, top, right, bottom
    Vec3 gamma = Vec3(1, 1, 1);
    Vec3 luminance = Vec3(0.5f, 0.5f, 0.5f);
    float exponent = 1.0f;
    bool enabled = false;
};

struct ColorCorrection {
    bool enabled = false;
    float opacity = 1.0f;
    float brightness = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    Vec3 gamma = Vec3(1, 1, 1);
    Vec3 lift = Vec3(0, 0, 0);
    Vec3 gain = Vec3(1, 1, 1);
    float blackLevel = 0.0f;
    float whiteLevel = 1.0f;
    bool premultipliedAlpha = false;
};

struct SnapSettings {
    bool enabled = false;
    bool snapToCanvasEdges = true;
    bool snapToCanvasCenter = true;
    bool snapToOtherSurfaces = true;
    bool snapToGrid = true;
    float gridStepNorm = 0.05f;
    float thresholdPixels = 8.0f;
};

struct PerformanceSettings {
    PerformanceMode mode = PerformanceMode::Balanced;
    int maxGridSubdivision = 64;
    int maxCircleSegments = 128;
    bool rebuildMeshesAsync = false;
    bool reduceOverlayDetailWhileDragging = true;
    bool pauseVideoWhenHidden = true;
};

struct RenderStats {
    int drawnSurfaceCount = 0;
    int skippedSurfaceCount = 0;
    int rebuiltMeshCount = 0;
    int missingSourceCount = 0;
    int invalidSurfaceCount = 0;
    int maskCount = 0;
    int texturedDrawCount = 0;
    int placeholderDrawCount = 0;
    int activeVideoSourceCount = 0;
    int pausedVideoSourceCount = 0;
    int alphaMaskCount = 0;
    int featheredMaskCount = 0;
};

// ---------------------------------------------------------------------------
// Mesh data
// ---------------------------------------------------------------------------
struct MeshData {
    std::vector<float> vertices;    // x,y pairs (2 floats per vertex)
    std::vector<float> uvs;         // u,v pairs (2 floats per vertex)
    std::vector<uint32_t> indices;  // triangle indices

    void clear() {
        vertices.clear();
        uvs.clear();
        indices.clear();
    }

    size_t vertexCount() const { return vertices.size() / 2; }

    void addVertex(float x, float y, float u, float v) {
        vertices.push_back(x);
        vertices.push_back(y);
        uvs.push_back(u);
        uvs.push_back(v);
    }

    void addTriangle(uint32_t a, uint32_t b, uint32_t c) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }
};

struct GeometryValidation {
    bool valid = true;
    bool selfIntersecting = false;
    bool tooSmall = false;
    bool windingFlipped = false;
    bool hasNaN = false;
    std::string message;
};

struct ProjectAsset {
    std::string id;
    SourceId sourceId;
    std::string originalPath;
    std::string relativePath;
    bool exists = false;
    uint64_t fileSize = 0;
    std::string contentHash;
};

struct ProjectValidationReport {
    bool ok = true;
    std::vector<std::string> missingSources;
    std::vector<std::string> missingFiles;
    std::vector<std::string> warnings;
};

struct AutosaveSettings {
    bool enabled = true;
    float intervalSeconds = 30.0f;
    int maxBackups = 10;
    std::string autosaveFolder;
};

} // namespace mapwrap
} // namespace tcx
