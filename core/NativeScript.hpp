#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include "Component.hpp"
#include "Entity.hpp"
#include "scene/Scene.hpp"
#include "StoryState.hpp"
#include "SaveManager.hpp"
#include "Time.hpp"
#include "Coroutine.hpp"
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

        // Callback set by Editor to load prefab JSON, deserialize, and instantiate into scene
        static std::function<Entity*(const std::string& name, Scene*)> s_prefabLoader;

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

        // --- Scene Helpers (safe to call from OnUpdate) ---

        /** @brief Schedules this entity for destruction at the end of the frame. */
        void Destroy()
        {
            if (scene_ && entity) scene_->DestroyEntity(entity->id);
        }

        /** @brief Schedules any entity for destruction at the end of the frame. */
        void Destroy(Entity* target)
        {
            if (scene_ && target) scene_->DestroyEntity(target->id);
        }

        /**
         * @brief Spawns a new entity of type T into the scene. Safe to call from OnUpdate.
         * @param name Optional display name for the entity.
         * @return Pointer to the new entity, or nullptr outside OnUpdate.
         *
         * Example:
         *   auto* bullet = Spawn<Circle>("Bullet");
         *   bullet->position = entity->position;
         *   bullet->radius   = 8.0f;
         */
        template<typename T>
        T* Spawn(const std::string& name = "", Vector2 position = {0.0f, 0.0f}, float rotation = 0.0f)
        {
            if (!scene_) return nullptr;
            auto e = std::make_unique<T>();
            e->id       = scene_->nextEntityId++;
            e->name     = name.empty() ? e->getType() : name;
            e->position = position;
            e->rotation = rotation;
            T* ptr = e.get();
            scene_->Instantiate(std::move(e));
            return ptr;
        }

        /** @brief Finds the first entity in the scene with the given name. Returns nullptr if not found. */
        Entity* FindByName(const std::string& name) const
        {
            if (!scene_) return nullptr;
            for (const auto& e : scene_->entities)
                if (e->name == name) return e.get();
            return nullptr;
        }

        /** @brief Finds an entity by its unique ID. Returns nullptr if not found. */
        Entity* FindById(int id) const
        {
            if (!scene_) return nullptr;
            return scene_->FindEntity(id);
        }

        /** @brief Returns the active scene. Nullptr outside of OnUpdate. */
        Scene* GetScene() const { return scene_; }

        // --- Component Management ---

        /** @brief Adds a new component of type T to this entity at runtime. */
        template<typename T>
        T* AddComponent()
        {
            if (!entity) return nullptr;
            T* c = entity->addComponent<T>();
            c->start(scene_);
            return c;
        }

        // --- Scene Queries ---

        /** @brief Returns the first component of type T found in the scene. */
        template<typename T>
        T* FindObjectOfType() const
        {
            if (!scene_) return nullptr;
            for (const auto& e : scene_->entities)
                if (T* c = e->getComponent<T>()) return c;
            return nullptr;
        }

        /** @brief Returns all components of type T found in the scene. */
        template<typename T>
        std::vector<T*> FindObjectsOfType() const
        {
            std::vector<T*> result;
            if (!scene_) return result;
            for (const auto& e : scene_->entities)
                if (T* c = e->getComponent<T>()) result.push_back(c);
            return result;
        }

        /** @brief Returns the first entity that has a CameraComponent attached. */
        Entity* GetMainCamera() const
        {
            if (!scene_) return nullptr;
            for (const auto& e : scene_->entities)
                for (const auto& c : e->components)
                    if (c->getName() == "Camera Component") return e.get();
            return nullptr;
        }

        // --- Physics2D Query API ---

        using RaycastHit2D = Scene::RaycastHit2D;

        /** @brief Casts a ray and returns the closest entity hit. Safe to call from OnUpdate. */
        RaycastHit2D Raycast(Vector2 origin, Vector2 direction, float maxDist, int layerMask = -1) const
        {
            if (!scene_) return {};
            return scene_->Raycast(origin, direction, maxDist, layerMask);
        }

        /** @brief Returns all active entities whose collider overlaps a circle. */
        std::vector<Entity*> OverlapCircle(Vector2 center, float radius, int layerMask = -1) const
        {
            if (!scene_) return {};
            return scene_->OverlapCircle(center, radius, layerMask);
        }

        /** @brief Returns all active entities whose collider overlaps a box. */
        std::vector<Entity*> OverlapBox(Vector2 center, Vector2 size, int layerMask = -1) const
        {
            if (!scene_) return {};
            return scene_->OverlapBox(center, size, layerMask);
        }

        // --- Coroutines ---

        /** @brief Starts a coroutine. Use co_await WaitForSeconds{n} / WaitForFrames{n} / WaitUntil{fn} inside. */
        void StartCoroutine(CoroutineTask task)
        {
            coroutineManager_.Start(std::move(task));
        }

        /** @brief Stops and destroys all coroutines started by this script. */
        void StopAllCoroutines()
        {
            coroutineManager_.StopAll();
        }

        /** @brief Number of currently running coroutines on this script. */
        int CoroutineCount() const { return coroutineManager_.Count(); }

        // --- Prefabs ---

        /** @brief Instantiates a prefab by name (without extension) into the scene at runtime.
         *  Requires the prefab to exist in <projectPath>/prefabs/<name>.prefab.
         *  Returns a pointer to the spawned entity, or nullptr on failure. */
        Entity* InstantiatePrefab(const std::string& name)
        {
            if (!scene_ || !s_prefabLoader) return nullptr;
            return s_prefabLoader(name, scene_);
        }

        /** @brief Immediately terminates the application. */
        static void Quit() { exit(0); }

        /**
         * @brief Requests a scene transition at the end of the current frame.
         * @param sceneName File name without path or extension. Example: LoadScene("level2")
         */
        void LoadScene(const std::string& sceneName)
        {
            if (scene_) scene_->_pendingSceneLoad = sceneName;
        }

        // --- Internal Engine Methods ---

        std::string getName() const override {
            return (scriptName == "NativeScript" || scriptName.empty()) ? "Custom Script" : scriptName;
        }
        void awake(Scene* scene = nullptr) override {
            scene_ = scene;
            OnAwake();
            scene_ = nullptr;
        }
        void start(Scene* scene = nullptr) override {
            scene_ = scene;
            OnStart();
            scene_ = nullptr;
        }
        void destroy(Scene* scene = nullptr) override {
            coroutineManager_.StopAll();
            scene_ = scene;
            OnDestroy();
            scene_ = nullptr;
        }
        void draw() const override { OnDraw(); }

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

        virtual void OnAwake() {}
        virtual void OnStart() {}
        virtual void OnUpdate(float dt) {}
        virtual void OnFixedUpdate(float fixedDt) {}
        virtual void OnLateUpdate(float dt) {}
        virtual void OnDestroy() {}
        virtual void OnDraw() const {}

        // --- Collision / Trigger Callbacks ---
        /** @brief First frame two non-trigger rigidbodies begin overlapping. */
        virtual void OnCollisionEnter2D(Entity* other) {}
        /** @brief Every physics step two non-trigger rigidbodies remain overlapping. */
        virtual void OnCollisionStay2D(Entity* other)  {}
        /** @brief First frame two non-trigger rigidbodies stop overlapping. */
        virtual void OnCollisionExit2D(Entity* other)  {}
        /** @brief Another entity enters a TriggerComponent attached to this entity. */
        virtual void OnTriggerEnter2D(Entity* other)   {}
        /** @brief Another entity exits a TriggerComponent attached to this entity. */
        virtual void OnTriggerExit2D(Entity* other)    {}

        void RegisterProperty(const std::string& name, PropertyType type, void* data)
        {
            properties.push_back({name, type, data});
        }

        void update(float dt, Vector2 worldSize, Scene* scene) override
        {
            scene_ = scene;
            if (entity)
            {
                OnUpdate(dt);
                coroutineManager_.Update(dt);
            }
            scene_ = nullptr;
        }

        void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene) override
        {
            scene_ = scene;
            if (entity) OnFixedUpdate(fixedDt);
            scene_ = nullptr;
        }

        void lateUpdate(float dt, Vector2, Scene* scene) override
        {
            scene_ = scene;
            if (entity) OnLateUpdate(dt);
            scene_ = nullptr;
        }

        nlohmann::json serialize() const override
        {
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

        void deserialize(const nlohmann::json& j) override
        {
            if (j.contains("scriptName")) scriptName = j["scriptName"].get<std::string>();

            if (j.contains("properties"))
            {
                const auto& props = j["properties"];
                for (auto& prop : properties)
                {
                    if (!prop.data || !props.contains(prop.name)) continue;
                    try
                    {
                        if      (prop.type == PropertyType::Float)  *(float*)prop.data  = props[prop.name].get<float>();
                        else if (prop.type == PropertyType::Int)    *(int*)prop.data    = props[prop.name].get<int>();
                        else if (prop.type == PropertyType::Bool)   *(bool*)prop.data   = props[prop.name].get<bool>();
                        else if (prop.type == PropertyType::String) *(std::string*)prop.data = props[prop.name].get<std::string>();
                        else if (prop.type == PropertyType::Vector2)
                        {
                            ((Vector2*)prop.data)->x = props[prop.name][0].get<float>();
                            ((Vector2*)prop.data)->y = props[prop.name][1].get<float>();
                        }
                        else if (prop.type == PropertyType::Color)
                        {
                            Color* c = (Color*)prop.data;
                            c->r = (unsigned char)props[prop.name][0].get<int>();
                            c->g = (unsigned char)props[prop.name][1].get<int>();
                            c->b = (unsigned char)props[prop.name][2].get<int>();
                            c->a = (unsigned char)props[prop.name][3].get<int>();
                        }
                    }
                    catch (...) {}
                }
            }
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), ICON_FA_CODE "  %s", scriptName.c_str());
            ImGui::Dummy(ImVec2(0, 4));

            if (properties.empty())
            {
                ImGui::TextDisabled("(No visible properties)");
                return;
            }

            for (auto& prop : properties)
            {
                ImGui::PushID(prop.data);
                ImGui::TextDisabled("%s", prop.name.c_str());
                ImGui::SetNextItemWidth(-1);
                if (prop.type == PropertyType::Float)        ImGui::DragFloat("##v", (float*)prop.data, 0.1f);
                else if (prop.type == PropertyType::Int)     ImGui::DragInt("##v", (int*)prop.data);
                else if (prop.type == PropertyType::Bool)    ImGui::Checkbox("##v", (bool*)prop.data);
                else if (prop.type == PropertyType::Vector2) ImGui::DragFloat2("##v", (float*)prop.data, 0.1f);
                else if (prop.type == PropertyType::String)
                {
                    std::string& s = *(std::string*)prop.data;
                    char buf[256] = {};
                    strncpy(buf, s.c_str(), sizeof(buf) - 1);
                    if (ImGui::InputText("##v", buf, sizeof(buf))) s = buf;
                }
                else if (prop.type == PropertyType::Color)
                {
                    Color* c = (Color*)prop.data;
                    float col[4] = { c->r / 255.0f, c->g / 255.0f, c->b / 255.0f, c->a / 255.0f };
                    if (ImGui::ColorEdit4("##v", col))
                    {
                        c->r = (unsigned char)(col[0] * 255);
                        c->g = (unsigned char)(col[1] * 255);
                        c->b = (unsigned char)(col[2] * 255);
                        c->a = (unsigned char)(col[3] * 255);
                    }
                }
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::Spacing();
                ImGui::PopID();
            }
        }

    private:
        Scene*           scene_             = nullptr;
        CoroutineManager coroutineManager_;
    };

    inline std::function<Component*(const std::string&)>                 NativeScript::InstantiateCallback = nullptr;
    inline std::function<Entity*(const std::string&, Scene*)>            NativeScript::s_prefabLoader      = nullptr;

    template<typename T> struct PropertyTypeTrait;
    template<> struct PropertyTypeTrait<float>       { static constexpr PropertyType type = PropertyType::Float; };
    template<> struct PropertyTypeTrait<int>         { static constexpr PropertyType type = PropertyType::Int; };
    template<> struct PropertyTypeTrait<bool>        { static constexpr PropertyType type = PropertyType::Bool; };
    template<> struct PropertyTypeTrait<std::string> { static constexpr PropertyType type = PropertyType::String; };
    template<> struct PropertyTypeTrait<Vector2>     { static constexpr PropertyType type = PropertyType::Vector2; };
    template<> struct PropertyTypeTrait<Color>       { static constexpr PropertyType type = PropertyType::Color; };

    template<typename T>
    struct PropertyRegistrar
    {
        PropertyRegistrar(NativeScript* script, const std::string& name, T* ptr)
        {
            script->RegisterProperty(name, PropertyTypeTrait<T>::type, (void*)ptr);
        }
    };

    typedef Indium::Component* (*ScriptCreator)();

    inline std::map<std::string, ScriptCreator>& GetGlobalScriptRegistry()
    {
        static std::map<std::string, ScriptCreator> _registry;
        return _registry;
    }

    template<typename T>
    struct ScriptRegisterer
    {
        ScriptRegisterer(const std::string& name)
        {
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
