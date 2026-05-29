#include "tcApp.h"
using namespace tc;

void tcApp::setup() {
    setWindowTitle("tcxAssimp - FBX Animation Example");
    setDataPathRoot("../../../../../staticModelExample/bin/data/");
    light_.setDirectional(tc::Vec3(-1, 1, -1));
    light_.setAmbient(0.3f, 0.3f, 0.35f);
    light_.setDiffuse(0.9f, 0.9f, 0.85f);
    material_ = tc::Material::plastic(tc::Color(0.72f, 0.72f, 0.76f));
    loadModel(getDataPath("Fox/Fox_05.fbx"));
}

void tcApp::update() {
    rotY_ += 0.003f;
    model_.update((float)getDeltaTime());
}

void tcApp::draw() {
    clear(0.13f, 0.13f, 0.16f, 1.0f);
    float cx = getWindowWidth() * 0.5f;
    float cy = getWindowHeight() * 0.56f;
    addLight(light_);
    setCameraPosition(cx, cy, 1000);
    setMaterial(material_);
    if (model_.isLoaded()) {
        pushMatrix();
        translate(cx, cy, 0);
        rotateY(rotY_);
        model_.drawFaces();
        if (wire_) model_.drawWireframe();
        popMatrix();
    }
    setupScreenOrtho();
    setColor(1, 1, 1);
    drawBitmapString(status_, 12, 18);
    drawBitmapString("[1-9]play [P]pause [0]stop [L]loop [< >]speed [G]gpu [W]wire", 12, 38);
}

void tcApp::keyPressed(int key) {
    if (key >= '1' && key <= '9') playIndex((size_t)(key - '1'));
    if (key == 'O') loadModel(getDataPath("Fox/Fox_05.fbx"));
    if (key == 'P') { model_.pause(); status_ = "Paused"; }
    if (key == '0') { model_.stop(); status_ = "Stopped"; }
    if (key == 'L') { model_.setLoop(true); status_ = "Loop enabled"; }
    if (key == ',') { speed_ *= 0.75f; model_.setAnimationSpeed(speed_); status_ = "Speed " + std::to_string(speed_); }
    if (key == '.') { speed_ *= 1.25f; model_.setAnimationSpeed(speed_); status_ = "Speed " + std::to_string(speed_); }
    if (key == 'G') {
        model_.setGpuSkinningEnabled(!model_.isGpuSkinningEnabled());
        status_ = model_.isGpuSkinningEnabled() ? "GPU skinning requested" : "CPU skinning";
    }
    if (key == 'W') wire_ = !wire_;
}

void tcApp::filesDropped(const std::vector<std::string>& files) {
    if (!files.empty()) loadModel(files.front());
}

void tcApp::loadModel(const std::string& path) {
    model_.setScaleNormalize(true);
    model_.setGpuSkinningEnabled(true);
    if (!model_.load(path)) {
        status_ = "FAILED: " + path;
        return;
    }
    if (model_.hasAnimations()) model_.play(0);
    status_ = "OK animations:" + std::to_string(model_.getAnimationCount())
            + " bones:" + std::to_string(model_.getBoneCount())
            + " gpu:" + std::string(model_.isGpuSkinningAvailable() ? "yes" : "fallback");
}

void tcApp::playIndex(size_t index) {
    if (index >= model_.getAnimationCount()) {
        status_ = "No animation " + std::to_string(index + 1);
        return;
    }
    model_.play(index);
    status_ = "Anim " + std::to_string(index + 1) + ": " + model_.getAnimationName(index);
}
