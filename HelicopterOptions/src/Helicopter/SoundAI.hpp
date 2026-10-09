#pragma once

class EAXCop;
class EAXDispatch;

class EAXAirSupport {
  public:
    void IntentToRam();
};

class SoundAI {
  public:
    static SoundAI* Get();

    EAXAirSupport* GetHeli() {
        return mHeli;
    }

    unsigned char  mActivityAndUsage[0xD8];
    EAXDispatch*   mDispatch;
    EAXCop*        mLeader;
    EAXAirSupport* mHeli;
};
