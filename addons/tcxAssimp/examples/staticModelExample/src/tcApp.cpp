#include "tcApp.h"
#include <algorithm>

void tcApp::setup() {
    setWindowTitle("tcxAssimp — 3D Model Viewer");
    setDataPathRoot("../../../data/");
    light_.setDirectional(tc::Vec3(-1, 1, -1));
    light_.setAmbient(0.3f, 0.3f, 0.35f);
    light_.setDiffuse(0.9f, 0.9f, 0.85f);
    light_.setSpecular(0.5f, 0.5f, 0.5f);
    mat_ = tc::Material::plastic(tc::Color(0.7f, 0.7f, 0.75f));
}

void tcApp::update() {
    if (autoRotate_) rotY_ += 0.005f;
    if (model_.isLoaded()) model_.updateAnimation((float)getDeltaTime());
}

void tcApp::draw() {
    clear(0.15f, 0.15f, 0.18f, 1.0f);
    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() * 0.56f;

    if (model_.isLoaded()) {
        addLight(light_); setCameraPosition(cx, cy, 1000); setMaterial(mat_);
        pushMatrix(); translate(cx, cy, 0); scale(viewScale_); rotateY(rotY_);
        model_.drawFaces();
        if (wire_) model_.drawWireframe();
        if (skel_) model_.drawSkeleton();
        popMatrix();
    } else {
        addLight(light_); setCameraPosition(cx, cy, 1000); setMaterial(mat_);
        pushMatrix(); translate(cx, cy, 0); rotateY(rotY_); drawBox(100, 100, 100);
        popMatrix();
    }

    setupScreenOrtho(); setColor(1, 1, 1);
    drawBitmapString(status_, 12, 16);
    string info = "[W]wire [S]skeleton [G]gpu [A]auto [<- ->]rotate [1-9]anim [0]stop [O]Fox [D]drop";
    if (model_.hasAnimations()) info += " | Anims:" + to_string(model_.getAnimationCount());
    if (model_.isLoaded()) info += model_.isGpuSkinningEnabled() ? " | GPU skin" : " | CPU skin";
    drawBitmapString(info, 12, 36);
}

void tcApp::keyPressed(int k) {
    if (k == 'W') wire_ = !wire_;
    if (k == 'S') skel_ = !skel_;
    if (k == 'G') {
        model_.setGpuSkinningEnabled(!model_.isGpuSkinningEnabled());
        status_ = model_.isGpuSkinningEnabled()
                ? (model_.isGpuSkinningAvailable() ? "GPU skinning enabled" : "GPU skinning requested, CPU fallback")
                : "CPU skinning enabled";
    }
    if (k == 'A') autoRotate_ = !autoRotate_;
    if (k == KEY_LEFT) rotY_ -= 0.15f;
    if (k == KEY_RIGHT) rotY_ += 0.15f;
    if (k == 'O') tryLoad(getDataPath("Fox/Fox_05.fbx"));
    if (k >= '1' && k <= '9') {
        int idx = k - '1';
        if (idx < (int)model_.getAnimationCount()) {
            model_.playAnimation(idx);
            status_ = "Anim " + to_string(idx + 1) + "/" + to_string(model_.getAnimationCount())
                    + ": " + model_.getAnimationName(idx)
                    + " | " + to_string((int)model_.getAnimationDuration(idx)) + "s";
        } else if (model_.hasAnimations()) {
            status_ = "No animation " + to_string(idx + 1)
                    + " | imported clips: " + to_string(model_.getAnimationCount());
        }
    }
    if (k == '0') { model_.stopAnimation(); status_ = "Stopped"; }
}

void tcApp::filesDropped(const vector<string>& f) { if (!f.empty()) tryLoad(f[0]); }

void tcApp::tryLoad(const string& p) {
    model_.setScaleNormalize(true);
    model_.setGpuSkinningEnabled(true);
    bool ok = model_.load(p);
    if (ok) {
        model_.setGpuSkinningEnabled(true);
        auto sz = model_.getSceneSize();
        viewScale_ = 1.0f;
        rotY_ = 0.0f;
        status_ = "OK | m:" + to_string(model_.getMeshCount())
                + " a:" + to_string(model_.getAnimationCount())
                + " bones:" + to_string(model_.getBoneCount())
                + " gpu:" + string(model_.isGpuSkinningAvailable() ? "yes" : "no")
                + " size:" + to_string((int)std::max({sz.x, sz.y, sz.z}));
    } else {
        status_ = "FAILED: " + p;
    }
}
