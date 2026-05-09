// debug_physmodel.cpp — Test PhysicalModel pluck+playback
// clang++ -std=c++17 -I../src -o debug_physmodel debug_physmodel.cpp && ./debug_physmodel
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include "modules/Advanced.h"
using namespace tcx::pdsp;

int main() {
    PhysicalModel pm;
    AudioContext ctx; ctx.sampleRate=48000; ctx.bufferSize=256;
    pm.prepare(ctx);
    pm.setFrequency(220); pm.setDecay(0.9999f);
    pm.pluck(0.5f);
    
    float maxVal=0; int samplesHeard=0;
    // Simulate 3 seconds of audio with 3 plucks
    for(int pluckNum=0;pluckNum<3;pluckNum++){
        if(pluckNum>0){
            pm.pluck(0.5f);
            printf("[DEBUG] Pluck #%d at t=%.2fs\n", pluckNum+1, ctx.currentSample/48000.0f);
        }
        // Play 1 second
        for(int block=0;block<188;block++){ // 188*256≈48000 samples=1s
            pm.process(ctx, 256);
            float*s=pm.output().channel(0);
            for(int i=0;i<256;i++){
                float a=fabsf(s[i]);
                if(a>0.001f){samplesHeard++;if(a>maxVal)maxVal=a;}
            }
        }
        printf("[DEBUG] After pluck #%d: max=%.4f samples>threshold=%d\n", pluckNum+1, maxVal, samplesHeard);
        maxVal=0; samplesHeard=0;
    }
    return 0;
}
