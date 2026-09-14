#pragma once

namespace AIActionHeliPursuit {

    void InstallPatches();
    void Refresh();
    bool HookConstructor();

    int  ReadMode(void* rigidBody);
    void BeginHelicopter();
    void TrackAttacks(int mode, float dt);

}
