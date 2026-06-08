#pragma once

#include <TrussC.h>
#include <tcxFlowTools.h>

#include <array>
#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void windowResized(int width, int height) override;

private:
    enum class ViewMode {
        Density,
        Velocity,
        Combined
    };

    struct RegionState {
        tcx::flow::AverageFlow flow;
        tc::Color color;
    };

    void resizeSystems();
    void configureRegions();
    void injectProceduralFlow(float time, float dt);
    void handleMouseFlow();
    void drawView() const;
    void drawRegion(const RegionState& region, int index) const;
    void drawRegionBorder(const tcx::flow::AverageFlow::Region& roi, const tc::Color& color) const;
    std::string settingsPath() const;
    bool saveAverageSettings() const;
    bool loadAverageSettings();
    std::string viewName() const;

    tcx::flow::Fluid2D fluid_;
    tcx::flow::OpticalFlow opticalFlow_;
    std::array<RegionState, 4> regions_;
    tc::Vec2 previousMouse_;
    ViewMode viewMode_ = ViewMode::Density;
    bool paused_ = false;
    bool showAverage_ = true;
    bool showVectors_ = false;
    bool readbackOk_ = false;
};
