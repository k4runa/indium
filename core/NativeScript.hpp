#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include "Component.hpp"
#include "Entity.hpp"
#include "imgui.h"

// Fallback for icons when compiling in DLL/Script context
#ifndef ICON_FA_CODE
    #define ICON_FA_CODE ""
#endif

namespace Indium {

    enum class PropertyType { Float, Int, Bool, String, Vector2, Color };

    struct Property {
        std::string name;
        PropertyType type;
        void* data;
    };

    /**
     * @brief Base class for all C++ scripts.
     * Designed to feel like Unity's MonoBehaviour.
     */
    class NativeScript : public Component {
    public:
        // Shortcut to the owning entity with proper typing for IDE suggestions
        Entity*& entity = owner; 

        // Callback to avoid circular dependency with ScriptManager
        static std::function<Component*(const std::string&)> InstantiateCallback;

        std::vector<Property> properties;
        std::string scriptName = "NativeScript";

        NativeScript() = default;
        virtual ~NativeScript() = default;

        // --- Unity-style API Helpers ---
        
        /** @brief Gets a component of type T from the owning entity. */
        template<typename T>
        T* GetComponent() {
            if (!entity) return nullptr;
            for (auto& comp : entity->components) {
                T* casted = dynamic_cast<T*>(comp.get());
                if (casted) return casted;
            }
            return nullptr;
        }

        // --- Internal Engine Methods ---

        std::string getName() const override {
            return (scriptName == "NativeScript" || scriptName.empty()) ? "Custom Script" : scriptName;
        }
        void start() override { OnStart(); }

        std::unique_ptr<Component> clone() const override {
            Component* newComp = nullptr;
            if (InstantiateCallback && scriptName != "NativeScript" && !scriptName.empty()) {
                newComp = InstantiateCallback(scriptName);
            }
            
            if (!newComp) newComp = new NativeScript();
            
            static_cast<NativeScript*>(newComp)->scriptName = this->scriptName;
            newComp->deserialize(this->serialize());
            return std::unique_ptr<Component>(newComp);
        }

        virtual void OnStart() {}
        virtual void OnUpdate(float dt) {}
        virtual void OnDestroy() {}

        void RegisterProperty(const std::string& name, PropertyType type, void* data) {
            properties.push_back({name, type, data});
        }

        void update(float dt, Vector2 worldSize, Scene* scene) override {
            if (entity) OnUpdate(dt);
        }

        nlohmann::json serialize() const override {
            nlohmann::json j;
            j["type"] = "NativeScript";
            j["scriptName"] = scriptName;

            nlohmann::json props = nlohmann::json::object();
            for (auto const& prop : properties) {
                if (!prop.data) continue;
                if      (prop.type == PropertyType::Float)  props[prop.name] = *(float*)prop.data;
                else if (prop.type == PropertyType::Int)    props[prop.name] = *(int*)prop.data;
                else if (prop.type == PropertyType::Bool)   props[prop.name] = *(bool*)prop.data;
                else if (prop.type == PropertyType::String) props[prop.name] = *(std::string*)prop.data;
                else if (prop.type == PropertyType::Vector2) {
                    const Vector2* v = (const Vector2*)prop.data;
                    props[prop.name] = { v->x, v->y };
                }
                else if (prop.type == PropertyType::Color) {
                    const Color* c = (const Color*)prop.data;
                    props[prop.name] = { (int)c->r, (int)c->g, (int)c->b, (int)c->a };
                }
            }
            j["properties"] = props;
            return j;
        }

        void deserialize(const nlohmann::json& j) override {
            if (j.contains("scriptName")) scriptName = j["scriptName"].get<std::string>();

            if (j.contains("properties")) {
                const auto& props = j["properties"];
                for (auto& prop : properties) {
                    if (!prop.data || !props.contains(prop.name)) continue;
                    try {
                        if      (prop.type == PropertyType::Float)  *(float*)prop.data  = props[prop.name].get<float>();
                        else if (prop.type == PropertyType::Int)    *(int*)prop.data    = props[prop.name].get<int>();
                        else if (prop.type == PropertyType::Bool)   *(bool*)prop.data   = props[prop.name].get<bool>();
                        else if (prop.type == PropertyType::String) *(std::string*)prop.data = props[prop.name].get<std::string>();
                        else if (prop.type == PropertyType::Vector2) {
                            ((Vector2*)prop.data)->x = props[prop.name][0].get<float>();
                            ((Vector2*)prop.data)->y = props[prop.name][1].get<float>();
                        }
                        else if (prop.type == PropertyType::Color) {
                            Color* c = (Color*)prop.data;
                            c->r = (unsigned char)props[prop.name][0].get<int>();
                            c->g = (unsigned char)props[prop.name][1].get<int>();
                            c->b = (unsigned char)props[prop.name][2].get<int>();
                            c->a = (unsigned char)props[prop.name][3].get<int>();
                        }
                    } catch (...) {}
                }
            }
        }

