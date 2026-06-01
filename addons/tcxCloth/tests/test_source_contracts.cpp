#include "test_common.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open " + path.string());
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void requireContains(const std::string& text, const std::string& needle, const char* message) {
    require(text.find(needle) != std::string::npos, message);
}

void requireNotContains(const std::string& text, const std::string& needle, const char* message) {
    require(text.find(needle) == std::string::npos, message);
}

int countOccurrences(const std::string& text, const std::string& needle) {
    int count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path root = argc > 1 ? std::filesystem::path(argv[1])
                                                : std::filesystem::current_path();

    for (const auto& example : {"clothBasic", "clothCollision", "clothWind"}) {
        const auto source = readText(root / "examples" / example / "src" / "tcApp.cpp");
        requireContains(source,
                        "settings.backend = tcxCloth::ClothSettings::SolverBackend::Auto;",
                        "examples request Auto so GPU is preferred with CPU fallback");
        requireNotContains(source,
                           "settings.backend = tcxCloth::ClothSettings::SolverBackend::CpuReference;",
                           "examples do not pin themselves to CPU reference");
    }

    const auto readme = readText(root / "README.md");
    requireContains(readme, "`Auto`: prefers the GPU TexturePingPong backend",
                    "README documents Auto as GPU-first");
    requireNotContains(readme, "Auto`: selects the best available backend",
                       "README no longer describes Auto as CPU-only");
    requireContains(readme, "`examples/clothBasic`: GPU-first Auto",
                    "README documents basic example backend");
    requireContains(readme, "`examples/clothCollision`: GPU-first Auto",
                    "README documents collision example backend");
    requireContains(readme, "`examples/clothWind`: GPU-first Auto",
                    "README documents wind example backend");

    const auto clothSource = readText(root / "src" / "Cloth.cpp");
    requireContains(clothSource, "void Cloth::solveGpuConstraints(float dt)",
                    "GPU solver has a named constraint phase");
    require(countOccurrences(clothSource, "solveGpuConstraints(dt);") >= 2,
            "GPU step runs constraint phases before and after Verlet integration");
    requireContains(clothSource, "uniforms.options[3] = 0.30f;",
                    "GPU constraint pass uses explicit relaxation for stable stiffness");
    requireNotContains(clothSource, "gpu y=",
                       "temporary GPU bounds diagnostics are not shipped");
    requireNotContains(clothSource, "collision minDist=",
                       "temporary GPU collision diagnostics are not shipped");

    const auto shader = readText(root / "shaders" / "cloth.glsl");
    requireContains(shader, "correction += delta * ((len - restLen) / len) * (0.5 * k);",
                    "GPU constraints accumulate pair corrections before relaxation");
    requireContains(shader, "vec3 corrected = pos + correction * options.w;",
                    "GPU constraints apply an explicit relaxation value");
    requireNotContains(shader, "correction / max(weight",
                       "GPU constraints do not dilute stiffness by neighbor count");

    const auto collisionExample = readText(root / "examples" / "clothCollision" / "src" / "tcApp.cpp");
    requireContains(collisionExample, "sphere_.radius = 76.0f;",
                    "collision example uses a large enough sphere for a visible cloth dome");
    requireContains(collisionExample, "-42.0f + 88.0f",
                    "collision example keeps the sphere close behind the cloth for visible back-side pushing");

    std::cout << "tcxCloth_source_contracts passed\n";
    return 0;
}
