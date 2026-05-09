#pragma once
// Denormal prevention — add tiny DC offset to flush denormals to zero
namespace tcx::pdsp { inline float denormalFix(float x){ return x + 1e-20f - 1e-20f; } }
