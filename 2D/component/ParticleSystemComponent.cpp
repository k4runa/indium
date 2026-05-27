#include "ParticleSystemComponent.hpp"
#include "../../core/Entity.hpp"
#include "raymath.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace Indium
{
    void ParticleSystemComponent::update(float dt, Vector2, Scene*)
    {
        if (!owner) return;

        // Spawn new particles
        if (playing && (int)particles_.size() < maxParticles)
        {
            emitAccum_ += emissionRate * dt;
            int toSpawn = (int)emitAccum_;
            emitAccum_ -= (float)toSpawn;

            for (int i = 0; i < toSpawn && (int)particles_.size() < maxParticles; ++i) spawnOne_();
        }

        // Update existing particles
        const float G = 980.0f * gravityScale;
        for (auto& p : particles_)
        {
            p.age += dt;
            if (p.age >= p.lifetime) continue;

            p.velocity.y += G * dt;
            p.velocity    = Vector2Scale(p.velocity, 1.0f - damping * dt);
            p.position    = Vector2Add(p.position, Vector2Scale(p.velocity, dt));
        }

        // Remove dead particles
        particles_.erase(
            std::remove_if(particles_.begin(), particles_.end(),
            [](const Particle& p){ return p.age >= p.lifetime; }),
            particles_.end());

        // Auto-stop if no more particles and not looping
        if (!loop && !playing && particles_.empty()) Clear();
    }

    void ParticleSystemComponent::draw() const
    {
        for (const auto& p : particles_)
        {
            if (p.age >= p.lifetime) continue;
            float t    = p.age / p.lifetime;
            Color col  = lerpColor_(startColor, endColor, t);
            float size = startSize + (endSize - startSize) * t;
            if (size <= 0.0f) continue;
            DrawCircleV(p.position, size * 0.5f, col);
        }
    }

    void ParticleSystemComponent::inspect(std::function<void()> snapshotCb)
    {
        // Playback
        ImGui::Text("Playback");
        if (playing) { if (ImGui::Button("Stop##PS"))  Stop(); }
        else         { if (ImGui::Button("Play##PS"))  Play(); }
        ImGui::SameLine();
        if (ImGui::Button("Clear##PS")) Clear();
        ImGui::SameLine();
        ImGui::TextDisabled("%d particles", (int)particles_.size());

        ImGui::Checkbox("Loop",          &loop);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::SameLine();
        ImGui::Checkbox("Play On Start", &playOnStart);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Spacing();
        ImGui::Separator();

        // Emission
        ImGui::TextDisabled("Emission");
        ImGui::Text("Rate (particles/sec)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##PSRate", &emissionRate, 0.5f, 0.1f, 2000.0f, "%.1f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Max Particles");
        ImGui::PushItemWidth(-1);
        ImGui::DragInt("##PSMax", &maxParticles, 1.0f, 1, 5000);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        // Shape
        ImGui::Spacing();
        ImGui::TextDisabled("Emitter Shape");
        const char* shapeNames[] = { "Point", "Circle", "Rectangle" };
        int shapeIdx = static_cast<int>(shape);
        ImGui::PushItemWidth(-1);
        if (ImGui::Combo("##PSShape", &shapeIdx, shapeNames, 3))
        {
            if (snapshotCb) snapshotCb();
            shape = static_cast<EmitterShape>(shapeIdx);
        }
        ImGui::PopItemWidth();

        if (shape == EmitterShape::Circle)
        {
            ImGui::Text("Radius");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##PSShapeR", &shapeRadius, 0.5f, 0.0f, 2000.0f, "%.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
        }
        else if (shape == EmitterShape::Rectangle)
        {
            ImGui::Text("Size");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##PSShapeSz", &shapeSize.x, 0.5f, 0.0f, 2000.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
        }

        // Lifetime
        ImGui::Spacing();
        ImGui::TextDisabled("Lifetime");
        float lt[2] = {lifetimeMin, lifetimeMax};
        ImGui::PushItemWidth(-1);
        if (ImGui::DragFloat2("##PSLife", lt, 0.01f, 0.01f, 30.0f, "%.2f s"))
        {
            lifetimeMin = lt[0];
            lifetimeMax = std::max(lt[0], lt[1]);
        }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        // Velocity
        ImGui::Spacing();
        ImGui::TextDisabled("Velocity");
        ImGui::Text("Direction (deg)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##PSDeg", &directionAngle, 1.0f, -360.0f, 360.0f, "%.0f°");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Spread (±deg)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##PSSpread", &spreadAngle, 0.5f, 0.0f, 180.0f, "%.0f°");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Speed");
        float spd[2] = {speedMin, speedMax};
        ImGui::PushItemWidth(-1);
        if (ImGui::DragFloat2("##PSSpd", spd, 1.0f, 0.0f, 5000.0f, "%.0f"))
        {
            speedMin = spd[0];
            speedMax = std::max(spd[0], spd[1]);
        }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        // Physics
        ImGui::Spacing();
        ImGui::TextDisabled("Physics");
        ImGui::Text("Gravity Scale");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##PSGrav", &gravityScale, 0.01f, -5.0f, 5.0f, "%.2f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Damping");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##PSDamp", &damping, 0.01f, 0.0f, 10.0f, "%.2f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        // Appearance
        ImGui::Spacing();
        ImGui::TextDisabled("Appearance");

        ImGui::Text("Start Color");
        float sc[4] = {startColor.r/255.f, startColor.g/255.f, startColor.b/255.f, startColor.a/255.f};
        ImGui::PushItemWidth(-1);
        if (ImGui::ColorEdit4("##PSStartCol", sc))
        {
            startColor.r = (unsigned char)(sc[0]*255); startColor.g = (unsigned char)(sc[1]*255);
            startColor.b = (unsigned char)(sc[2]*255); startColor.a = (unsigned char)(sc[3]*255);
        }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("End Color");
        float ec[4] = {endColor.r/255.f, endColor.g/255.f, endColor.b/255.f, endColor.a/255.f};
        ImGui::PushItemWidth(-1);
        if (ImGui::ColorEdit4("##PSEndCol", ec))
        {
            endColor.r = (unsigned char)(ec[0]*255); endColor.g = (unsigned char)(ec[1]*255);
            endColor.b = (unsigned char)(ec[2]*255); endColor.a = (unsigned char)(ec[3]*255);
        }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Start / End Size");
        float sz[2] = {startSize, endSize};
        ImGui::PushItemWidth(-1);
        if (ImGui::DragFloat2("##PSSz", sz, 0.1f, 0.0f, 500.0f, "%.1f"))
        {
            startSize = sz[0];
            endSize   = sz[1];
        }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();
    }

    std::string ParticleSystemComponent::getName() const { return "ParticleSystem"; }

    std::unique_ptr<Component> ParticleSystemComponent::clone() const
    {
        auto c = std::make_unique<ParticleSystemComponent>();
        c->enabled        = enabled;
        c->emissionRate   = emissionRate;
        c->maxParticles   = maxParticles;
        c->loop           = loop;
        c->playOnStart    = playOnStart;
        c->shape          = shape;
        c->shapeRadius    = shapeRadius;
        c->shapeSize      = shapeSize;
        c->lifetimeMin    = lifetimeMin;
        c->lifetimeMax    = lifetimeMax;
        c->directionAngle = directionAngle;
        c->spreadAngle    = spreadAngle;
        c->speedMin       = speedMin;
        c->speedMax       = speedMax;
        c->gravityScale   = gravityScale;
        c->damping        = damping;
        c->startColor     = startColor;
        c->endColor       = endColor;
        c->startSize      = startSize;
        c->endSize        = endSize;
        return c;
    }

    nlohmann::json ParticleSystemComponent::serialize() const
    {
        nlohmann::json j = Component::serialize();
        j["emissionRate"]   = emissionRate;
        j["maxParticles"]   = maxParticles;
        j["loop"]           = loop;
        j["playOnStart"]    = playOnStart;
        j["shape"]          = static_cast<int>(shape);
        j["shapeRadius"]    = shapeRadius;
        j["shapeSize"]      = {shapeSize.x, shapeSize.y};
        j["lifetimeMin"]    = lifetimeMin;
        j["lifetimeMax"]    = lifetimeMax;
        j["directionAngle"] = directionAngle;
        j["spreadAngle"]    = spreadAngle;
        j["speedMin"]       = speedMin;
        j["speedMax"]       = speedMax;
        j["gravityScale"]   = gravityScale;
        j["damping"]        = damping;
        j["startColor"]     = {startColor.r, startColor.g, startColor.b, startColor.a};
        j["endColor"]       = {endColor.r,   endColor.g,   endColor.b,   endColor.a};
        j["startSize"]      = startSize;
        j["endSize"]        = endSize;
        return j;
    }

    void ParticleSystemComponent::deserialize(const nlohmann::json& j)
    {
        Component::deserialize(j);
        auto g = [&](const char* k, auto& v) { if (j.contains(k)) v = j[k].get<std::decay_t<decltype(v)>>(); };
        g("emissionRate",   emissionRate);
        g("maxParticles",   maxParticles);
        g("loop",           loop);
        g("playOnStart",    playOnStart);
        g("shapeRadius",    shapeRadius);
        g("lifetimeMin",    lifetimeMin);
        g("lifetimeMax",    lifetimeMax);
        g("directionAngle", directionAngle);
        g("spreadAngle",    spreadAngle);
        g("speedMin",       speedMin);
        g("speedMax",       speedMax);
        g("gravityScale",   gravityScale);
        g("damping",        damping);
        g("startSize",      startSize);
        g("endSize",        endSize);
        if (j.contains("shape")) shape = static_cast<EmitterShape>(j["shape"].get<int>());
        if (j.contains("shapeSize")) { shapeSize.x = j["shapeSize"][0]; shapeSize.y = j["shapeSize"][1]; }
        if (j.contains("startColor")) { startColor = {j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3]}; }
        if (j.contains("endColor"))   { endColor   = {j["endColor"][0],   j["endColor"][1],   j["endColor"][2],   j["endColor"][3]};   }
    }

    void ParticleSystemComponent::spawnOne_()
    {
        if (!owner) return;
        Particle p;

        // Position within emitter shape
        Vector2 base = owner->getGlobalPosition();
        if (shape == EmitterShape::Circle)
        {
            float angle = (float)(GetRandomValue(0, 3600)) * 0.1f * DEG2RAD;
            float r     = shapeRadius * sqrtf((float)GetRandomValue(0, 1000) / 1000.0f);
            base.x += cosf(angle) * r;
            base.y += sinf(angle) * r;
        }
        else if (shape == EmitterShape::Rectangle)
        {
            base.x += ((float)GetRandomValue(0, 1000) / 1000.0f - 0.5f) * shapeSize.x;
            base.y += ((float)GetRandomValue(0, 1000) / 1000.0f - 0.5f) * shapeSize.y;
        }
        p.position = base;

        // Velocity from direction cone
        float halfSpread = spreadAngle * 0.5f;
        float randDeg    = directionAngle
                         + ((float)GetRandomValue(0, 1000) / 1000.0f * 2.0f - 1.0f) * halfSpread;
        float rad   = randDeg * DEG2RAD;
        float speed = speedMin + (float)GetRandomValue(0, 1000) / 1000.0f * (speedMax - speedMin);
        p.velocity  = { cosf(rad) * speed, sinf(rad) * speed };

        // Lifetime
        float lt = lifetimeMin + (float)GetRandomValue(0, 1000) / 1000.0f * (lifetimeMax - lifetimeMin);
        p.lifetime = fmaxf(lt, 0.001f);
        p.age      = 0.0f;

        particles_.push_back(p);
    }

    Color ParticleSystemComponent::lerpColor_(Color a, Color b, float t)
    {
        return {
            (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
            (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
            (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
            (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t),
        };
    }
}
