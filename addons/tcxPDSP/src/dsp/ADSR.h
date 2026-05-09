#pragma once
// =============================================================================
// tcxPDSP ADSR — Attack-Decay-Sustain-Release envelope
// =============================================================================
// Sample-accurate. Call noteOn()/noteOff(), then process() each sample.

namespace tcx::pdsp {

class ADSR {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setAttack(float seconds)  { attackSec_  = seconds; }
    void setDecay(float seconds)   { decaySec_   = seconds; }
    void setSustain(float level)   { sustainLvl_ = level; }
    void setRelease(float seconds) { releaseSec_ = seconds; }

    float getAttack()  const { return attackSec_; }
    float getDecay()   const { return decaySec_; }
    float getSustain() const { return sustainLvl_; }
    float getRelease() const { return releaseSec_; }

    void noteOn() {
        if (stage_ == Stage::Attack || stage_ == Stage::Decay || stage_ == Stage::Sustain) return;
        // Restart attack from current level (prevents click)
        stage_ = Stage::Attack;
        attackStep_ = (1.0f - current_) / std::max(attackSec_ * sampleRate_, 1.0f);
    }

    void noteOff() {
        if (stage_ == Stage::Idle || stage_ == Stage::Release) return;
        stage_ = Stage::Release;
        releaseStep_ = current_ / std::max(releaseSec_ * sampleRate_, 1.0f);
    }

    float process() {
        switch (stage_) {
            case Stage::Idle:
                current_ = 0.0f;
                break;
            case Stage::Attack:
                current_ += attackStep_;
                if (current_ >= 1.0f) {
                    current_ = 1.0f;
                    stage_ = Stage::Decay;
                    decayStep_ = (1.0f - sustainLvl_) / std::max(decaySec_ * sampleRate_, 1.0f);
                }
                break;
            case Stage::Decay:
                current_ -= decayStep_;
                if (current_ <= sustainLvl_) {
                    current_ = sustainLvl_;
                    stage_ = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                current_ = sustainLvl_;
                break;
            case Stage::Release:
                current_ -= releaseStep_;
                if (current_ <= 0.0f) {
                    current_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
        }
        return current_;
    }

    bool isActive() const { return stage_ != Stage::Idle; }
    Stage getStage() const { return stage_; }

    void setSampleRate(float sr) {
        sampleRate_ = sr;
        // Recompute steps
        if (stage_ == Stage::Attack) {
            attackStep_ = (1.0f - current_) / std::max(attackSec_ * sr, 1.0f);
        }
    }

private:
    Stage stage_ = Stage::Idle;
    float sampleRate_ = 48000.0f;
    float current_ = 0.0f;

    float attackSec_  = 0.01f;
    float decaySec_   = 0.1f;
    float sustainLvl_ = 0.7f;
    float releaseSec_ = 0.2f;

    float attackStep_  = 0.0f;
    float decayStep_   = 0.0f;
    float releaseStep_ = 0.0f;
};

} // namespace tcx::pdsp
