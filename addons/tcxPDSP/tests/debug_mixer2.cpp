#include <cstdio>
#include "core/AudioBuffer.h"
#include "core/AudioContext.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include "dsp/Mixer.h"
using namespace tcx::pdsp;
int main() {
    Mixer mixer;
    mixer.addInput(); mixer.addInput();
    mixer.setLevel(0, 0.5f); mixer.setPan(0, -1.0f);
    mixer.setLevel(1, 0.3f); mixer.setPan(1, 1.0f);
    AudioContext ctx; ctx.sampleRate=48000; ctx.bufferSize=64;
    mixer.prepare(ctx);
    auto& b0=mixer.inputBuffer(0); auto& b1=mixer.inputBuffer(1);
    for(int i=0;i<64;i++){b0.sample(0,i)=1.0f;b1.sample(0,i)=0.5f;}
    printf("b0 ptr=%p b1 ptr=%p out ptr=%p\n", (void*)b0.channel(0), (void*)b1.channel(0), (void*)mixer.output().channel(0));
    mixer.process(ctx, 64);
    printf("outL=%.4f outR=%.4f\n", mixer.output().sample(0,0), mixer.output().sample(1,0));
    return 0;
}
