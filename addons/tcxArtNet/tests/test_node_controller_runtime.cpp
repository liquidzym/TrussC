#include "test_common.h"

#include <optional>

namespace {

using namespace tcx::artnet;

bool receiveDecoded(UdpSocket& socket, Packet& packet, Endpoint& sender, Error* error) {
    std::array<uint8_t, 2048> buffer {};
    for (int i = 0; i < 50; ++i) {
        size_t received = 0;
        Error receiveError;
        if (socket.receiveFrom(buffer, received, sender, &receiveError)) {
            return Codec::decode(std::span<const uint8_t>(buffer.data(), received), packet, error);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    setError(error, ErrorCode::ReceiveFailed, "timed out waiting for UDP packet");
    return false;
}

} // namespace

void test_node_controller_runtime() {
    using namespace tcx::artnet;

    {
        UdpSocket receiver;
        Error error;
        require(receiver.open(&error), error.message.c_str());
        require(receiver.setReuseAddress(true, &error), error.message.c_str());
        require(receiver.bind(64621, &error), error.message.c_str());
        require(receiver.setNonBlocking(true, &error), error.message.c_str());

        Node node;
        NodeSettings settings;
        settings.port = 64622;
        settings.ipAddress = "2.1.2.3";
        settings.bindIpAddress = "2.1.2.3";
        settings.outputPorts.push_back({ UniverseAddress { 2, 3, 4 }, false, true, true });
        require(node.setup(settings, &error), error.message.c_str());
        require(node.sendPollReply({ "127.0.0.1", 64621 }, &error), error.message.c_str());

        Packet packet;
        Endpoint sender;
        require(receiveDecoded(receiver, packet, sender, &error), error.message.c_str());
        require(std::holds_alternative<ArtPollReply>(packet), "node sends ArtPollReply");
        const auto& reply = std::get<ArtPollReply>(packet);
        require(reply.ipAddress == std::array<uint8_t, 4> { 2, 1, 2, 3 }, "PollReply advertises configured node IP");
        require(reply.bindIp == std::array<uint8_t, 4> { 2, 1, 2, 3 }, "PollReply advertises configured bind IP");
        require(reply.netSwitch == 2 && reply.subSwitch == 3 && reply.swOut[0] == 4, "PollReply exports full output port address");
    }

    {
        Error error;
        Controller controller;
        ControllerSettings settings;
        settings.localPort = 64623;
        require(controller.setup(settings, &error), error.message.c_str());

        UdpSocket sender;
        require(sender.open(&error), error.message.c_str());
        require(sender.setReuseAddress(true, &error), error.message.c_str());
        require(sender.bind(64624, &error), error.message.c_str());

        ArtPollReply first;
        first.ipAddress = { 2, 0, 0, 1 };
        first.bindIp = first.ipAddress;
        first.bindIndex = 1;
        first.shortName = "node-1";
        std::vector<uint8_t> bytes;
        require(Codec::encode(Packet { first }, bytes, &error), error.message.c_str());
        require(sender.sendTo(bytes, { "127.0.0.1", 64623 }, &error), error.message.c_str());

        ArtPollReply second = first;
        second.bindIndex = 2;
        second.shortName = "node-2";
        require(Codec::encode(Packet { second }, bytes, &error), error.message.c_str());
        require(sender.sendTo(bytes, { "127.0.0.1", 64623 }, &error), error.message.c_str());

        for (int i = 0; i < 20; ++i) {
            controller.update();
            if (controller.getDiscoveredNodes().size() == 2) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const auto nodes = controller.getDiscoveredNodes();
        require(nodes.size() == 2, "Controller keeps separate discovered entries for different BindIndex values");
    }

    {
        Error error;
        Receiver receiver;
        std::optional<Packet> receivedPacket;
        receiver.setPacketCallback([&](const Packet& packet, const Endpoint&) {
            receivedPacket = packet;
        });
        require(receiver.setup(64625, &error), error.message.c_str());

        Sender sender;
        require(sender.setup(false, &error), error.message.c_str());
        ArtDataReply reply;
        reply.data.assign(1400, 0xab);
        require(sender.sendPacket({ "127.0.0.1", 64625 }, Packet { reply }, &error), error.message.c_str());

        for (int i = 0; i < 50 && !receivedPacket; ++i) {
            receiver.poll(&error);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(receivedPacket.has_value(), "Receiver accepts a codec-only packet larger than 1024 bytes");
        require(std::holds_alternative<ArtDataReply>(*receivedPacket), "large packet decodes as ArtDataReply");
        require(std::get<ArtDataReply>(*receivedPacket).data.size() == 1400, "large packet payload is not truncated by UDP receive buffer");
    }

    {
        Error error;
        UdpSocket receiver;
        require(receiver.open(&error), error.message.c_str());
        require(receiver.setReuseAddress(true, &error), error.message.c_str());
        require(receiver.bind(64631, &error), error.message.c_str());
        require(receiver.setNonBlocking(true, &error), error.message.c_str());

        Sender sender;
        require(sender.setup(false, &error), error.message.c_str());
        const UniverseAddress universe { 0, 0, 1 };
        const std::array<uint8_t, 2> dmx { 1, 2 };
        require(sender.sendDmx({ "127.0.0.1", 64631 }, universe, dmx, &error), error.message.c_str());
        require(sender.sendDmx({ "127.0.0.1", 64631 }, universe, dmx, &error), error.message.c_str());

        Packet first;
        Packet second;
        Endpoint endpoint;
        require(receiveDecoded(receiver, first, endpoint, &error), error.message.c_str());
        require(receiveDecoded(receiver, second, endpoint, &error), error.message.c_str());
        require(std::get<ArtDmx>(first).sequence == 1, "Sender assigns first ArtDmx sequence");
        require(std::get<ArtDmx>(second).sequence == 2, "Sender increments ArtDmx sequence");

        sender.close();
        Error sendAfterClose;
        require(!sender.sendSync({ "127.0.0.1", 64631 }, &sendAfterClose), "Sender does not silently recover after close");
        require(sendAfterClose.code == ErrorCode::NotOpen, "closed Sender reports NotOpen");

        require(sender.recover(&error), error.message.c_str());
        require(sender.sendSync({ "127.0.0.1", 64631 }, &error), error.message.c_str());
        Packet sync;
        require(receiveDecoded(receiver, sync, endpoint, &error), error.message.c_str());
        require(std::holds_alternative<ArtSync>(sync), "Sender recover explicitly reopens socket for sends");
    }

    {
        Error error;
        UdpSocket receiver;
        require(receiver.open(&error), error.message.c_str());
        require(receiver.setReuseAddress(true, &error), error.message.c_str());
        require(receiver.bind(64632, &error), error.message.c_str());
        require(receiver.setNonBlocking(true, &error), error.message.c_str());

        Controller controller;
        ControllerSettings settings;
        settings.localPort = 64633;
        require(controller.setup(settings, &error), error.message.c_str());
        const UniverseAddress universe { 0, 0, 2 };
        const std::array<uint8_t, 2> dmx { 3, 4 };
        require(controller.sendDmx({ "127.0.0.1", 64632 }, universe, dmx, &error), error.message.c_str());
        require(controller.sendDmx({ "127.0.0.1", 64632 }, universe, dmx, &error), error.message.c_str());

        Packet first;
        Packet second;
        Endpoint endpoint;
        require(receiveDecoded(receiver, first, endpoint, &error), error.message.c_str());
        require(receiveDecoded(receiver, second, endpoint, &error), error.message.c_str());
        require(std::get<ArtDmx>(first).sequence == 1, "Controller assigns first ArtDmx sequence");
        require(std::get<ArtDmx>(second).sequence == 2, "Controller increments ArtDmx sequence");
    }
}
