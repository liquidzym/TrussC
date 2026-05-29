#include "tcApp.h"
using namespace tc;

void tcApp::setup() {
    setWindowTitle("tcxAssimp - glTF Material Example");
    setDataPathRoot("../../../../../staticModelExample/bin/data/");
    light_.setDirectional(tc::Vec3(-0.6f, 0.8f, -0.4f));
    light_.setAmbient(0.25f, 0.25f, 0.28f);
    light_.setDiffuse(0.95f, 0.92f, 0.86f);
    material_ = tc::Material::plastic(tc::Color(0.8f, 0.8f, 0.82f));
    loadModel(getDataPath("FlightHelmet/FlightHelmet.gltf"));
}

void tcApp::update() {
    rotation_ += 0.003f;
}

void tcApp::draw() {
    clear(0.12f, 0.13f, 0.15f, 1.0f);
    float cx = getWindowWidth() * 0.5f;
    float cy = getWindowHeight() * 0.56f;
    addLight(light_);
    setCameraPosition(cx, cy, 900);
    setMaterial(material_);

    if (model_.isLoaded()) {
        pushMatrix();
        translate(cx, cy, 0);
        rotateY(rotation_);
        model_.drawFaces();
        if (wire_) model_.drawWireframe();
        if (bbox_) model_.drawBoundingBox();
        popMatrix();
    }

    setupScreenOrtho();
    setColor(1, 1, 1);
    drawBitmapString(status_, 12, 18);
    drawBitmapString("[1]FlightHelmet [2]Payphone [W]wire [B]bbox", 12, 38);
}

void tcApp::keyPressed(int key) {
    if (key == '1') loadModel(getDataPath("FlightHelmet/FlightHelmet.gltf"));
    if (key == '2') loadModel(getDataPath("Payphone/korean_public_payphone_01_1k.gltf"));
    if (key == 'W') wire_ = !wire_;
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
    status_ = "OK materials:" + std::to_string(model_.getSceneData().materials.size())
            + " meshes:" + std::to_string(model_.getMeshCount())
            + " path:" + path;
}
