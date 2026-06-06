// =============================================================================
// tcxMapWrap — MapWrapDocument.cpp Implementation
// =============================================================================

#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapStage.h"
#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/SurfaceTriangle.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/SurfaceGroup.h"
#include "tcxMapWrap/MapWrapCue.h"
#include "tcxMapWrap/SourceVideo.h"
#include "tcxMapWrap/SourceImage.h"
#include "tcxMapWrap/MapWrapI18n.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <sstream>

namespace tcx {
namespace mapwrap {

static bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

static bool looksLikePathReference(const std::string& value) {
    return value.find('/') != std::string::npos ||
           value.find('\\') != std::string::npos ||
           value.find('.') != std::string::npos;
}

// ===========================================================================
// Impl
// ===========================================================================

struct MapWrapDocument::Impl {
    std::string name = "default";
    Vec2 designCanvasSize = Vec2(1920, 1080);
    std::unique_ptr<MapWrapStage> stage;
    std::vector<std::shared_ptr<Surface>> surfaces;
    std::vector<std::shared_ptr<SurfaceGroup>> groups;
    std::vector<std::shared_ptr<MapWrapCue>> cues;
    bool dirty = false;

    // Per-type counters for readable surface IDs
    int quadCounter = 0;
    int gridCounter = 0;
    int bezierCounter = 0;
    int triangleCounter = 0;
    int circleCounter = 0;
    int polygonCounter = 0;

    // Cue counter
    int cueCounter = 0;

