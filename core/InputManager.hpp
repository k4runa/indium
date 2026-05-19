#pragma once
#include "raylib.h"
#include "../include/nlohmann/json.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>

namespace Indium
{
    struct ActionBinding
    {
        int  key      = 0;    // KeyboardKey; 0 = KEY_NULL = unset
        int  mouseBtn = -1;   // MouseButton; -1 = not a mouse binding
        bool useMouse = false;

        bool IsPressed()  const
        {
            if (useMouse && mouseBtn >= 0) return IsMouseButtonPressed((MouseButton)mouseBtn);
            return key > 0 && IsKeyPressed((KeyboardKey)key);
        }
        bool IsDown()     const
        {
            if (useMouse && mouseBtn >= 0) return IsMouseButtonDown((MouseButton)mouseBtn);
            return key > 0 && IsKeyDown((KeyboardKey)key);
        }
        bool IsReleased() const
        {
            if (useMouse && mouseBtn >= 0) return IsMouseButtonReleased((MouseButton)mouseBtn);
            return key > 0 && IsKeyReleased((KeyboardKey)key);
        }
    };

    class InputManager
    {
    public:
        static InputManager& Get() { static InputManager inst; return inst; }

        // ---- Action registration ----
        void SetAction(const std::string& name, int key)
        {
            actions_[name] = { key, -1, false };
        }
        void SetMouseAction(const std::string& name, int btn)
        {
            actions_[name] = { 0, btn, true };
        }
        void RemoveAction(const std::string& name) { actions_.erase(name); }
        bool HasAction(const std::string& name) const { return actions_.count(name) > 0; }

        // ---- Query ----
        bool IsPressed(const std::string& name) const
        {
            auto it = actions_.find(name);
            return it != actions_.end() && it->second.IsPressed();
        }
        bool IsDown(const std::string& name) const
        {
            auto it = actions_.find(name);
            return it != actions_.end() && it->second.IsDown();
        }
        bool IsReleased(const std::string& name) const
        {
            auto it = actions_.find(name);
            return it != actions_.end() && it->second.IsReleased();
        }

        // ---- Mutable access for editor ----
        std::unordered_map<std::string, ActionBinding>& GetActions() { return actions_; }
        const std::unordered_map<std::string, ActionBinding>& GetActions() const { return actions_; }

        // ---- Persistence ----
        void Load(const std::string& path)
        {
            actions_.clear();
            std::ifstream f(path);
            if (!f.is_open()) return;
            nlohmann::json j;
            try { f >> j; } catch (...) { return; }
            if (!j.contains("actions")) return;
            for (auto& [name, val] : j["actions"].items())
            {
                ActionBinding b;
                if (val.contains("key"))      b.key      = val["key"].get<int>();
                if (val.contains("mouseBtn")) b.mouseBtn = val["mouseBtn"].get<int>();
                if (val.contains("useMouse")) b.useMouse = val["useMouse"].get<bool>();
                actions_[name] = b;
            }
        }

        void Save(const std::string& path) const
        {
            nlohmann::json j;
            j["actions"] = nlohmann::json::object();
            for (auto& [name, b] : actions_)
            {
                j["actions"][name] = {
                    {"key",      b.key},
                    {"mouseBtn", b.mouseBtn},
                    {"useMouse", b.useMouse}
                };
            }
            std::ofstream f(path);
            if (f.is_open()) f << j.dump(4);
        }

        // ---- Key / button name helpers ----
        static std::string KeyName(int k)
        {
            if (k <= 0) return "(none)";
            switch ((KeyboardKey)k)
            {
                case KEY_SPACE:         return "Space";
                case KEY_ESCAPE:        return "Escape";
                case KEY_ENTER:         return "Enter";
                case KEY_TAB:           return "Tab";
                case KEY_BACKSPACE:     return "Backspace";
                case KEY_INSERT:        return "Insert";
                case KEY_DELETE:        return "Delete";
                case KEY_RIGHT:         return "Right";
                case KEY_LEFT:          return "Left";
                case KEY_DOWN:          return "Down";
                case KEY_UP:            return "Up";
                case KEY_LEFT_SHIFT:    return "Left Shift";
                case KEY_LEFT_CONTROL:  return "Left Ctrl";
                case KEY_LEFT_ALT:      return "Left Alt";
                case KEY_RIGHT_SHIFT:   return "Right Shift";
                case KEY_RIGHT_CONTROL: return "Right Ctrl";
                case KEY_RIGHT_ALT:     return "Right Alt";
                case KEY_F1:  return "F1";  case KEY_F2:  return "F2";
                case KEY_F3:  return "F3";  case KEY_F4:  return "F4";
                case KEY_F5:  return "F5";  case KEY_F6:  return "F6";
                case KEY_F7:  return "F7";  case KEY_F8:  return "F8";
                case KEY_F9:  return "F9";  case KEY_F10: return "F10";
                case KEY_F11: return "F11"; case KEY_F12: return "F12";
                case KEY_KP_0: return "KP 0"; case KEY_KP_1: return "KP 1";
                case KEY_KP_2: return "KP 2"; case KEY_KP_3: return "KP 3";
                case KEY_KP_4: return "KP 4"; case KEY_KP_5: return "KP 5";
                case KEY_KP_6: return "KP 6"; case KEY_KP_7: return "KP 7";
                case KEY_KP_8: return "KP 8"; case KEY_KP_9: return "KP 9";
                default: break;
            }
            // Letter/digit keys: their keycode equals ASCII value
            if (k >= 65 && k <= 90)  { char buf[2] = {(char)k, 0}; return buf; }
            if (k >= 48 && k <= 57)  { char buf[2] = {(char)k, 0}; return buf; }
            return "Key " + std::to_string(k);
        }

        static std::string MouseBtnName(int b)
        {
            switch (b)
            {
                case MOUSE_BUTTON_LEFT:   return "Mouse Left";
                case MOUSE_BUTTON_RIGHT:  return "Mouse Right";
                case MOUSE_BUTTON_MIDDLE: return "Mouse Middle";
                default:                  return "Mouse " + std::to_string(b);
            }
        }

        static std::string BindingName(const ActionBinding& b)
        {
            if (b.useMouse && b.mouseBtn >= 0) return MouseBtnName(b.mouseBtn);
            return KeyName(b.key);
        }

    private:
        InputManager() = default;
        std::unordered_map<std::string, ActionBinding> actions_;
    };
}
