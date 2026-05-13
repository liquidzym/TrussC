#include <tcxFlowTools.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireNear(float a, float b, float eps, const char* message) {
    if (std::fabs(a - b) > eps) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    tcx::flow::SoftBody2D body;
    body.gravity = tc::Vec2(0.0f, 0.0f);
    body.boundsEnabled = false;
    body.solverIterations = 8;

    tcx::flow::SoftBody2DGridSettings settings;
    settings.columns = 4;
    settings.rows = 3;
    settings.spacing = 10.0f;
    settings.structural = true;
    settings.shear = true;
    settings.bend = true;
    settings.bendDistance = 2;

    const auto grid = body.addGrid(tc::Vec2(20.0f, 30.0f), settings);
    require(body.particleCount() == 12, "grid particle count");
    require(body.constraintCount() == 39, "grid structural/shear/bend constraint count");
    require(grid.node(3, 2) == 11, "grid node indexing");

    body.setFixed(grid.node(0, 0), true);
    const tc::Vec2 fixedPos = body.particles()[grid.node(0, 0)].position;
    body.addForce(grid.node(0, 0), tc::Vec2(1000.0f, 1000.0f));
    body.step(1.0f / 60.0f);
    require(body.particles()[grid.node(0, 0)].position == fixedPos, "fixed particle remains fixed");

    const int moving = grid.node(1, 1);
    const float beforeY = body.particles()[moving].position.y;
    body.gravity = tc::Vec2(0.0f, 200.0f);
    body.step(1.0f / 60.0f);
    require(body.particles()[moving].position.y > beforeY, "gravity moves free particle");

    const int nearest = body.nearestParticle(body.particles()[moving].position, 5.0f);
    require(nearest == moving, "nearest particle lookup");

    const int cut = body.cutConstraintsNear(body.particles()[moving].position, 15.0f);
    require(cut > 0, "constraint cutting disables nearby springs");

    body.clear();
    require(body.empty(), "clear removes all softbody state");

    tcx::flow::SoftBody2D pair;
    pair.gravity = tc::Vec2(0.0f, 0.0f);
    pair.boundsEnabled = false;
    const int a = pair.addParticle(tc::Vec2(0.0f, 0.0f));
    const int b = pair.addParticle(tc::Vec2(30.0f, 0.0f));
    pair.addConstraint(a, b, 10.0f, 1.0f);
    pair.step(1.0f / 60.0f);
    requireNear(pair.particles()[a].position.distance(pair.particles()[b].position), 10.0f, 0.01f,
                "constraint solves toward rest length");

    std::cout << "tcxFlowTools_softbody2d passed\n";
    return 0;
}
