#pragma once
#include <string>
#include <map>
#include <vector>
#include <algorithm>

namespace Indium
{
    // --------------------------------------------------------------------
    // AudioMixer — global volume buses for grouping sounds.
    //
    // Every AudioSourceComponent belongs to a named bus (default "SFX").
    // Its playback volume is multiplied by that bus's volume and the master
    // volume, so you can fade all music, duck SFX, mute UI, etc. from one
    // place — or from script:
    //
    //   AudioMixer::Get().SetBusVolume("Music", 0.3f);
    //   AudioMixer::Get().master = 0.0f;   // mute everything
    //
    // A function-local-static singleton shared across the script-DLL boundary
    // (same mechanism as StoryState / SaveManager).
    // --------------------------------------------------------------------
    struct AudioMixer
    {
        static AudioMixer& Get()
        {
            static AudioMixer instance;
            return instance;
        }

        float master = 1.0f;
        std::map<std::string, float> buses;

        AudioMixer()
        {
            buses["Music"]   = 1.0f;
            buses["SFX"]     = 1.0f;
            buses["UI"]      = 1.0f;
            buses["Ambient"] = 1.0f;
        }

        /** @brief Volume of a single bus (1.0 if the bus is unknown). */
        [[nodiscard]] float BusVolume(const std::string& bus) const
        {
            auto it = buses.find(bus);
            return (it == buses.end()) ? 1.0f : it->second;
        }

        void SetBusVolume(const std::string& bus, float v)
        {
            buses[bus] = std::clamp(v, 0.0f, 1.0f);
        }

        /** @brief Final multiplier for a source on `bus` = master * busVolume. */
        [[nodiscard]] float Effective(const std::string& bus) const
        {
            return std::clamp(master, 0.0f, 1.0f) * BusVolume(bus);
        }

        /** @brief Bus names in a stable order (for inspector combos / mixer UI). */
        [[nodiscard]] std::vector<std::string> BusNames() const
        {
            std::vector<std::string> names;
            names.reserve(buses.size());
            for (const auto& [name, _] : buses) names.push_back(name);
            return names;
        }
    };
}
