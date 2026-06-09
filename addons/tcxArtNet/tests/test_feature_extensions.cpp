#include "test_common.h"

#include <optional>

namespace {

using namespace tcx::artnet;

void sendPacketBytes(UdpSocket& socket, const Endpoint& endpoint, const Packet& packet, Error& error) {
    std::vector<uint8_t> bytes;
    require(Codec::encode(packet, bytes, &error), error.message.c_str());
    require(socket.sendTo(bytes, endpoint, &error), error.message.c_str());
}

std::optional<Packet> receivePacket(UdpSocket& socket, int attempts = 50) {
    std::array<uint8_t, 2048> buffer {};
    for (int i = 0; i < attempts; ++i) {
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

void testSenderStateApi() {
    Error error;
    UdpSocket receiver;
    require(receiver.open(&error), error.message.c_str());
    require(receiver.setReuseAddress(true, &error), error.message.c_str());
    require(receiver.bind(64634, &error), error.message.c_str());
    require(receiver.setNonBlocking(true, &error), error.message.c_str());

    Sender sender;
    require(sender.setup(false, &error), error.message.c_str());
    sender.setDestination({ "127.0.0.1", 64634 });

    require(sender.setChannel(3, 0, 11, &error), error.message.c_str());
    require(sender.setColor(3, 1, trussc::Color(0.5f, 0.25f, 1.0f), &error), error.message.c_str());
    require(sender.setChannel(3, 511, 22, &error), error.message.c_str());
    require(sender.getUniverseCount() == 1, "Sender tracks one active universe");
    require(sender.getChannel(3, 1, &error) == 128, "Sender color helper writes red to zero-based channel 1");
    require(sender.send(&error), error.message.c_str());

    auto packet = receivePacket(receiver);
    require(packet.has_value(), "Sender state API emits ArtDmx");
    require(std::holds_alternative<ArtDmx>(*packet), "Sender state API sends ArtDmx packets");
    const auto& firstDmx = std::get<ArtDmx>(*packet);
    require(firstDmx.universe.toPortAddress() == 3, "Sender state API maps flat universe numbers to Art-Net port addresses");
    require(firstDmx.data.size() == MaxDmxChannels, "Sender state API sends full DMX universes");
    require(firstDmx.data[0] == 11, "Sender setChannel writes zero-based channel 0");
    require(firstDmx.data[1] == 128 && firstDmx.data[2] == 64 && firstDmx.data[3] == 255, "Sender setColor writes RGB bytes");
    require(firstDmx.data[511] == 22, "Sender setChannel writes zero-based channel 511");

    require(!sender.setChannel(3, 512, 1, &error), "Sender rejects out-of-range zero-based channels");
    require(error.code == ErrorCode::InvalidLength, "Sender reports InvalidLength for channel 512");
    require(sender.clear(3, &error), error.message.c_str());
    require(sender.send(&error), error.message.c_str());
    packet = receivePacket(receiver);
    require(packet.has_value(), "Sender clear keeps universe active");
    const auto& cleared = std::get<ArtDmx>(*packet);
    require(cleared.data.size() == MaxDmxChannels, "Sender clear keeps full DMX universe length");
    for (uint8_t value : cleared.data) {
        require(value == 0, "Sender clear blacks out every channel");
    }

    require(sender.removeUniverse(3, &error), error.message.c_str());
    require(sender.getUniverseCount() == 0, "Sender removeUniverse drops the active universe");
    require(sender.send(&error), error.message.c_str());
    require(!receivePacket(receiver, 5).has_value(), "Sender send emits nothing after all universes are removed");
}

void testSenderAutoSend() {
    Error error;
    UdpSocket receiver;
    require(receiver.open(&error), error.message.c_str());
    require(receiver.setReuseAddress(true, &error), error.message.c_str());
    require(receiver.bind(64635, &error), error.message.c_str());
    require(receiver.setNonBlocking(true, &error), error.message.c_str());

    Sender sender;
    require(sender.setup(false, &error), error.message.c_str());
    sender.setDestination({ "127.0.0.1", 64635 });
    require(sender.setChannel(4, 0, 77, &error), error.message.c_str());
    require(sender.startAutoSend(20.0, &error), error.message.c_str());
    require(sender.isAutoSending(), "Sender reports active auto-send thread");

    int dmxPackets = 0;
    for (int i = 0; i < 120 && dmxPackets < 2; ++i) {
        auto packet = receivePacket(receiver, 1);
        if (packet && std::holds_alternative<ArtDmx>(*packet)) {
            const auto& dmx = std::get<ArtDmx>(*packet);
            if (dmx.universe.toPortAddress() == 4 && dmx.data.size() == MaxDmxChannels && dmx.data[0] == 77) {
                ++dmxPackets;
            }
        }
    }
    sender.stopAutoSend();
    require(!sender.isAutoSending(), "Sender stops auto-send thread");
    require(dmxPackets >= 2, "Sender auto-send refreshes unchanged DMX state");
}

void testReceiverStateApi() {
    Error error;
    Receiver receiver;
    require(receiver.setup(64636, &error), error.message.c_str());
    require(!receiver.hasNewData(), "Receiver starts with no unread DMX changes");

    UdpSocket sender;
    require(sender.open(&error), error.message.c_str());
    ArtDmx dmx;
    dmx.universe = { 0, 0, 5 };
    dmx.data = { 9, 8, 7, 6 };
    sendPacketBytes(sender, { "127.0.0.1", 64636 }, Packet { dmx }, error);

    for (int i = 0; i < 50; ++i) {
        receiver.poll(&error);
        if (receiver.hasUniverse(5)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    require(receiver.hasUniverse(5), "Receiver records incoming DMX universe state");
    require(receiver.hasNewData(), "Receiver hasNewData reports unread DMX updates");
    require(!receiver.hasNewData(), "Receiver hasNewData clears on read");
    require(receiver.getChannel(5, 0, &error) == 9, "Receiver getChannel reads zero-based channel 0");
    require(receiver.getChannel(5, 3, &error) == 6, "Receiver getChannel reads partial ArtDmx payload");
    require(receiver.getChannel(5, 4, &error) == 0, "Receiver getChannel returns zero beyond a short ArtDmx payload");
    const auto dmxData = receiver.getDmx(5);
    require(dmxData.size() == MaxDmxChannels, "Receiver getDmx expands latest data to a full universe");
    require(dmxData[0] == 9 && dmxData[1] == 8 && dmxData[2] == 7 && dmxData[3] == 6, "Receiver getDmx returns latest channel values");
}

void testSessionFacade() {
    Error error;
    UdpSocket dmxSink;
    require(dmxSink.open(&error), error.message.c_str());
    require(dmxSink.setReuseAddress(true, &error), error.message.c_str());
    require(dmxSink.bind(64639, &error), error.message.c_str());
    require(dmxSink.setNonBlocking(true, &error), error.message.c_str());

    SessionSettings settings;
    settings.sender.enableBroadcast = false;
    settings.receiver.port = 64637;
    settings.controller.localPort = 64638;
    settings.controller.directedBroadcastIp = "127.0.0.1";
    settings.controller.autoPoll = false;

    Session session;
    require(session.setup(settings, &error), error.message.c_str());

    session.sender().setDestination({ "127.0.0.1", 64639 });
    require(session.sender().setChannel(7, 0, 55, &error), error.message.c_str());
    require(session.sender().send(&error), error.message.c_str());

    auto packet = receivePacket(dmxSink);
    require(packet.has_value(), "Session sender accessor emits DMX through the managed Sender");
    require(std::holds_alternative<ArtDmx>(*packet), "Session sender sends ArtDmx");
    require(std::get<ArtDmx>(*packet).universe.toPortAddress() == 7, "Session sender preserves flat universe address");
    require(std::get<ArtDmx>(*packet).data[0] == 55, "Session sender preserves channel state");

    UdpSocket peer;
    require(peer.open(&error), error.message.c_str());

    ArtDmx incoming;
    incoming.universe = { 0, 0, 8 };
    incoming.data = { 12, 34 };
    sendPacketBytes(peer, { "127.0.0.1", 64637 }, Packet { incoming }, error);

    ArtPollReply reply;
    reply.ipAddress = { 2, 0, 0, 44 };
    reply.bindIp = reply.ipAddress;
    reply.bindIndex = 1;
    reply.shortName = "session-node";
    sendPacketBytes(peer, { "127.0.0.1", 64638 }, Packet { reply }, error);

    for (int i = 0; i < 50; ++i) {
        session.update();
        if (session.receiver().hasUniverse(8) && session.getDiscoveredNodes().size() == 1) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    require(session.receiver().hasUniverse(8), "Session update polls the managed Receiver");
    require(session.receiver().getChannel(8, 1, &error) == 34, "Session receiver accessor exposes latest DMX");
    const auto nodes = session.getDiscoveredNodes();
    require(nodes.size() == 1, "Session update polls the managed Controller");
    require(nodes[0].reply.shortName == "session-node", "Session exposes discovered nodes through the controller workflow");

    session.close();
    Error closedError;
    require(!session.sender().sendSync({ "127.0.0.1", 64639 }, &closedError), "Session close closes the managed Sender");
    require(closedError.code == ErrorCode::NotOpen, "Session close leaves Sender in NotOpen state");
}

} // namespace

void test_feature_extensions() {
    using namespace tcx::artnet;

    testSenderStateApi();
    testSenderAutoSend();
    testReceiverStateApi();
    testSessionFacade();

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
