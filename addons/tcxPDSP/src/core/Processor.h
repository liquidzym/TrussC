#pragma once
// =============================================================================
// tcxPDSP Processor — Core DSP graph engine
// =============================================================================
// Manages AudioContext, output patching, and the process() pipeline.
// All registered AudioNodes are processed in order each audio block.

#include "core/AudioContext.h"
#include "core/AudioBuffer.h"
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include <vector>
#include <algorithm>

namespace tcx::pdsp {

struct ProcessorSettings {
    int sampleRate     = 48000;
    int bufferSize     = 256;
    int outputChannels = 2;
};

class Processor {
public:
    bool setup(const ProcessorSettings& settings) {
        ctx_.sampleRate     = settings.sampleRate;
        ctx_.bufferSize     = settings.bufferSize;
        ctx_.outputChannels = settings.outputChannels;
        ctx_.currentSample  = 0;

        // Create output patch nodes
        outputNodes_.clear();
        for (int i = 0; i < settings.outputChannels; i++) {
            outputNodes_.emplace_back(nullptr, i);
        }
        activeNodes_.clear();
        orderedNodes_.clear();
        graphDirty_ = true;

        // Allocate mix buffer (2 extra channels for stereo mixdown)
        mixBuffer_.allocate(settings.outputChannels, settings.bufferSize);

        return true;
    }

    // Get output patch point (use with >> to connect DSP chain)
    PatchNode& out(int channel) {
        return outputNodes_[channel];
    }

    // Register an AudioNode for processing (called automatically by nodes)
    void addNode(AudioNode* node) {
        for (auto* n : activeNodes_) {
            if (n == node) return;
        }
        activeNodes_.push_back(node);
        graphDirty_ = true;
    }

    void rebuildGraph() {
        orderedNodes_.clear();

        for (auto& outNode : outputNodes_) {
            if (auto* src = outNode.source()) {
                if (auto* owner = src->owner()) {
                    visitNode(owner);
                }
            }
        }

        if (orderedNodes_.empty()) {
            orderedNodes_ = activeNodes_;
        }

        graphDirty_ = false;
    }

    // Process one audio block → interleaved output
    void process(float* interleavedOutput, int frames, int channels) {
        if (frames <= 0 || channels <= 0) return;

        int outCh = std::min(channels, (int)outputNodes_.size());

        // Clear output
        for (int i = 0; i < frames * outCh; i++) {
            interleavedOutput[i] = 0.0f;
        }

        if (graphDirty_) {
            rebuildGraph();
        }

        // Process reachable nodes in dependency order. If no outputs are
        // connected, this falls back to registered order for compatibility.
        for (auto* node : orderedNodes_) {
            if (!node->isActive()) continue;
            node->process(ctx_, frames);
        }

        // Mix from output patch nodes → interleaved output
        for (int ch = 0; ch < outCh; ch++) {
            auto& pn = outputNodes_[ch];
            float* mixCh = mixBuffer_.channel(ch);
            for (int f = 0; f < frames; f++) mixCh[f] = 0.0f;

            if (auto* src = pn.source()) {
                if (auto* owner = src->owner()) {
                    auto& buf = owner->output();
                    if (!buf.empty()) {
                        int srcCh = std::min(src->channel(), buf.channels() - 1);
                        const float* srcData = buf.channel(srcCh);
                        int copyFrames = std::min(frames, buf.frames());
                        for (int f = 0; f < copyFrames; f++) {
                            mixCh[f] += srcData[f];
                        }
                    }
                }
            } else {
                // Backward-compatible fallback for early examples that register
                // nodes but do not connect them to processor outputs.
                for (auto* node : activeNodes_) {
                    if (!node->isActive()) continue;
                    auto& buf = node->output();
                    if (buf.empty()) continue;
                    int srcCh = std::min(ch, buf.channels() - 1);
                    const float* srcData = buf.channel(srcCh);
                    for (int f = 0; f < frames && f < buf.frames(); f++) {
                        mixCh[f] += srcData[f];
                    }
                }
            }

            for (auto* src : pn.destinations()) {
                if (!src || !src->owner()) continue;
                auto& buf = src->owner()->output();
                if (buf.empty()) continue;
                int srcCh = std::min(src->channel(), buf.channels() - 1);
                const float* srcData = buf.channel(srcCh);
                int copyFrames = std::min(frames, buf.frames());
                for (int f = 0; f < copyFrames; f++) {
                    mixCh[f] += srcData[f];
                }
            }

            // Write mix to interleaved output
            for (int f = 0; f < frames; f++) {
                float val = mixCh[f];
                if (val > 1.0f) val = 1.0f;
                if (val < -1.0f) val = -1.0f;
                interleavedOutput[f * outCh + ch] = val;
            }
        }

        ctx_.advance(frames);
    }

    AudioContext& context() { return ctx_; }

private:
    bool containsNode(const std::vector<AudioNode*>& nodes, AudioNode* target) const {
        return std::find(nodes.begin(), nodes.end(), target) != nodes.end();
    }

    bool isRegistered(AudioNode* node) const {
        return containsNode(activeNodes_, node);
    }

    void visitNode(AudioNode* node) {
        if (!node || !isRegistered(node) || containsNode(orderedNodes_, node)) return;

        for (const auto& edge : PatchNode::allConnections()) {
            if (!edge.source || !edge.destination) continue;
            AudioNode* dstOwner = edge.destination->owner();
            AudioNode* srcOwner = edge.source->owner();
            if (dstOwner == node && srcOwner && srcOwner != node) {
                visitNode(srcOwner);
            }
        }

        orderedNodes_.push_back(node);
    }

    AudioContext ctx_;
    std::vector<PatchNode> outputNodes_;
    std::vector<AudioNode*> activeNodes_;
    std::vector<AudioNode*> orderedNodes_;
    AudioBuffer mixBuffer_;
    bool graphDirty_ = true;
};

} // namespace tcx::pdsp
