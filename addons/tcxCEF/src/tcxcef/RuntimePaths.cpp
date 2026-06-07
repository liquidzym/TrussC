#include "tcxcef/RuntimePaths.h"

namespace tcxCEF {

std::filesystem::path addonRoot() {
#ifdef TCXCEF_ADDON_ROOT
    return std::filesystem::path(TCXCEF_ADDON_ROOT);
#else
    return {};
#endif
}

bool isCefAvailable() {
#if TCXCEF_HAS_CEF
    return true;
#else
    return false;
#endif
}

CefRuntimePaths runtimePaths() {
    CefRuntimePaths paths;
    paths.available = isCefAvailable();
    paths.addonRoot = addonRoot();
#if TCXCEF_HAS_CEF
#ifdef TCXCEF_PLATFORM_NAME
    paths.platform = TCXCEF_PLATFORM_NAME;
#endif
#ifdef TCXCEF_VERSION_STRING
    paths.version = TCXCEF_VERSION_STRING;
#endif
#ifdef TCXCEF_ROOT_PATH
    paths.cefRoot = TCXCEF_ROOT_PATH;
#endif
#ifdef TCXCEF_INCLUDE_PATH
    paths.includeDir = TCXCEF_INCLUDE_PATH;
#endif
#ifdef TCXCEF_RELEASE_PATH
    paths.releaseDir = TCXCEF_RELEASE_PATH;
#endif
#ifdef TCXCEF_RESOURCE_PATH
    paths.resourceDir = TCXCEF_RESOURCE_PATH;
#endif
#ifdef TCXCEF_LIBCEF_PATH
    paths.libcefLibrary = TCXCEF_LIBCEF_PATH;
#endif
#ifdef TCXCEF_FRAMEWORK_PATH
    paths.cefFrameworkPath = TCXCEF_FRAMEWORK_PATH;
#endif
#ifdef TCXCEF_WRAPPER_PATH
    paths.wrapperLibrary = TCXCEF_WRAPPER_PATH;
#endif
#endif
    return paths;
}

} // namespace tcxCEF
