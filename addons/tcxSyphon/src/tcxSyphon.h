#pragma once

// tcxSyphon - Syphon GPU texture sharing for TrussC (macOS only).
//
//   tcx::SyphonServer  - publish a Texture / Fbo to other apps
//   tcx::SyphonClient  - receive a published texture (+ static listServers())
//
// Syphon is macOS-only and always shares 8-bit BGRA frames. On other
// platforms these classes compile but their operations are no-ops.

#include "tcxSyphonServer.h"
#include "tcxSyphonClient.h"
