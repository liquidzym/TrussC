#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>

namespace {

tc::Color constraintColor(tcx::flow::SoftBody2DConstraintKind kind, bool showBend) {
    using tcx::flow::SoftBody2DConstraintKind;
    if (kind == SoftBody2DConstraintKind::Structural) return tc::Color(0.86f, 0.90f, 0.96f, 0.44f);
    if (kind == SoftBody2DConstraintKind::Shear) return tc::Color(0.30f, 0.60f, 0.95f, 0.24f);
    return showBend ? tc::Color(1.0f, 0.45f, 0.16f, 0.22f) : tc::Color(0.0f, 0.0f, 0.0f, 0.0f);
}

} // namespace

void tcApp::setup() {
    rebuild();
}

void tcApp::update() {
    if (paused_) return;
    const float time = tc::getElapsedTimef();
    applyWind(time);
    softBody_.step(static_cast<float>(tc::getDeltaTime()));
}

void tcApp::draw() {
    tc::clear(0.028f, 0.032f, 0.038f);

    for (const auto& cloth : cloths_) {
        drawClothFill(cloth);
    }
    drawConstraints();
    if (showParticles_) {
        drawParticles();
    }

    tc::setColor(1.0f);
    tc::drawBitmapString("softbody2d-cloth | drag particles | right drag cuts | w wind | b bend | p particles | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("PixelFlow SoftBody2D_Cloth parity target | particles " +
                             tc::toString(softBody_.particleCount()) +
                             " constraints " + tc::toString(softBody_.constraintCount()),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'r')) rebuild();
    if (keyIs(key, 'p')) showParticles_ = !showParticles_;
    if (keyIs(key, 'b')) showBend_ = !showBend_;
    if (keyIs(key, ' ')) paused_ = !paused_;
    if (keyIs(key, 'w')) windStrength_ = windStrength_ > 0.0f ? 0.0f : 28.0f;
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    if (button == 0) {
        grabbedParticle_ = softBody_.nearestParticle(pos, 34.0f);
        if (grabbedParticle_ >= 0) {
            softBody_.setPosition(grabbedParticle_, pos);
        }
    } else {
        softBody_.cutConstraintsNear(pos, 22.0f);
    }
}

void tcApp::mouseReleased(tc::Vec2 pos, int button) {
    (void)pos;
    (void)button;
    grabbedParticle_ = -1;
}

void tcApp::mouseDragged(tc::Vec2 pos, int button) {
    if (button == 0 && grabbedParticle_ >= 0) {
        softBody_.setPosition(grabbedParticle_, pos);
    } else if (button != 0) {
        softBody_.cutConstraintsNear(pos, 24.0f);
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    rebuild();
}

void tcApp::rebuild() {
    softBody_.clear();
    cloths_.clear();
    grabbedParticle_ = -1;

    softBody_.boundsMin = tc::Vec2(8.0f, 8.0f);
    softBody_.boundsMax = tc::Vec2(static_cast<float>(tc::getWindowWidth()) - 8.0f,
                                   static_cast<float>(tc::getWindowHeight()) - 8.0f);
    softBody_.gravity = tc::Vec2(0.0f, 420.0f);
    softBody_.damping = 0.994f;
    softBody_.solverIterations = 7;

    tcx::flow::SoftBody2DGridSettings settings;
    settings.columns = 25;
    settings.rows = 25;
    settings.spacing = std::min(tc::getWindowWidth(), tc::getWindowHeight()) / 58.0f;
    settings.particleRadius = 3.2f;
    settings.structuralStiffness = 0.98f;
    settings.shearStiffness = 0.78f;
    settings.bendStiffness = 0.30f;
    settings.bendDistance = 3;

    const float clothW = (settings.columns - 1) * settings.spacing;
    const float availableW = static_cast<float>(tc::getWindowWidth()) - 180.0f;
    const float spacing = std::max(48.0f, (availableW - clothW * 2.0f) / 3.0f);
    const float startY = 92.0f;

    for (int i = 0; i < 2; ++i) {
        const tc::Vec2 origin(90.0f + spacing + i * (clothW + spacing), startY);
        ClothView cloth;
        cloth.grid = softBody_.addGrid(origin, settings);
        cloth.materialColor = i == 0 ? tc::Color(1.0f, 0.56f, 0.04f, 0.20f)
                                     : tc::Color(0.0f, 0.64f, 1.0f, 0.20f);
        cloth.particleColor = i == 0 ? tc::Color(1.0f, 0.68f, 0.12f, 0.82f)
                                     : tc::Color(0.12f, 0.72f, 1.0f, 0.82f);
        softBody_.setFixed(cloth.grid.node(0, 0), true);
        softBody_.setFixed(cloth.grid.node(cloth.grid.columns - 1, 0), true);
        cloths_.push_back(cloth);
    }
}

void tcApp::applyWind(float time) {
    if (windStrength_ <= 0.0f) return;
    auto& particles = softBody_.particles();
    for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
        const float wave = std::sin(time * 1.7f + particles[i].position.y * 0.035f);
        softBody_.addForce(i, tc::Vec2(windStrength_ * (0.25f + wave), 0.0f));
    }
}

void tcApp::drawClothFill(const ClothView& cloth) {
    const auto& particles = softBody_.particles();
    tc::setColor(cloth.materialColor);
    for (int y = 0; y + 1 < cloth.grid.rows; ++y) {
        for (int x = 0; x + 1 < cloth.grid.columns; ++x) {
            const auto& p00 = particles[cloth.grid.node(x, y)].position;
            const auto& p10 = particles[cloth.grid.node(x + 1, y)].position;
            const auto& p01 = particles[cloth.grid.node(x, y + 1)].position;
            const auto& p11 = particles[cloth.grid.node(x + 1, y + 1)].position;
            tc::drawTriangle(tc::Vec3(p00, 0), tc::Vec3(p10, 0), tc::Vec3(p11, 0));
            tc::drawTriangle(tc::Vec3(p00, 0), tc::Vec3(p11, 0), tc::Vec3(p01, 0));
        }
    }
}

void tcApp::drawConstraints() {
    const auto& particles = softBody_.particles();
    for (const auto& constraint : softBody_.constraints()) {
        if (!constraint.enabled) continue;
        const tc::Color color = constraintColor(constraint.kind, showBend_);
        if (color.a <= 0.0f) continue;
        tc::setColor(color);
        const auto& a = particles[constraint.a].position;
        const auto& b = particles[constraint.b].position;
        tc::drawLine(a.x, a.y, b.x, b.y);
    }
}

void tcApp::drawParticles() {
    const auto& particles = softBody_.particles();
    for (const auto& particle : particles) {
        tc::setColor(particle.fixed ? tc::Color(1.0f, 1.0f, 1.0f, 0.95f)
                                    : tc::Color(0.92f, 0.95f, 1.0f, 0.62f));
        tc::drawCircle(particle.position.x, particle.position.y, particle.fixed ? 5.5f : 2.8f);
    }
}
