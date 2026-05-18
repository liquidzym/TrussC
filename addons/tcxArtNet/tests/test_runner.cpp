#include "test_common.h"

int main() {
    try {
        test_opcode();
        test_header();
        test_art_poll();
        test_art_poll_reply();
        test_art_dmx();
        test_art_nzs();
        test_art_sync();
        test_art_address();
        test_art_input();
        test_art_timecode();
        test_art_trigger();
        test_art_ip_prog();
        test_art_diag_data();
        test_art_command_data();
        test_packet_validation();
        test_universe_address();
        test_pixel_mapper();
        test_public_include();
        test_cross_platform_socket();
    } catch (const std::exception& ex) {
        std::cerr << "tcxArtNet_tests failed: " << ex.what() << "\n";
        return 1;
    }
    std::cout << "tcxArtNet_tests passed\n";
    return 0;
}
