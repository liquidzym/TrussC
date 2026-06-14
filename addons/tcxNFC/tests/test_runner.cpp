#include "test_common.h"

#include <iostream>

int main() {
    try {
        test_ndef_https_uri_tlv();
        test_bks710i_frames();
        test_activation_runtime();
    } catch (const std::exception& ex) {
        std::cerr << "tcxNFC_tests failed: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "tcxNFC_tests passed\n";
    return 0;
}
