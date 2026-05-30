#include "doctest.h"
#include "../core/Entity.hpp"
#include "scene/Scene.hpp"
#include "2D/component/RigidbodyComponent.hpp"
#include "core/IndiumEngine.hpp"
#include <vector>
#include <memory>

TEST_CASE("Entity basic properties")
{
    Indium::Entity entity;
    Indium::Scene scene;
    auto* rb = entity.addComponent<Indium::RigidbodyComponent>();
    CHECK(rb->mass == 1.0f);
}
