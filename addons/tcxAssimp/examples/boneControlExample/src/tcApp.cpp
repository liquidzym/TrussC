#include "tcApp.h"
using namespace tc;

void tcApp::setup() {
    setWindowTitle("tcxAssimp - Bone Control Example");
    setDataPathRoot("../../../../../staticModelExample/bin/data/");
    light_.setDirectional(tc::Vec3(-1, 1, -1));
    light_.setAmbient(0.3f, 0.3f, 0.35f);
    light_.setDiffuse(0.9f, 0.9f, 0.85f);
    material_ = tc::Material::plastic(tc::Color(0.72f, 0.72f, 0.76f));
    loadModel(getDataPath("Fox/Fox_05.fbx"));
}

void tcApp::update() {
    time_ += (float)getDeltaTime();
    model_.update((float)getDeltaTime());
    if (override_) applyOverride();
}

void tcApp::draw() {
    clear(0.13f, 0.14f, 0.16f, 1.0f);
    float cx = getWindowWidth() * 0.5f;
    float cy = getWindowHeight() * 0.56f;
    addLight(light_);
    setCameraPosition(cx, cy, 1000);
    setMaterial(material_);
    if (model_.isLoaded()) {
        pushMatrix();
        translate(cx, cy, 0);
        model_.drawFaces();
        if (skeleton_) model_.drawSkeleton();
        popMatrix();
    }
    setupScreenOrtho();
    setColor(1, 1, 1);
    drawBitmapString(status_, 12, 18);
    drawBitmapString("[B]toggle override [C]clear [S]skeleton [G]gpu", 12, 38);
}

void tcApp::keyPressed(int key) {
    if (key == 'O') loadModel(getDataPath("Fox/Fox_05.fbx"));
    if (key == 'B') {
        override_ = !override_;
        if (!override_) model_.clearBoneOverrides();
        status_ = override_ ? "Override first bone enabled" : "Override disabled";
    }
    if (key == 'C') { override_ = false; model_.clearBoneOverrides(); status_ = "Bone overrides cleared"; }
    if (key == 'S') skeleton_ = !skeleton_;
    if (key == 'G') model_.setGpuSkinningEnabled(!model_.isGpuSkinningEnabled());
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
    status_ = "OK bones:" + std::to_string(model_.getBoneCount());
}

void tcApp::applyOverride() {
    if (!model_.isLoaded() || model_.getBoneCount() == 0) return;
    tc::Mat4 m = tc::Mat4::translate(0.0f, std::sin(time_) * 12.0f, 0.0f)
              * tc::Mat4::rotateY(std::sin(time_) * 0.4f);
    model_.setBoneGlobalTransform((size_t)0, m);
}
