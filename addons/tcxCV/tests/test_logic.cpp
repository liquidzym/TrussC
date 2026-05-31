#include "tcxCvDistance.h"
#include "tcxCvRunningBackground.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

float backgroundValue(tcx::RunningBackground& background) {
    return background.getBackground().at<float>(0, 0);
}

void test_running_background_learning_time_uses_ema_alpha() {
    tcx::RunningBackground background;
    background.setLearningTime(4.0);

    cv::Mat thresholded;
    cv::Mat first(1, 1, CV_32FC1, cv::Scalar(100.0f));
    cv::Mat second(1, 1, CV_32FC1, cv::Scalar(200.0f));

    background.update(first, thresholded);
    require(std::fabs(backgroundValue(background) - 100.0f) < 0.001f,
            "learning-time update preserves the first background frame");

    background.update(second, thresholded);
    require(std::fabs(backgroundValue(background) - 125.0f) < 0.001f,
            "learning time 4 applies EMA alpha 0.25 to the new frame");
}

void test_most_representative_does_not_collapse_to_first_string() {
    const std::vector<std::string> strings {
        "zzzz",
        "aaaa",
        "aaab",
        "aaab",
        "aaac",
    };

    require(tcx::mostRepresentative(strings) == "aaaa",
            "mostRepresentative selects the minimum total edit distance string");
}

} // namespace

int main() {
    try {
        test_most_representative_does_not_collapse_to_first_string();
        test_running_background_learning_time_uses_ema_alpha();
    } catch (const std::exception& error) {
        std::cerr << "tcxCV_tests failed: " << error.what() << "\n";
        return 1;
    }

    std::cout << "tcxCV_tests passed\n";
    return 0;
}