    void reserveSurfaceId(const SurfaceId& id) {
        auto reserve = [&](const std::string& prefix, int& counter) {
            if (id.rfind(prefix, 0) != 0) return;
            char* end = nullptr;
            long value = std::strtol(id.c_str() + prefix.size(), &end, 10);
            if (end && *end == '\0' && value > counter) {
                counter = static_cast<int>(value);
            }
        };

        reserve("surface_quad_", quadCounter);
        reserve("surface_grid_", gridCounter);
        reserve("surface_bezier_", bezierCounter);
        reserve("surface_triangle_", triangleCounter);
        reserve("surface_circle_", circleCounter);
        reserve("surface_polygon_", polygonCounter);
    }
};

// ===========================================================================
// Construction
// ===========================================================================

MapWrapDocument::MapWrapDocument()
    : impl_(std::make_unique<Impl>())
{
    impl_->stage = std::make_unique<MapWrapStage>();
}

MapWrapDocument::~MapWrapDocument() = default;

// ===========================================================================
// Stage / Output
// ===========================================================================

MapWrapStage& MapWrapDocument::stage() { return *impl_->stage; }
const MapWrapStage& MapWrapDocument::stage() const { return *impl_->stage; }

// ===========================================================================
// Canvas
// ===========================================================================

Vec2 MapWrapDocument::designCanvasSize() const { return impl_->designCanvasSize; }
void MapWrapDocument::setDesignCanvasSize(Vec2 size) {
    impl_->designCanvasSize = size;
    impl_->stage->setDesignCanvasSize(size);
    impl_->dirty = true;
}

void MapWrapDocument::clear() {
    impl_->name = "default";
    impl_->designCanvasSize = Vec2(1920, 1080);
    impl_->stage = std::make_unique<MapWrapStage>();
    impl_->surfaces.clear();
    impl_->groups.clear();
    impl_->cues.clear();
    impl_->quadCounter = 0;
    impl_->gridCounter = 0;
    impl_->bezierCounter = 0;
    impl_->triangleCounter = 0;
    impl_->circleCounter = 0;
    impl_->polygonCounter = 0;
    impl_->cueCounter = 0;
    impl_->dirty = true;
}

// ===========================================================================
// Surface creation
// ===========================================================================

std::shared_ptr<SurfaceQuad> MapWrapDocument::createQuadSurface(const std::string& name) {
    auto surface = std::make_shared<SurfaceQuad>();
    SurfaceId sid = "surface_quad_" + std::to_string(++impl_->quadCounter);
    surface->setId(sid);
    if (name.empty()) {
        surface->setName(tr("surface.quad") + " " + std::to_string(impl_->quadCounter));
    } else {
        surface->setName(name);
    }
    return surface;
}

std::shared_ptr<SurfaceGrid> MapWrapDocument::createGridSurface(int cols, int rows, const std::string& name) {
    auto surface = std::make_shared<SurfaceGrid>(cols, rows);
    SurfaceId sid = "surface_grid_" + std::to_string(++impl_->gridCounter);
    surface->setId(sid);
    if (name.empty()) {
        surface->setName(tr("surface.grid") + " " + std::to_string(impl_->gridCounter));
    } else {
        surface->setName(name);
    }
    return surface;
}

std::shared_ptr<SurfaceBezier> MapWrapDocument::createBezierSurface(int controlCols, int controlRows, const std::string& name) {
    auto surface = std::make_shared<SurfaceBezier>(controlCols, controlRows);
    SurfaceId sid = "surface_bezier_" + std::to_string(++impl_->bezierCounter);
    surface->setId(sid);
    if (name.empty()) {
        surface->setName(tr("surface.bezier") + " " + std::to_string(impl_->bezierCounter));
    } else {
        surface->setName(name);
    }
    return surface;
}

std::shared_ptr<SurfaceTriangle> MapWrapDocument::createTriangleSurface(const std::string& name) {
    auto surface = std::make_shared<SurfaceTriangle>();
    SurfaceId sid = "surface_triangle_" + std::to_string(++impl_->triangleCounter);
    surface->setId(sid);
    if (name.empty()) {
        surface->setName(tr("surface.triangle") + " " + std::to_string(impl_->triangleCounter));
    } else {
        surface->setName(name);
    }
    return surface;
}

std::shared_ptr<SurfaceCircle> MapWrapDocument::createCircleSurface(const std::string& name) {
    auto surface = std::make_shared<SurfaceCircle>();
    SurfaceId sid = "surface_circle_" + std::to_string(++impl_->circleCounter);
    surface->setId(sid);
    if (name.empty()) {
        surface->setName(tr("surface.circle") + " " + std::to_string(impl_->circleCounter));
    } else {
        surface->setName(name);
    }
    return surface;
}

std::shared_ptr<SurfacePolygon> MapWrapDocument::createPolygonSurface(const std::vector<Vec2>& points, const std::string& name) {
    auto surface = std::make_shared<SurfacePolygon>();
    SurfaceId sid = "surface_polygon_" + std::to_string(++impl_->polygonCounter);
    surface->setId(sid);
    if (name.empty()) {
        surface->setName(tr("surface.polygon") + " " + std::to_string(impl_->polygonCounter));
    } else {
        surface->setName(name);
    }
    // Initialize polygon points if provided
    if (!points.empty()) {
        for (const auto& pt : points) {
            surface->addPoint(pt);
        }
    }
    return surface;
}

// ===========================================================================
// Surface management
// ===========================================================================

void MapWrapDocument::addSurface(std::shared_ptr<Surface> surface) {
    if (surface) {
        if (getSurface(surface->id())) return;
        impl_->reserveSurfaceId(surface->id());
        impl_->surfaces.push_back(std::move(surface));
        impl_->dirty = true;
    }
}

void MapWrapDocument::removeSurface(const SurfaceId& id) {
    auto it = std::remove_if(impl_->surfaces.begin(), impl_->surfaces.end(),
        [&id](const std::shared_ptr<Surface>& s) { return s && s->id() == id; });
    if (it != impl_->surfaces.end()) {
        impl_->surfaces.erase(it, impl_->surfaces.end());
        for (auto& group : impl_->groups) {
            if (!group) continue;
            auto& refs = group->surfaceIds;
            refs.erase(std::remove(refs.begin(), refs.end(), id), refs.end());
        }
        impl_->dirty = true;
    }
}

void MapWrapDocument::insertSurface(std::shared_ptr<Surface> surface, int index) {
    if (!surface) return;
    if (getSurface(surface->id())) return;
    impl_->reserveSurfaceId(surface->id());
    int clamped = std::max(0, std::min(index, (int)impl_->surfaces.size()));
    impl_->surfaces.insert(impl_->surfaces.begin() + clamped, std::move(surface));
    impl_->dirty = true;
}

void MapWrapDocument::reorderSurface(const SurfaceId& id, int newIndex) {
    int oldIndex = surfaceIndex(id);
    if (oldIndex < 0) return;
    newIndex = std::max(0, std::min(newIndex, (int)impl_->surfaces.size() - 1));
    if (oldIndex == newIndex) return;

    auto surface = impl_->surfaces[oldIndex];
    impl_->surfaces.erase(impl_->surfaces.begin() + oldIndex);
    impl_->surfaces.insert(impl_->surfaces.begin() + newIndex, surface);
    impl_->dirty = true;
}

int MapWrapDocument::surfaceIndex(const SurfaceId& id) const {
    for (int i = 0; i < (int)impl_->surfaces.size(); ++i) {
        if (impl_->surfaces[i] && impl_->surfaces[i]->id() == id)
            return i;
    }
    return -1;
}

std::shared_ptr<Surface> MapWrapDocument::getSurface(const SurfaceId& id) {
    for (auto& surface : impl_->surfaces) {
        if (surface && surface->id() == id) {
            return surface;
        }
    }
    return nullptr;
}

std::shared_ptr<const Surface> MapWrapDocument::getSurface(const SurfaceId& id) const {
    for (const auto& surface : impl_->surfaces) {
        if (surface && surface->id() == id) {
            return surface;
        }
    }
    return nullptr;
}

const std::vector<std::shared_ptr<Surface>>& MapWrapDocument::surfaces() const {
    return impl_->surfaces;
}

// ===========================================================================
// Groups
// ===========================================================================

void MapWrapDocument::addGroup(std::shared_ptr<SurfaceGroup> group) {
    if (group) {
        impl_->groups.push_back(std::move(group));
        impl_->dirty = true;
    }
}

void MapWrapDocument::removeGroup(const GroupId& id) {
    auto it = std::remove_if(impl_->groups.begin(), impl_->groups.end(),
        [&id](const std::shared_ptr<SurfaceGroup>& g) { return g && g->id == id; });
    if (it != impl_->groups.end()) {
        impl_->groups.erase(it, impl_->groups.end());
        impl_->dirty = true;
    }
}

std::shared_ptr<SurfaceGroup> MapWrapDocument::getGroup(const GroupId& id) {
    for (auto& group : impl_->groups) {
        if (group && group->id == id) {
            return group;
        }
    }
    return nullptr;
}

const std::vector<std::shared_ptr<SurfaceGroup>>& MapWrapDocument::groups() const {
    return impl_->groups;
}

// ===========================================================================
// Cues
// ===========================================================================

CueId MapWrapDocument::createCueFromCurrentState(const std::string& name) {
    CueId cid = "cue_" + std::to_string(++impl_->cueCounter);
    auto cue = std::make_shared<MapWrapCue>();
    cue->id = cid;
    cue->name = name.empty()
        ? (tr("cue.name") + " " + std::to_string(impl_->cueCounter))
        : name;

    // Snapshot current surface states as patches
    for (const auto& surface : impl_->surfaces) {
        if (!surface) continue;
        SurfaceStatePatch patch;
        patch.surfaceId = surface->id();
        MapWrapJson patchJson = {
            {"name", surface->name()},
            {"visible", surface->isVisible()},
            {"locked", surface->isLocked()},
            {"opacity", surface->opacity()},
            {"source", surface->source()},
            {"sourceRect", {
                {"x", surface->sourceRect().x},
                {"y", surface->sourceRect().y},
                {"w", surface->sourceRect().w},
                {"h", surface->sourceRect().h}
            }}
        };
        patch.patchJson = patchJson.dump();
        cue->surfacePatches.push_back(std::move(patch));
    }

    // Snapshot current output states
    for (const auto& output : impl_->stage->outputs()) {
        OutputStatePatch patch;
        patch.outputId = output.id;
        MapWrapJson patchJson = {
            {"enabled", output.enabled},
            {"rotationDegrees", output.rotationDegrees},
            {"contentScale", output.contentScale}
        };
        patch.patchJson = patchJson.dump();
        cue->outputPatches.push_back(std::move(patch));
    }

    impl_->cues.push_back(std::move(cue));
    impl_->dirty = true;
    return cid;
}

Result MapWrapDocument::applyCue(const CueId& id) {
    // Find the cue
    std::shared_ptr<MapWrapCue> cue;
    for (auto& c : impl_->cues) {
        if (c && c->id == id) {
            cue = c;
            break;
        }
    }
    if (!cue) {
        return Result::error("Cue not found: " + id);
    }

    // Apply surface patches
    for (const auto& patch : cue->surfacePatches) {
        auto surface = getSurface(patch.surfaceId);
        if (!surface) continue;

        try {
            MapWrapJson json = MapWrapJson::parse(patch.patchJson);
            if (json.contains("name") && json["name"].is_string())
                surface->setName(json["name"].get<std::string>());
            if (json.contains("visible") && json["visible"].is_boolean())
                surface->setVisible(json["visible"].get<bool>());
            if (json.contains("locked") && json["locked"].is_boolean())
                surface->setLocked(json["locked"].get<bool>());
            if (json.contains("opacity") && json["opacity"].is_number())
                surface->setOpacity(json["opacity"].get<float>());
            if (json.contains("source") && json["source"].is_string())
                surface->setSource(json["source"].get<std::string>());
            if (json.contains("sourceRect") && json["sourceRect"].is_object()) {
                const auto& r = json["sourceRect"];
                Rect rect = surface->sourceRect();
                if (r.contains("x") && r["x"].is_number()) rect.x = r["x"].get<float>();
                if (r.contains("y") && r["y"].is_number()) rect.y = r["y"].get<float>();
                if (r.contains("w") && r["w"].is_number()) rect.w = r["w"].get<float>();
                if (r.contains("h") && r["h"].is_number()) rect.h = r["h"].get<float>();
                surface->setSourceRect(rect);
            }
        } catch (const MapWrapJson::exception&) {
            continue;
        }
    }

    // Apply output patches
    for (const auto& patch : cue->outputPatches) {
        auto* output = impl_->stage->getOutput(patch.outputId);
        if (!output) continue;

        try {
            MapWrapJson json = MapWrapJson::parse(patch.patchJson);
            if (json.contains("enabled") && json["enabled"].is_boolean())
                output->enabled = json["enabled"].get<bool>();
            if (json.contains("rotationDegrees") && json["rotationDegrees"].is_number())
                output->rotationDegrees = json["rotationDegrees"].get<float>();
            if (json.contains("contentScale") && json["contentScale"].is_number())
                output->contentScale = json["contentScale"].get<float>();
        } catch (const MapWrapJson::exception&) {
            continue;
        }
    }

    impl_->dirty = true;
    return Result::success();
}

const std::vector<std::shared_ptr<MapWrapCue>>& MapWrapDocument::cues() const {
    return impl_->cues;
}

void MapWrapDocument::addCue(std::shared_ptr<MapWrapCue> cue) {
    if (cue) {
        impl_->cues.push_back(std::move(cue));
        impl_->dirty = true;
    }
}

void MapWrapDocument::removeCue(const CueId& id) {
    auto it = std::remove_if(impl_->cues.begin(), impl_->cues.end(),
        [&id](const std::shared_ptr<MapWrapCue>& c) { return c && c->id == id; });
    if (it != impl_->cues.end()) {
        impl_->cues.erase(it, impl_->cues.end());
        impl_->dirty = true;
    }
}

// ===========================================================================
// Validation
// ===========================================================================

ProjectValidationReport MapWrapDocument::validateProject() const {
    ProjectValidationReport report;
    report.ok = true;

    // Collect all source IDs referenced by surfaces
    std::set<SourceId> referencedSources;
    for (const auto& surface : impl_->surfaces) {
        if (!surface) continue;

        const SourceId& srcId = surface->source();
        if (srcId.empty()) continue;  // no source assigned — not an error

        referencedSources.insert(srcId);

        // Check source geometry validity
        auto geom = surface->validateGeometry();
        if (!geom.valid) {
            if (geom.selfIntersecting) {
                report.warnings.push_back(
                    "Surface '" + surface->name() + "' (" + surface->id() + "): " +
                    tr("geometry.self_intersecting"));
            }
            if (geom.tooSmall) {
                report.warnings.push_back(
                    "Surface '" + surface->name() + "' (" + surface->id() + "): " +
                    tr("geometry.too_small"));
            }
            if (geom.windingFlipped) {
                report.warnings.push_back(
                    "Surface '" + surface->name() + "' (" + surface->id() + "): " +
                    tr("geometry.winding_flipped"));
            }
            if (geom.hasNaN) {
                report.warnings.push_back(
                    "Surface '" + surface->name() + "' (" + surface->id() + "): " +
                    tr("geometry.has_nan"));
            }
        }
    }

    // Check source references that can be validated without a SourceRegistry.
    // Registry-owned IDs should use the "src_" prefix. Path-like references are
    // legacy direct media sources and are missing at document scope until the
    // engine or packaging layer resolves them.
    for (const auto& sourceId : referencedSources) {
        if (looksLikePathReference(sourceId)) {
            report.missingSources.push_back(sourceId);
            continue;
        }

        if (!startsWith(sourceId, "src_")) {
            report.warnings.push_back("Source reference requires registry validation: " + sourceId);
        }
    }

    // Check surface group integrity — referenced surface IDs should exist
    for (const auto& group : impl_->groups) {
        if (!group) continue;
        for (const auto& sid : group->surfaceIds) {
            bool found = false;
            for (const auto& surface : impl_->surfaces) {
                if (surface && surface->id() == sid) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                report.warnings.push_back(
                    "Group '" + group->name + "' references unknown surface: " + sid);
            }
        }
    }

    // Check cue integrity — referenced surface IDs should exist
    for (const auto& cue : impl_->cues) {
        if (!cue) continue;
        for (const auto& patch : cue->surfacePatches) {
            bool found = false;
            for (const auto& surface : impl_->surfaces) {
                if (surface && surface->id() == patch.surfaceId) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                report.warnings.push_back(
                    "Cue '" + cue->name + "' references unknown surface: " + patch.surfaceId);
            }
        }
    }

    // Check outputs
    if (impl_->stage->outputs().empty()) {
        report.warnings.push_back("Stage has no outputs defined");
    }

    // If we have any missing sources or files, mark not ok
    if (!report.missingSources.empty() || !report.missingFiles.empty()) {
        report.ok = false;
    }

    return report;
}

// ===========================================================================
// Dirty flag
// ===========================================================================

bool MapWrapDocument::isDirty() const { return impl_->dirty; }
void MapWrapDocument::markDirty() { impl_->dirty = true; }
void MapWrapDocument::clearDirty() { impl_->dirty = false; }

// ===========================================================================
// Name
// ===========================================================================

const std::string& MapWrapDocument::name() const { return impl_->name; }
void MapWrapDocument::setName(const std::string& n) { impl_->name = n; }

} // namespace mapwrap
} // namespace tcx
