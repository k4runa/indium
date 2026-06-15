// A real gameplay script used by CI to prove the shipped editor can compile
// scripts offline against the bundled sdk/ (engine umbrella + raylib headers),
// with a clean PATH and no Homebrew. Same surface a user's script touches.
#include "IndiumEngine.hpp"
using namespace Indium;

class VerifyProbe : public NativeScript {
public:
    void OnUpdate(float dt) override {
        if (entity && IsKeyDown(KEY_SPACE)) entity->position.x += dt;
    }
};

REGISTER_SCRIPT(VerifyProbe)
