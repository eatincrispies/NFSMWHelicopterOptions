#pragma once

namespace Interfaces {

    bool CallGetter(void* object, unsigned slot, void** result);
    bool ReadRigidBody(void* rigidBody, float position[3], float velocity[3], float** velocityPointer);
    bool ReadPlayer(float position[3], float velocity[3]);

}
