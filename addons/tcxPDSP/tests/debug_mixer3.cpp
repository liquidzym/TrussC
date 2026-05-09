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
    AudioContext ctx; ctx.sampleRate=48000; ctx.bufferSize=4;
    mixer.prepare(ctx);
    mixer.setLevel(0, 0.5f); mixer.setPan(0, -1.0f);
    mixer.setLevel(1, 0.3f); mixer.setPan(1, 1.0f);
    auto& b0=mixer.inputBuffer(0); auto& b1=mixer.inputBuffer(1);
    for(int i=0;i<4;i++){b0.sample(0,i)=1.0f;b1.sample(0,i)=0.5f;}
    printf("Pre-process: b0=%.2f,%.2f b1=%.2f,%.2f\n", b0.sample(0,0),b0.sample(0,1), b1.sample(0,0),b1.sample(0,1));
    mixer.process(ctx, 4);
    printf("Post-process: outL=%.2f,%.2f outR=%.2f,%.2f\n",
        mixer.output().sample(0,0),mixer.output().sample(0,1),
        mixer.output().sample(1,0),mixer.output().sample(1,1));
    // Manually trace
    printf("\nManual trace:\n");
    printf("  ch0 level target=%.2f ch1 level target=%.2f\n", mixer.inputBuffer(0).sample(0,0), mixer.inputBuffer(1).sample(0,0));
    return 0;
}
