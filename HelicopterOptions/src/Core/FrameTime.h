#pragma once

namespace FrameTime {

    void  Init();
    void  Reset();
    void  BeginFrame();

    float Step();
    float SmoothedDelta();

    void  ScaleFilterPair(float weight, float scale, float dt, float* outWeight, float* outScale);

}
