#pragma once

#include <tcxArtNet.h>

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

inline void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
T roundTrip(const T& packet) {
    tcx::artnet::Packet encodedPacket { packet };
    std::vector<uint8_t> bytes;
    tcx::artnet::Error error;
    require(tcx::artnet::Codec::encode(encodedPacket, bytes, &error), error.message.c_str());
    tcx::artnet::Packet decodedPacket;
    require(tcx::artnet::Codec::decode(bytes, decodedPacket, &error), error.message.c_str());
    require(std::holds_alternative<T>(decodedPacket), "decoded packet has the expected type");
    return std::get<T>(decodedPacket);
}

void test_opcode();
void test_header();
void test_art_poll();
void test_art_poll_reply();
void test_art_dmx();
void test_art_nzs();
void test_art_sync();
void test_art_address();
void test_art_input();
void test_art_timecode();
void test_art_trigger();
void test_art_ip_prog();
void test_art_diag_data();
void test_art_command_data();
void test_packet_validation();
void test_protocol_fixtures();
void test_node_controller_runtime();
void test_feature_extensions();
void test_universe_address();
void test_pixel_mapper();
void test_public_include();
void test_cross_platform_socket();
void test_network_diagnostics();
