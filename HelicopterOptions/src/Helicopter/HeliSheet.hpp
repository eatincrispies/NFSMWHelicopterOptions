#pragma once
#include "Interfaces.hpp"

class AIActionHeliPursuit;

namespace HeliSheet {

    void BeginHelicopter();
    void Update(const UMath::Vector3& heliPosition, const AIActionHeliPursuit* pursuit);

}
