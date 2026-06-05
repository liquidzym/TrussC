#pragma once
// =============================================================================
// tcxMapWrap — TrussC Projection Mapping Addon
// =============================================================================
// Main umbrella header. Include this to access the full addon API.
//
// Platform targets: macOS / iOS / Windows only.
// Not supported: Linux / Web / Raspberry Pi / Android
//
// Namespace: tcx::mapwrap
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapI18n.h"
#include "tcxMapWrap/MapWrapEngine.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapStage.h"
#include "tcxMapWrap/MapWrapOutput.h"
#include "tcxMapWrap/MapWrapRenderer.h"
#include "tcxMapWrap/MapWrapEditor.h"
#include "tcxMapWrap/MapWrapInput.h"
#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/MapWrapMask.h"
#include "tcxMapWrap/MapWrapAutosave.h"
#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/SurfaceTriangle.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/SurfaceGroup.h"
#include "tcxMapWrap/SurfacePreset.h"
#include "tcxMapWrap/Warp.h"
#include "tcxMapWrap/WarpPerspective.h"
#include "tcxMapWrap/WarpGrid.h"
#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/SourceRegistry.h"
#include "tcxMapWrap/SourceTexture.h"
#include "tcxMapWrap/SourceFbo.h"
#include "tcxMapWrap/SourceVideo.h"
#include "tcxMapWrap/SourceImage.h"
#include "tcxMapWrap/SourceGenerated.h"
#include "tcxMapWrap/SourceClock.h"
#include "tcxMapWrap/CalibrationPatterns.h"
#include "tcxMapWrap/ColorCorrection.h"
#include "tcxMapWrap/BlendMode.h"
#include "tcxMapWrap/EditorViewport.h"
#include "tcxMapWrap/EditableProperty.h"
#include "tcxMapWrap/ProjectPackaging.h"
#include "tcxMapWrap/GeometryValidation.h"
#include "tcxMapWrap/HitTestIndex.h"
#include "tcxMapWrap/MapWrapCue.h"
#include "tcxMapWrap/UndoStack.h"
