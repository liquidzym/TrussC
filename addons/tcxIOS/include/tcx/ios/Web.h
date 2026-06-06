#pragma once

#include "Types.h"

#include <string>

namespace tcx::ios {

struct SafariRequest {
    std::string url;
    bool entersReaderIfAvailable = false;
    bool barCollapsingEnabled = true;
};

class Web {
public:
    void openSafari(const SafariRequest& request, Completion<void> done);
};

Web& web();

} // namespace tcx::ios
