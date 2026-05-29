#include <tcxAssimp.h>
#include <tcx/assimp/AssimpConvert.h>

#include <assimp/matrix4x4.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using tcx::assimp::AnimationClip;
using tcx::assimp::Animator;
using tcx::assimp::Model;
using tcx::assimp::Mesh;
using tcx::assimp::Node;
using tcx::assimp::NodeAnimationChannel;
using tcx::assimp::PositionKey;
using tcx::assimp::RotationKey;

namespace {

struct TestRunner {
    int failed = 0;

    void expect(bool condition, const std::string& message) {
        if (condition) {
            std::cout << "PASS " << message << "\n";
        } else {
            failed++;
            std::cout << "FAIL " << message << "\n";
        }
    }
};

bool near(float a, float b, float eps = 0.001f) {
    return std::abs(a - b) <= eps;
}

bool nearVec(const tc::Vec3& a, const tc::Vec3& b, float eps = 0.001f) {
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

bool nearMat(const tc::Mat4& a, const tc::Mat4& b, float eps = 0.001f) {
    for (int i = 0; i < 16; ++i) {
        if (!near(a.m[i], b.m[i], eps)) return false;
    }
    return true;
}

void testAssimpConvert(TestRunner& t) {
    aiMatrix4x4 identity;
    t.expect(nearMat(tcx::assimp::toMat4(identity), tc::Mat4::identity()), "Assimp matrix identity conversion");

    aiMatrix4x4 ai;
    ai.a4 = 7.0f;
    ai.b4 = -3.0f;
    ai.c4 = 2.0f;
    tc::Mat4 m = tcx::assimp::toMat4(ai);
    t.expect(nearVec(m * tc::Vec3(1, 2, 3), tc::Vec3(8, -1, 5)), "Assimp matrix translation conversion");

    aiMatrix4x4 roundTrip = tcx::assimp::toAiMat4(m);
    t.expect(near(roundTrip.a4, 7.0f) && near(roundTrip.b4, -3.0f) && near(roundTrip.c4, 2.0f),
             "Assimp matrix reverse conversion");

    aiMatrix4x4 scale;
    scale.a1 = 2.0f;
    scale.b2 = 3.0f;
    scale.c3 = 4.0f;
    t.expect(nearVec(tcx::assimp::toMat4(scale) * tc::Vec3(1, 1, 1), tc::Vec3(2, 3, 4)),
             "Assimp matrix scale conversion");

    aiQuaternion aq(0.7071068f, 0.0f, 0.7071068f, 0.0f);
    tc::Quaternion tq = tcx::assimp::toQuaternion(aq);
    aiQuaternion ar = tcx::assimp::toAiQuaternion(tq);
    t.expect(near(ar.w, aq.w) && near(ar.y, aq.y), "Assimp quaternion conversion");
}

void testSceneViews(TestRunner& t) {
    tcx::assimp::SceneData scene;
    scene.nodes.resize(2);
    scene.nodes[0].name = "root";
    scene.nodes[0].localTransform = tc::Mat4::translate(3, 0, 0);
    scene.nodes[0].localPosition = tc::Vec3(3, 0, 0);
    scene.nodes[0].childIndices.push_back(1);
    scene.nodes[1].name = "child";
    scene.nodes[1].parentIndex = 0;
    scene.nodes[1].localTransform = tc::Mat4::translate(0, 4, 0);
    scene.nodes[1].localPosition = tc::Vec3(0, 4, 0);
    scene.nodes[1].meshIndices.push_back(0);
    scene.meshes.resize(1);
    scene.meshes[0].name = "mesh";
    scene.meshes[0].vertices.push_back({});
    scene.meshes[0].indices = {0, 0, 0};

    Node root(&scene, 0);
    Node child = root.getChild(0);
    t.expect(root.isValid() && root.getChildCount() == 1, "Node view exposes child count");
    t.expect(child.isValid() && child.getParent().getName() == "root", "Node view exposes parent");
    t.expect(nearVec(child.getGlobalTransform() * tc::Vec3(0, 0, 0), tc::Vec3(3, 4, 0)),
             "Node view computes parent-child global transform");
    child.setPosition(tc::Vec3(0, 5, 0));
    t.expect(nearVec(child.getGlobalTransform() * tc::Vec3(0, 0, 0), tc::Vec3(3, 5, 0)),
             "Node view updates local position");

    Mesh mesh(&scene, 0);
    t.expect(mesh.isValid() && mesh.getVertexCount() == 1 && mesh.getIndexCount() == 3,
             "Mesh view exposes geometry counts");
}

void testAnimatorBindFallback(TestRunner& t) {
    AnimationClip clip;
    clip.name = "partial";
    clip.durationTicks = 10.0;
    clip.ticksPerSecond = 10.0;

    NodeAnimationChannel ch;
    ch.nodeName = "Bone";
    ch.positionKeys.push_back(PositionKey{0.0, tc::Vec3(5, 0, 0)});
    ch.positionKeys.push_back(PositionKey{10.0, tc::Vec3(15, 0, 0)});
    clip.channels.push_back(ch);
    clip.channelIndex["Bone"] = 0;

    std::vector<AnimationClip> clips;
    clips.push_back(clip);

    Animator animator;
    animator.setClips(&clips);
    animator.play(0);
    animator.setNormalizedTime(0.5f);

    tc::Vec3 pos;
    tc::Quaternion rot;
    tc::Vec3 scale;
    bool ok = animator.getNodeTransform("Bone",
                                        tc::Vec3(1, 2, 3),
                                        tc::Quaternion::fromAxisAngle(tc::Vec3(0, 1, 0), 0.25f),
                                        tc::Vec3(2, 3, 4),
                                        pos,
                                        rot,
                                        scale);
    t.expect(ok && nearVec(pos, tc::Vec3(10, 0, 0)), "Animator interpolates present position track");
    t.expect(nearVec(scale, tc::Vec3(2, 3, 4)), "Animator preserves bind scale when scale track is missing");

    animator.pause();
    t.expect(animator.hasActiveClip() && !animator.isAdvancing(), "Animator pause keeps active clip but stops advancing");
}

void testModelTransform(TestRunner& t) {
    Model model;
    tc::Mat4 transform = tc::Mat4::translate(4, -2, 9)
                       * tc::Mat4::rotateY(0.35f)
                       * tc::Mat4::scale(2, 3, 4);
    model.setTransform(transform);
    t.expect(nearVec(model.getPosition(), tc::Vec3(4, -2, 9)), "Model::setTransform extracts position");
    t.expect(nearVec(model.getScale(), tc::Vec3(2, 3, 4), 0.01f), "Model::setTransform extracts scale");
}

void testSimpleObjLoad(TestRunner& t) {
    fs::path objPath = fs::path(TCX_ASSIMP_VERIFY_DATA_DIR) / "simple.obj";
    Model model;
    bool loaded = model.load(objPath.string());
    t.expect(loaded, "Model loads bundled simple OBJ");
    t.expect(model.getMeshCount() == 1, "Simple OBJ imports one mesh");
    t.expect(nearVec(model.getSceneSize(), tc::Vec3(2, 3, 0)), "Simple OBJ scene bounds are correct");
    t.expect(nearVec(model.getSceneMin(), tc::Vec3(0, 0, 0)) &&
             nearVec(model.getSceneMax(), tc::Vec3(2, 3, 0)), "Model exposes scene min/max");
    t.expect(model.getMesh(0).isValid() && model.getMesh(0).getVertexCount() == 3,
             "Model exposes Mesh view");
    t.expect(model.getNodeCount() > 0 && model.getNode(0).isValid(),
             "Model exposes Node view");
}

void testOptionalSampleAssets(TestRunner& t) {
    fs::path sampleRoot = TCX_ASSIMP_SAMPLE_DATA_DIR;

    fs::path foxPath = sampleRoot / "Fox" / "Fox_05.fbx";
    if (fs::exists(foxPath)) {
        Model fox;
        fox.setScaleNormalize(true);
        bool loaded = fox.load(foxPath.string());
        t.expect(loaded, "Optional Fox FBX loads");
        t.expect(fox.getAnimationCount() > 0, "Optional Fox FBX imports animation clips");
        t.expect(fox.getBoneCount() > 0, "Optional Fox FBX imports bones");
        if (loaded && fox.getAnimationCount() > 0) {
            fox.play(0);
            fox.setAnimationNormalizedTime(0.5f);
            fox.pause();
            t.expect(fox.animator().hasActiveClip() && !fox.animator().isAdvancing(), "Paused Fox animation keeps active pose");
        }
        if (loaded && fox.getBoneCount() > 0) {
            bool overrideSet = fox.setBoneGlobalTransform((size_t)0, tc::Mat4::translate(1, 2, 3));
            bool overrideCleared = fox.clearBoneOverride((size_t)0);
            t.expect(overrideSet && overrideCleared, "Bone global override set/clear by index");
        }
    } else {
        std::cout << "SKIP Optional Fox FBX not found at " << foxPath << "\n";
    }

    fs::path helmetPath = sampleRoot / "FlightHelmet" / "FlightHelmet.gltf";
    if (fs::exists(helmetPath)) {
        tcx::assimp::Importer importer;
        tcx::assimp::SceneData scene;
        bool loaded = importer.load(helmetPath.string(), scene);
        t.expect(loaded, "Optional FlightHelmet glTF loads");
        bool hasBaseColorTex = false;
        bool hasMetalRoughTex = false;
        bool hasOcclusionTex = false;
        for (const auto& mat : scene.materials) {
            hasBaseColorTex = hasBaseColorTex || !mat.diffuseTexture.empty();
            hasMetalRoughTex = hasMetalRoughTex || !mat.metallicRoughnessTexture.empty();
            hasOcclusionTex = hasOcclusionTex || !mat.occlusionTexture.empty();
        }
        t.expect(hasBaseColorTex, "FlightHelmet imports base-color texture refs");
        t.expect(hasMetalRoughTex, "FlightHelmet imports metallic-roughness texture refs");
        t.expect(hasOcclusionTex, "FlightHelmet imports occlusion texture refs");
    } else {
        std::cout << "SKIP Optional FlightHelmet glTF not found at " << helmetPath << "\n";
    }
}

} // namespace

int main() {
    TestRunner t;
    testAssimpConvert(t);
    testSceneViews(t);
    testAnimatorBindFallback(t);
    testModelTransform(t);
    testSimpleObjLoad(t);
    testOptionalSampleAssets(t);

    if (t.failed == 0) {
        std::cout << "tcxAssimp verification PASS\n";
        return 0;
    }
    std::cout << "tcxAssimp verification FAIL count=" << t.failed << "\n";
    return 1;
}