        void inspect() override {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), ICON_FA_CODE "  %s", scriptName.c_str());
            ImGui::Dummy(ImVec2(0, 5));

            if (properties.empty()) {
                ImGui::TextDisabled("(No visible properties)");
            }

            for (auto& prop : properties) {
                ImGui::PushID(prop.data);
                if      (prop.type == PropertyType::Float)   ImGui::DragFloat(prop.name.c_str(), (float*)prop.data, 0.1f);
                else if (prop.type == PropertyType::Int)     ImGui::DragInt(prop.name.c_str(), (int*)prop.data);
                else if (prop.type == PropertyType::Bool)    ImGui::Checkbox(prop.name.c_str(), (bool*)prop.data);
                else if (prop.type == PropertyType::Vector2) ImGui::DragFloat2(prop.name.c_str(), (float*)prop.data, 0.1f);
                else if (prop.type == PropertyType::String) {
                    std::string& s = *(std::string*)prop.data;
                    char buf[256] = {};
                    strncpy(buf, s.c_str(), sizeof(buf) - 1);
                    if (ImGui::InputText(prop.name.c_str(), buf, sizeof(buf)))
                        s = buf;
                }
                else if (prop.type == PropertyType::Color) {
                    Color* c = (Color*)prop.data;
                    float col[4] = { c->r / 255.0f, c->g / 255.0f, c->b / 255.0f, c->a / 255.0f };
                    if (ImGui::ColorEdit4(prop.name.c_str(), col)) {
                        c->r = (unsigned char)(col[0] * 255);
                        c->g = (unsigned char)(col[1] * 255);
                        c->b = (unsigned char)(col[2] * 255);
                        c->a = (unsigned char)(col[3] * 255);
                    }
                }
                ImGui::PopID();
            }
        }
    };

    inline std::function<Component*(const std::string&)> NativeScript::InstantiateCallback = nullptr;

    template<typename T> struct PropertyTypeTrait;
    template<> struct PropertyTypeTrait<float>       { static constexpr PropertyType type = PropertyType::Float; };
    template<> struct PropertyTypeTrait<int>         { static constexpr PropertyType type = PropertyType::Int; };
    template<> struct PropertyTypeTrait<bool>        { static constexpr PropertyType type = PropertyType::Bool; };
    template<> struct PropertyTypeTrait<std::string> { static constexpr PropertyType type = PropertyType::String; };
    template<> struct PropertyTypeTrait<Vector2>     { static constexpr PropertyType type = PropertyType::Vector2; };
    template<> struct PropertyTypeTrait<Color>       { static constexpr PropertyType type = PropertyType::Color; };

    template<typename T>
    struct PropertyRegistrar {
        PropertyRegistrar(NativeScript* script, const std::string& name, T* ptr) {
            script->RegisterProperty(name, PropertyTypeTrait<T>::type, (void*)ptr);
        }
    };

    typedef Indium::Component* (*ScriptCreator)();

    inline std::map<std::string, ScriptCreator>& GetGlobalScriptRegistry() {
        static std::map<std::string, ScriptCreator> _registry;
        return _registry;
    }

    template<typename T>
    struct ScriptRegisterer {
        ScriptRegisterer(const std::string& name) {
            GetGlobalScriptRegistry()[name] = []() -> Indium::Component* { return new T(); };
        }
    };
}

// --- Macros ---

#define IND_PROP(type, name, def) \
    type name = def; \
    Indium::PropertyRegistrar<type> reg_##name{this, #name, &name}

#define REGISTER_SCRIPT(T) static Indium::ScriptRegisterer<T> reg_##T(#T);

#define INDIUM_EXPORT_SCRIPTS(...) \
    extern "C" { \
        void GetScriptNames(const char*** names, int* count) { \
            static std::vector<std::string> s_names; \
            static std::vector<const char*> s_ptrs; \
            auto& reg = Indium::GetGlobalScriptRegistry(); \
            s_names.clear(); s_ptrs.clear(); \
            for (auto const& [name, _] : reg) s_names.push_back(name); \
            for (auto const& name : s_names) s_ptrs.push_back(name.c_str()); \
            *names = s_ptrs.data(); \
            *count = (int)s_ptrs.size(); \
        } \
        Indium::Component* CreateScript(const char* name) { \
            auto& reg = Indium::GetGlobalScriptRegistry(); \
            if (reg.find(name) != reg.end()) { \
                auto* s = (Indium::NativeScript*)reg[name](); \
                s->scriptName = name; \
                return s; \
            } \
            return nullptr; \
        } \
    }
