#include "tcApp.h"

#include <fstream>
#include <sstream>

namespace ios = tcx::ios;

void tcApp::setup() {
    tc::setTouchAsMouse(true);
    tc::setIndependentFps(tc::VSYNC, tc::EVENT_DRIVEN);
    exportPath_ = std::filesystem::temp_directory_path() / "tcxios-share-example.txt";
    std::ofstream out(exportPath_);
    out << "tcxIOS share/export example\n";
    tc::redraw();
}

void tcApp::update() {
    ios::update();
}

void tcApp::draw() {
    tc::clear(0.10f);
    tc::setColor(1.0f);

    std::ostringstream text;
    text << "tcxIOS files/photos/share\n\n"
         << "I: import files\n"
         << "P: pick photos\n"
         << "S: share text\n"
         << "E: export temp file\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'I') {
        status_ = "Import requested.";
        ios::files().importFiles({{"public.image", "public.json"}, true, true},
                                 [this](ios::Result<std::vector<ios::PickedFile>> result) {
            if (result.ok) {
                status_ = "Imported " + std::to_string(result.value.size()) + " file(s).";
            } else {
                status_ = "Import failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            }
            tc::redraw();
        });
        tc::redraw();
    } else if (key == 'P') {
        status_ = "Photo pick requested.";
        ios::photos().pickPhotos({4, true}, [this](ios::Result<std::vector<ios::PickedPhoto>> result) {
            if (result.ok) {
                status_ = "Picked " + std::to_string(result.value.size()) + " photo(s).";
            } else {
                status_ = "Photo pick failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            }
            tc::redraw();
        });
        tc::redraw();
    } else if (key == 'S') {
        status_ = "Share requested.";
        ios::nativeUI().share({{}, {"Shared from tcxIOS / TrussC."}, "tcxIOS"},
                              [this](ios::Result<ios::ShareResult> result) {
            if (result.ok) {
                status_ = result.value.completed ? "Share completed." : "Share dismissed.";
            } else {
                status_ = "Share failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            }
            tc::redraw();
        });
        tc::redraw();
    } else if (key == 'E') {
        status_ = "Export requested.";
        ios::files().exportFile({exportPath_, "tcxios-share-example.txt"}, [this](ios::Result<void> result) {
            status_ = result.ok
                ? "Export request accepted."
                : "Export failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
        tc::redraw();
    }
}
