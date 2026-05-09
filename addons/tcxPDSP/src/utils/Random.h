#pragma once
// Fast random number generator (Xorshift32) — audio-thread safe, no global state
#include <cstdint>
namespace tcx::pdsp {
struct Random { uint32_t state=12345; float next(){state^=state<<13;state^=state>>17;state^=state<<5;return(float)(state>>8)/16777216.0f;} };
}
