#include "tcApp.h"

#include <sstream>

namespace ios = tcx::ios;

void tcApp::setup() {
    tc::setTouchAsMouse(true);
    tc::setIndependentFps(tc::VSYNC, tc::EVENT_DRIVEN);
    downloadPath_ = std::filesystem::temp_directory_path() / "tcxios-background-example.html";
    tc::redraw();
}

void tcApp::update() {
    ios::update();
}

void tcApp::draw() {
    tc::clear(0.09f);
    tc::setColor(1.0f);

    std::ostringstream text;
    text << "tcxIOS background download\n\n"
         << "Download target: " << downloadPath_.string() << "\n"
         << "Progress: " << static_cast<int>(downloadFraction_ * 100.0) << "%\n\n"
         << "B: register and schedule app refresh\n"
         << "D: start background URLSession download\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'B') {
        registerAndScheduleRefresh();
    } else if (key == 'D') {
        startDownload();
    }
}

void tcApp::registerAndScheduleRefresh() {
    const std::string identifier = "com.trussc.tcxios.example.refresh";
    status_ = "Registering background refresh handler.";
    ios::backgroundTasks().registerHandler({identifier, ios::BackgroundTaskKind::AppRefresh},
                                           [this](const ios::BackgroundTaskContext& context) {
        status_ = "Background task ran: " + context.identifier;
        tc::redraw();
        return true;
    }, [this, identifier](ios::Result<void> registration) {
        if (!registration.ok) {
            status_ = "BG registration failed: " + ios::toString(registration.error.code) + " - " + registration.error.message;
            tc::redraw();
            return;
        }

        ios::backgroundTasks().schedule({identifier, ios::BackgroundTaskKind::AppRefresh, 60.0, false, false},
                                        [this](ios::Result<void> scheduled) {
            status_ = scheduled.ok
                ? "Background refresh scheduled."
                : "BG schedule failed: " + ios::toString(scheduled.error.code) + " - " + scheduled.error.message;
            tc::redraw();
        });
    });
    tc::redraw();
}

void tcApp::startDownload() {
    status_ = "Starting background download.";
    ios::BackgroundDownloadRequest request;
    request.url = "https://www.example.com/";
    request.destination = downloadPath_;
    request.identifier = "tcxios-example-download";
    ios::backgroundDownloads().download(request,
        [this](ios::Result<ios::BackgroundDownloadResult> result) {
            status_ = result.ok
                ? "Download finished: " + result.value.file.string()
                : "Download failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        },
        [this](const ios::BackgroundDownloadProgress& progress) {
            downloadFraction_ = progress.fractionCompleted;
            status_ = "Download progress updated.";
            tc::redraw();
        });
    tc::redraw();
}
