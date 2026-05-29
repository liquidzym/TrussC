#include "tcApp.h"
using namespace tc;

void tcApp::setup() {
    setWindowTitle("tcxAssimp - Debug Scene Hierarchy");
    setDataPathRoot("../../../../../staticModelExample/bin/data/");
    loadModel(getDataPath("FlightHelmet/FlightHelmet.gltf"));
}

void tcApp::draw() {
    clear(0.11f, 0.12f, 0.14f, 1.0f);
    setupScreenOrtho();
    setColor(1, 1, 1);
    drawBitmapString(status_, 12, 18);
    drawBitmapString("[1]FlightHelmet [2]Fox [Up/Down]scroll", 12, 38);
    int y = 64;
    for (int i = scroll_; i < (int)lines_.size() && y < getWindowHeight() - 16; ++i) {
        drawBitmapString(lines_[i], 12, y);
        y += 18;
    }
}

void tcApp::keyPressed(int key) {
    if (key == '1') loadModel(getDataPath("FlightHelmet/FlightHelmet.gltf"));
    if (key == '2') loadModel(getDataPath("Fox/Fox_05.fbx"));
    if (key == KEY_UP && scroll_ > 0) scroll_--;
    if (key == KEY_DOWN && scroll_ + 1 < (int)lines_.size()) scroll_++;
}

void tcApp::filesDropped(const std::vector<std::string>& files) {
    if (!files.empty()) loadModel(files.front());
}

void tcApp::loadModel(const std::string& path) {
    model_.setScaleNormalize(false);
    model_.setGpuSkinningEnabled(false);
    if (!model_.load(path)) {
        status_ = "FAILED: " + path;
        lines_.clear();
        return;
    }
    status_ = "OK nodes:" + std::to_string(model_.getNodeCount())
            + " meshes:" + std::to_string(model_.getMeshCount())
            + " materials:" + std::to_string(model_.getSceneData().materials.size());
    scroll_ = 0;
    rebuildLines();
}

void tcApp::rebuildLines() {
    lines_.clear();
    const auto& scene = model_.getSceneData();
    if (scene.rootNodeIndex >= 0) {
        appendNode(scene.rootNodeIndex, 0);
    } else {
        for (int i = 0; i < (int)scene.nodes.size(); ++i) {
            if (scene.nodes[i].parentIndex < 0) appendNode(i, 0);
        }
    }
}

void tcApp::appendNode(int nodeIndex, int depth) {
    const auto& scene = model_.getSceneData();
    if (nodeIndex < 0 || nodeIndex >= (int)scene.nodes.size()) return;
    const auto& node = scene.nodes[nodeIndex];
    std::string indent((size_t)depth * 2, ' ');
    std::string line = indent + "[" + std::to_string(nodeIndex) + "] " + node.name
                     + " meshes:" + std::to_string(node.meshIndices.size())
                     + " children:" + std::to_string(node.childIndices.size());
    lines_.push_back(line);
    for (int meshIndex : node.meshIndices) {
        if (meshIndex >= 0 && meshIndex < (int)scene.meshes.size()) {
            const auto& mesh = scene.meshes[meshIndex];
            lines_.push_back(indent + "  mesh " + std::to_string(meshIndex)
                           + " mat:" + std::to_string(mesh.materialIndex)
                           + " verts:" + std::to_string(mesh.vertices.size()));
        }
    }
    for (int child : node.childIndices) appendNode(child, depth + 1);
}
