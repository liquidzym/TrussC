#include "test_common.h"

void test_universe_address() {
    using namespace tcx::artnet;
    UniverseAddress address { 2, 3, 4 };
    require(address.isValid(), "valid universe address is accepted");
    require(address.toPortAddress() == 0x0234, "universe encodes to 15-bit port-address");
    auto decoded = UniverseAddress::fromPortAddress(0x0234);
    require(decoded.has_value(), "port-address decodes");
    require(decoded->net == 2 && decoded->subnet == 3 && decoded->universe == 4, "port-address fields decode correctly");
    auto next = UniverseAddress::next(UniverseAddress { 0, 0, 15 });
    require(next && next->subnet == 1 && next->universe == 0, "universe increments into subnet");
    require(!UniverseAddress::fromPortAddress(32768), "port-address over 15 bits is rejected");
}
