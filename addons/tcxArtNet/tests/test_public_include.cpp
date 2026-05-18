#include <tcxArtNet.h>

#include "test_common.h"

void test_public_include() {
    tcx::artnet::UniverseAddress address { 0, 0, 1 };
    require(address.isValid(), "tcxArtNet umbrella header exposes public API");
}
