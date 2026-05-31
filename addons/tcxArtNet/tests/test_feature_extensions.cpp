#include "test_common.h"

#include <optional>

namespace {

using namespace tcx::artnet;

void sendPacketBytes(UdpSocket& socket, const Endpoint& endpoint, const Packet& packet, Error& error) {
    std::vector<uint8_t> bytes;
    require(Codec::encode(packet, bytes, &error), error.message.c_str());
    require(socket.sendTo(bytes, endpoint, &error), error.message.c_str());
}

std::optional<Packet> receivePacket(UdpSocket& socket) {
    std::array<uint8_t, 2048> buffer {};
    for (int i = 0; i < 50; ++i) {
        size_t received = 0;
        Endpoint sender;
        Error error;
        if (socket.receiveFrom(buffer, received, sender, &error)) {
            Packet packet;
            require(Codec::decode(std::span<const uint8_t>(buffer.data(), received), packet, &error), error.message.c_str());
            return packet;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return std::nullopt;
}

} // namespace

void test_feature_extensions() {
    using namespace tcx::artnet;

    {
        Error error;
        Controller controller;
        ControllerSettings settings;
        settings.localPort = 64628;
        settings.pollTimeout = std::chrono::milliseconds(20);
        require(controller.setup(settings, &error), error.message.c_str());

        UdpSocket sender;
        require(sender.open(&error), error.message.c_str());
        ArtPollReply reply;
        reply.ipAddress = { 2, 0, 0, 10 };
        reply.bindIp = reply.ipAddress;
        reply.bindIndex = 1;
        sendPacketBytes(sender, { "127.0.0.1", 64628 }, Packet { reply }, error);
        for (int i = 0; i < 50; ++i) {
            controller.update();
            if (controller.getDiscoveredNodes().size() == 1) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(controller.getDiscoveredNodes().size() == 1, "Controller records discovered nodes");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        controller.update();
        require(controller.getDiscoveredNodes().empty(), "Controller prunes stale discovered nodes using pollTimeout");
    }

    {
        Error error;
        Node node;
        NodeSettings settings;
        settings.port = 64629;
        settings.ipAddress = "2.0.0.20";
        settings.bindIpAddress = "2.0.0.20";
        settings.enableIpProg = true;
        require(node.setup(settings, &error), error.message.c_str());
        bool sawAddress = false;
        node.setAddressCallback([&](const ArtAddress& address) {
            sawAddress = address.shortName == "renamed";
        });
        bool sawInput = false;
        node.setInputCallback([&](const ArtInput& input) {
            sawInput = input.bindIndex == 2 && input.input[0] == 1;
        });

        UdpSocket controller;
        require(controller.open(&error), error.message.c_str());
        require(controller.setReuseAddress(true, &error), error.message.c_str());
        require(controller.bind(64630, &error), error.message.c_str());
        require(controller.setNonBlocking(true, &error), error.message.c_str());

        ArtAddress address;
        address.shortName = "renamed";
        sendPacketBytes(controller, { "127.0.0.1", 64629 }, Packet { address }, error);
        for (int i = 0; i < 50 && !sawAddress; ++i) {
            node.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(sawAddress, "Node emulator dispatches ArtAddress when enabled");
        require(node.sendPollReply({ "127.0.0.1", 64630 }, &error), error.message.c_str());
        auto maybeReply = receivePacket(controller);
        require(maybeReply.has_value(), "Node replies after ArtAddress update");
        require(std::get<ArtPollReply>(*maybeReply).shortName == "renamed", "Node emulator applies ArtAddress short name");

        ArtIpProg ipProg;
        ipProg.command = 0x97;
        ipProg.ip = { 2, 0, 0, 21 };
        ipProg.subnetMask = { 255, 0, 0, 0 };
        ipProg.portAddress = 0x2345;
        ipProg.defaultGateway = { 2, 0, 0, 1 };
        sendPacketBytes(controller, { "127.0.0.1", 64629 }, Packet { ipProg }, error);
        std::optional<Packet> maybeIpReply;
        for (int i = 0; i < 50; ++i) {
            node.update();
            maybeIpReply = receivePacket(controller);
            if (maybeIpReply && std::holds_alternative<ArtIpProgReply>(*maybeIpReply)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(maybeIpReply.has_value(), "Node sends ArtIpProgReply for virtual IP programming");
        require(std::holds_alternative<ArtIpProgReply>(*maybeIpReply), "Node response decodes as ArtIpProgReply");
        const auto& ipReply = std::get<ArtIpProgReply>(*maybeIpReply);
        require(ipReply.ip == ipProg.ip, "Node ArtIpProgReply reports programmed IP");
        require(ipReply.portAddress == 0x2345, "Node ArtIpProgReply reports programmed port address");

        ArtInput input;
        input.bindIndex = 2;
        input.numPorts = 1;
        input.input[0] = 1;
        sendPacketBytes(controller, { "127.0.0.1", 64629 }, Packet { input }, error);
        for (int i = 0; i < 50 && !sawInput; ++i) {
            node.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(sawInput, "Node emulator dispatches ArtInput when enabled");
    }

    {
        DmxReceiverState state;
        ArtDmx dmx;
        dmx.universe = { 0, 0, 1 };
        dmx.sequence = 1;
        dmx.data = { 10, 20 };
        require(state.processDmx(dmx, true), "DMX runtime buffers synced frame");
        require(!state.getUniverseData(dmx.universe).has_value(), "synced DMX frame is not committed before ArtSync");
        require(state.processSync(ArtSync {}) == 1, "ArtSync commits one pending universe");
        require(state.getUniverseData(dmx.universe).value() == dmx.data, "DMX runtime returns committed universe data");
        require(!state.processDmx(dmx, false), "DMX runtime drops duplicate non-zero sequence");

        DmxReceiverState staleState;
        staleState.setPendingTimeout(std::chrono::milliseconds(1));
        require(staleState.processDmx(dmx, true), "DMX runtime buffers synced frame with timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        require(staleState.processSync(ArtSync {}) == 0, "ArtSync does not commit expired pending universe");
        require(!staleState.getUniverseData(dmx.universe).has_value(), "expired pending universe is not visible as committed data");
    }

    {
        std::vector<uint8_t> pixels = { 100, 60, 20, 0 };
        PixelToDmxOptions options;
        options.format = PixelFormat::RGBW;
        options.extractWhite = true;
        options.brightness = 0.5f;
        options.gamma = 1.0f;
        std::vector<ArtDmx> frames;
        Error error;
        require(PixelMapper::splitPixelsToUniverses(pixels, { 0, 0, 1 }, options, frames, &error), error.message.c_str());
        require((frames[0].data == std::vector<uint8_t> { 40, 20, 0, 10 }), "PixelMapper supports RGBW white extraction plus brightness");
    }

    {
        ArtDmx dmx;
        dmx.universe = { 1, 2, 3 };
        dmx.data = { 1, 2 };
        std::vector<uint8_t> bytes;
        Error error;
        require(Codec::encode(Packet { dmx }, bytes, &error), error.message.c_str());
        const std::string summary = PacketInspector::summarize(Packet { dmx });
        require(summary.find("ArtDmx") != std::string::npos, "PacketInspector summarizes packet type");
        require(summary.find("1/2/3") != std::string::npos, "PacketInspector summarizes universe address");
        require(PacketInspector::bytesToHex(bytes).find("41 72 74 2d 4e 65 74 00") != std::string::npos, "PacketInspector produces hex dumps");
        require(PacketInspector::decodeHexBytes("41 72 74 2d").size() == 4, "PacketInspector decodes hex byte strings");
    }
}
