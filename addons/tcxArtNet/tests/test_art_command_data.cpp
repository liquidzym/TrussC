#include "test_common.h"

void test_art_command_data() {
    tcx::artnet::ArtCommand command;
    command.estaManufacturerCode = 0x1111;
    command.command = "reset";
    auto decodedCommand = roundTrip(command);
    require(decodedCommand.estaManufacturerCode == 0x1111, "ArtCommand ESTA survives round trip");
    require(decodedCommand.command == "reset", "ArtCommand text survives round trip");

    tcx::artnet::ArtDataRequest request;
    request.estaManufacturerCode = 0x2222;
    request.oemCode = 0x3333;
    request.requestCode = 4;
    auto decodedRequest = roundTrip(request);
    require(decodedRequest.oemCode == 0x3333 && decodedRequest.requestCode == 4, "ArtDataRequest survives round trip");

    tcx::artnet::ArtDataReply reply;
    reply.estaManufacturerCode = 0x2222;
    reply.oemCode = 0x3333;
    reply.requestCode = 4;
    reply.data = { 9, 8, 7 };
    auto decodedReply = roundTrip(reply);
    require(decodedReply.data == reply.data, "ArtDataReply payload survives round trip");
}
