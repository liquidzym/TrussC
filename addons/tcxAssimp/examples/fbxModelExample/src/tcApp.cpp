#include "tcApp.h"
using namespace tc;

void tcApp::setup() {
    setWindowTitle("tcxAssimp - FBX Model Transform Example");
    setDataPathRoot("../../../../../staticModelExample/bin/data/");
    light_.setDirectional(tc::Vec3(-1, 1, -1));
    light_.setAmbient(0.3f, 0.3f, 0.32f);
    light_.setDiffuse(0.9f, 0.9f, 0.86f);
    material_ = tc::Material::plastic(tc::Color(0.74f, 0.74f, 0.78f));
    loadModel(getDataPath("Fox/Fox_05.fbx"));
}

void tcApp::draw() {
    clear(0.14f, 0.14f, 0.17f, 1.0f);
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
        if (skeleton_) model_.drawSkeleton();
        if (bbox_) model_.drawBoundingBox();
        popMatrix();
    }
    setupScreenOrtho();
    setColor(1, 1, 1);
    drawBitmapString(status_, 12, 18);
    drawBitmapString("[1]Fox FBX [2]Astroboy DAE [<- ->]rotate [W]wire [S]skeleton [B]bbox", 12, 38);
}

void tcApp::keyPressed(int key) {
    if (key == '1') loadModel(getDataPath("Fox/Fox_05.fbx"));
    if (key == '2') loadModel(getDataPath("Astroboy/astroBoy_walk.dae"));
    if (key == KEY_LEFT) rotY_ -= 0.15f;
    if (key == KEY_RIGHT) rotY_ += 0.15f;
    if (key == 'W') wire_ = !wire_;
    if (key == 'S') skeleton_ = !skeleton_;
    if (key == 'B') bbox_ = !bbox_;
}

void tcApp::filesDropped(const std::vector<std::string>& files) {
    if (!files.empty()) loadModel(files.front());
}

void tcApp::loadModel(const std::string& path) {
    model_.setScaleNormalize(true);
    model_.setGpuSkinningEnabled(false);
    if (!model_.load(path)) {
        status_ = "FAILED: " + path;
        return;
    }
    auto size = model_.getSceneSize();
    status_ = "OK nodes:" + std::to_string(model_.getNodeCount())
            + " meshes:" + std::to_string(model_.getMeshCount())
            + " bbox:" + std::to_string((int)size.x) + ","
            + std::to_string((int)size.y) + ","
            + std::to_string((int)size.z);
}
