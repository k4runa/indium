#pragma once
#include "Component.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <iostream>

namespace Indium {

    /**
     * @brief Base class for all user-defined Native C++ Scripts.
     *
     * Users should inherit from this class and implement OnUpdate().
     */
    struct NativeScript : public Component {
        virtual void OnCreate() {}
        virtual void OnUpdate(float dt) {}
        virtual void OnDestroy() {}

        void update(float dt, Vector2 worldSize, Scene* scene) override {
            OnUpdate(dt);
        }

        // Default empty inspect
        void inspect() override {
            ImGui::TextDisabled("Script Component");
        }

        // Scripts might want custom serialization, but default to Component's
        nlohmann::json serialize() const override {
            nlohmann::json j = Component::serialize();
            return j;
        }

        void deserialize(const nlohmann::json& j) override {
            // Default empty
        }
    };

    // Type alias for script instantiation function
    typedef Component* (*ScriptFactory)();

    /**
     * @brief Internal registry to hold script factories within the shared library.
     */
    struct ScriptRegistry {
        static std::unordered_map<std::string, ScriptFactory>& GetFactories() {
            static std::unordered_map<std::string, ScriptFactory> factories;
            return factories;
        }

        static void Register(const std::string& name, ScriptFactory factory) {
            GetFactories()[name] = factory;
        }
    };
}

/**
 * @brief Macro to be placed inside the body of your script class.
 *
 * Implements required Component overrides like getName() and clone().
 */
#define SCRIPT_CLASS(Type) \
    std::string getName() const override { return #Type; } \
    std::unique_ptr<Indium::Component> clone() const override { \
        return std::make_unique<Type>(*this); \
    }

/**
 * @brief Macro to register a user script with the engine.
 *
 * Place this macro at the bottom of the script's .cpp file.
 */
#define REGISTER_SCRIPT(Type) \
    namespace { \
        struct Type##_Registrar { \
            Type##_Registrar() { \
                Indium::ScriptRegistry::Register(#Type, []() -> Indium::Component* { return new Type(); }); \
            } \
        }; \
        Type##_Registrar global_##Type##_registrar; \
    }

/**
 * @brief Macro to expose the script registry to the engine.
 *
 * The ScriptManager will call this function using dlsym().
 * Users should NOT call this manually. It will be injected by the build system or
 * must be placed in exactly one file in the scripts directory.
 */
#define INDIUM_EXPORT_SCRIPTS() \
    extern "C" { \
        void GetScriptNames(const char*** outNames, int* outCount) { \
            static std::vector<const char*> names; \
            names.clear(); \
            for (const auto& pair : Indium::ScriptRegistry::GetFactories()) { \
                names.push_back(pair.first.c_str()); \
            } \
            *outNames = names.data(); \
            *outCount = names.size(); \
        } \
        \
        Indium::Component* CreateScript(const char* name) { \
            auto& factories = Indium::ScriptRegistry::GetFactories(); \
            if (factories.find(name) != factories.end()) { \
                return factories[name](); \
            } \
            return nullptr; \
        } \
    }
