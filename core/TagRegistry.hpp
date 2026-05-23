#pragma once
#include <vector>
#include <string>

namespace Indium
{
    // Project-level tag list. Populated by ProjectManager on load; persisted in
    // project.indp. "Untagged" is always the first entry and cannot be removed.
    class TagRegistry
    {
    public:
        static TagRegistry& Get() { static TagRegistry inst; return inst; }

        const std::vector<std::string>& GetTags() const { return tags_; }

        void SetTags(std::vector<std::string> tags)
        {
            if (tags.empty() || tags[0] != "Untagged") tags.insert(tags.begin(), "Untagged");
            tags_ = std::move(tags);
        }

        void Reset() { tags_ = {"Untagged", "Player", "Enemy", "Ground", "Wall", "Trigger", "UI"}; }

    private:
        TagRegistry() { Reset(); }
        std::vector<std::string> tags_;
    };
}
